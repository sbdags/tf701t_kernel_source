/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LRUCache_h
#define LRUCache_h

#include "DoublyLinkedList.h"
#include "HashMap.h"

namespace WTF {

template <typename Key, typename Node, size_t Capacity>
class LRUCache {
public:
    size_t size() const;

    Node* operator[](const Key&);

    ~LRUCache();

    typedef Node* iterator;
    iterator begin();
    iterator end();

private:
    HashMap<Key, Node*> m_hashMap;
    DoublyLinkedList<Node> m_history;
};

template <typename Key, typename Node, size_t Capacity>
inline size_t LRUCache<Key, Node, Capacity>::size() const
{
    return m_hashMap.size();
}

template <typename Key, typename Node, size_t Capacity>
inline Node* LRUCache<Key, Node, Capacity>::operator[](const Key& key)
{
    typename HashMap<Key, Node*>::iterator result = m_hashMap.find(key);
    if (result != m_hashMap.end()) {
        Node* node = result->second;
        if (node->next()) {
            m_history.remove(node);
            m_history.append(node);
        }
        return node;
    }

    if (size() == Capacity) {
        Node* evictNode = m_history.head();
        m_hashMap.remove(evictNode->key());
        m_history.remove(evictNode);
        delete evictNode;
    }

    Node* node = new Node(key);
    m_hashMap.set(key, node);
    m_history.append(node);
    return node;
}

template <typename Key, typename Node, size_t Capacity>
inline LRUCache<Key, Node, Capacity>::~LRUCache()
{
    for (typename HashMap<Key, Node*>::iterator item = m_hashMap.begin(); item != m_hashMap.end(); ++item)
        delete item->second;
}

template <typename Key, typename Node, size_t Capacity>
inline typename LRUCache<Key, Node, Capacity>::iterator LRUCache<Key, Node, Capacity>::begin()
{
    return m_history.head();
}

template <typename Key, typename Node, size_t Capacity>
inline typename LRUCache<Key, Node, Capacity>::iterator LRUCache<Key, Node, Capacity>::end()
{
    return 0;
}

} // namespace WTF

using WTF::LRUCache;

#endif
