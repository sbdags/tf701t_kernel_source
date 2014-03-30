/*
 * FrameLoaderNavigatorTest.cpp
 *
 *  Created on: 2013/4/19
 *      Author: George1_Huang
 */

#include "FrameLoaderNavigator.h"
#include "FrameLoaderNavigatorTest.h"
#include <iostream>
#include <set>

using namespace std;


static void assert(bool result, const bool expect, const char *err_msg, const int line) {
    if (result == expect) {
        std::cout << ".";
    } else {
        std::cout << "(" << err_msg << "):" << line << ", x";
		FrameNavigation::getInstance().dump();
    }
}

#define ASSERT(result, expect) assert(result, expect, __func__, __LINE__)

static void testWwwWikipediaOrg() {
    FrameNavigation::getInstance().navi(1627109848, 1707020296, true, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1627109848, 1707020296, true, FrameNavigation::FinishDocumentLoad);

    ASSERT(FrameNavigation::getInstance().isFinish(1627109848, 1707020296), true);
    FrameNavigation::getInstance().navi(1627109848, 1707020296, true, FrameNavigation::FinishLoad);
}

static void testWwwAsusCom() {
    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1475743288, 1554837232, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1475743288, 1554837232, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1554837232), false);
    FrameNavigation::getInstance().navi(1475743288, 1554837232, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1559967344), false);
    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1554483840, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1475743288, 1554483840, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1554483840), false);

    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1073985296), true);

    FrameNavigation::getInstance().navi(1475743288, 1554483840, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1559967344), false);
    FrameNavigation::getInstance().navi(1475743288, 1559967344, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::FinishLoad);
}

static void testWwwYahooCom() {
    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1475743288, 1655012240, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1073985296), false);

    FrameNavigation::getInstance().navi(1475743288, 1655012240, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1655012240), true);
    FrameNavigation::getInstance().navi(1475743288, 1655012240, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1651059784, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1475743288, 1651059784, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1475743288, 1651059784), false);
    FrameNavigation::getInstance().navi(1475743288, 1651059784, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1475743288, 1073985296, true, FrameNavigation::FinishLoad);
}

static void testWwwGvmCom() {
    FrameNavigation::getInstance().navi(1650611968, 1664771520, true, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1693258744, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1693258744, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1669552136, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1669552136, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1664610232, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1664610232, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1664610232, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1664610232), false);
    FrameNavigation::getInstance().navi(1650611968, 1664610232, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1700971568, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1700971568, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696393488, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696393488, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1701360992, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1701360992, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1680184592, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1680184592, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1664771520, true, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1664771520), false);

    FrameNavigation::getInstance().navi(1650611968, 1700971568, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1700971568), false);
    FrameNavigation::getInstance().navi(1650611968, 1700971568, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696176912), false);
    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696184584), false);
    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696176912), false);
    FrameNavigation::getInstance().navi(1650611968, 1696176912, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696184584), false);
    FrameNavigation::getInstance().navi(1650611968, 1696184584, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1675849752, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1675849752, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1693258744, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1693258744), false);
    FrameNavigation::getInstance().navi(1650611968, 1693258744, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1701360992, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1701360992), false);

    FrameNavigation::getInstance().navi(1650611968, 1680184592, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1680184592), false);

    FrameNavigation::getInstance().navi(1650611968, 1667560336, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1667560336, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1658969112, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1658969112, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1682643136, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1682643136, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1664610232, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1664610232), false);

    FrameNavigation::getInstance().navi(1650611968, 1669552136, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1669552136), false);
    FrameNavigation::getInstance().navi(1650611968, 1669552136, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1682643136, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1682643136), false);

    FrameNavigation::getInstance().navi(1650611968, 1692928096, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1692928096, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1692928096, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1692928096), false);
    FrameNavigation::getInstance().navi(1650611968, 1692928096, false, FrameNavigation::FinishLoad);
    FrameNavigation::getInstance().navi(1650611968, 1692928096, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1692928096), false);

    FrameNavigation::getInstance().navi(1650611968, 1682643136, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1658969112, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1658969112), false);
    FrameNavigation::getInstance().navi(1650611968, 1667560336, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1667560336), false);
    FrameNavigation::getInstance().navi(1650611968, 1696393488, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696393488), false);

    FrameNavigation::getInstance().navi(1650611968, 1701360992, false, FrameNavigation::FinishLoad);
    FrameNavigation::getInstance().navi(1650611968, 1680184592, false, FrameNavigation::FinishLoad);
    FrameNavigation::getInstance().navi(1650611968, 1658969112, false, FrameNavigation::FinishLoad);
    FrameNavigation::getInstance().navi(1650611968, 1667560336, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1675849752, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1675849752), true);
    FrameNavigation::getInstance().navi(1650611968, 1675849752, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1678355880, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1678355880, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1711018192), false);
    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1696393488, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1689627848, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1689627848, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1689165664, false, FrameNavigation::Provision);
    FrameNavigation::getInstance().navi(1650611968, 1689165664, false, FrameNavigation::Provision);

    FrameNavigation::getInstance().navi(1650611968, 1689627848, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1696184584), false);
    FrameNavigation::getInstance().navi(1650611968, 1689627848, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1689165664, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1689165664), false);
    FrameNavigation::getInstance().navi(1650611968, 1689165664, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1678355880, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1678355880), false);
    FrameNavigation::getInstance().navi(1650611968, 1678355880, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::FinishDocumentLoad);
    ASSERT(FrameNavigation::getInstance().isFinish(1650611968, 1711018192), false);
    FrameNavigation::getInstance().navi(1650611968, 1711018192, false, FrameNavigation::FinishLoad);

    FrameNavigation::getInstance().navi(1650611968, 1664771520, true, FrameNavigation::FinishLoad);
}

void testFrameLoaderNavigator() {
    testWwwWikipediaOrg();
    testWwwAsusCom();
    testWwwYahooCom();
    testWwwGvmCom();
}

