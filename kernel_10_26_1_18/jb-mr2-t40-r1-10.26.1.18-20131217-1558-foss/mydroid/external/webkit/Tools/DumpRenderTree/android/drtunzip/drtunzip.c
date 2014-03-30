/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zipfile/zipfile.h>

static int mkpath(char *path)
{
    struct stat st;
    char *directoryNameStart;
    char *directoryNameEnd;
    int isDirectoryMissing;

    directoryNameStart = path;
    while ((directoryNameEnd = strchr(directoryNameStart, '/'))) {
        if (directoryNameEnd != directoryNameStart) {
            *directoryNameEnd = '\0';
            mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        }
        directoryNameStart = directoryNameEnd + 1;
        isDirectoryMissing = stat(path, &st) || !S_ISDIR(st.st_mode);
        *directoryNameEnd = '/';
        if (isDirectoryMissing)
            return -1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    int f;
    void* zipBuffer;
    off_t zipBufferSize;
    size_t contentsSizeForDecompression;
    size_t contentsSize;
    void* contents;
    zipfile_t zip;
    zipentry_t entry;
    void* cookie;
    char* entryName;
    int err;

    if (argc != 2) {
        fprintf(stderr, "usage: drtunzip ZIPFILE\n"
                "\textracts ZIPFILE to current directory\n");
        return 1;
    }

    f = open(argv[1], O_RDONLY);
    if (f < 0) {
        fprintf(stderr, "couldn't open %s\n", argv[1]);
        return 1;
    }

    zipBufferSize = lseek(f, 0, SEEK_END);

    if (!zipBufferSize) {
        close(f);
        return 0;
    }

    if (zipBufferSize < 0) {
        fprintf(stderr, "couldn't open %s\n", argv[1]);
        return 1;
    }

    lseek(f, 0, SEEK_SET);

    zipBuffer = mmap(NULL, zipBufferSize, PROT_READ, MAP_PRIVATE, f, 0);

    if (zipBuffer == MAP_FAILED) {
        fprintf(stderr, "error: mmap for zip file read failed\n");
        return 1;
    }

    zip = init_zipfile(zipBuffer, zipBufferSize);
    if (!zip) {
        fprintf(stderr, "error: init_zipfile failed\n");
        return 1;
    }

    close(f);

    cookie = NULL;

    while ((entry = iterate_zipfile(zip, &cookie))) {
        entryName = get_zipentry_name(entry);
        if (entryName[strlen(entryName) - 1] == '/') {
            free(entryName);
            continue;
        }

        mkpath(entryName);
        f = open(entryName, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
        if (f < 0) {
            fprintf(stderr, "error: can not open file for writing '%s'\n", entryName);
            return 1;
        }

        free(entryName);

        contentsSize = get_zipentry_size(entry);
        if (contentsSize) {
            contentsSizeForDecompression = contentsSize * 1.001;
            ftruncate(f, contentsSizeForDecompression);
            contents = mmap(NULL, contentsSizeForDecompression, PROT_WRITE | PROT_READ, MAP_SHARED, f, 0);
            if (contents == MAP_FAILED) {
                fprintf(stderr, "error: mmap for write failedq \n");
                return 1;
            }
            err = decompress_zipentry(entry, contents, contentsSizeForDecompression);
            if (err) {
                fprintf(stderr, "error: can not decompress file\n");
                return 1;
            }
            munmap(contents, contentsSizeForDecompression);
        }
        ftruncate(f, contentsSize);
        close(f);
    }

    release_zipfile(zip);
    munmap(zipBuffer, zipBufferSize);

    return 0;
}
