#include "FrameLoaderNavigator.h"
#include <iostream>


void FrameNavigation::navi(const PageID_t page, const FrameID_t frame, const bool isMainFrame, const State changeTo)
{
    if (changeTo == Provision) {
        if (isMainFrame)
            remove(page);

        create(page, frame, isMainFrame, changeTo);
    
	} else if (changeTo == FinishDocumentLoad) {
        if (isMainFrame) {
            markup(page, frame);
            return;
        }

        remove(page, frame, changeTo);
    
	} else if (changeTo == FinishLoad || changeTo == FailLoad) {
        if (isMainFrame)
            remove(page);
    } else {
        // unknown state
    }
}

bool FrameNavigation::isFinish(const PageID_t page, const FrameID_t frame)
{
	Page_t pg(page);
    NaviPageFrames_t::iterator it = m_PageFrames.find(pg);

	if (it == m_PageFrames.end())
        return true;

	if (it->second.size() == 1) {
		FrameSet_t::iterator it2 = it->second.find(Frame_t(frame));

		if (it2 != it->second.end()) { // main-frame finish load finally
			if (it2->isMainFrame_ == true && it2->state_ == Frame_t::FinishLoad) {
				markup(page, Page_t::FinishDocumentLoadByMainFrame);
				return true;
			} else {
				return false;
			}
		} else {
			it2 = it->second.begin();
			
			if (it->first.state_ == Page_t::Provision &&
				it2->isMainFrame_ == true && it2->state_ == Frame_t::FinishLoad) {
				
				markup(page, Page_t::FinishDocumentLoadBySubFrame);
				return true;
			} else {
				return false;
			}
		}
	}

    return false;
}

void FrameNavigation::create(const PageID_t page, const FrameID_t frame, const bool isMainFrame, const State changeTo)
{
	Page_t pg(page);
    NaviPageFrames_t::iterator it = m_PageFrames.find(pg);
    Frame_t f(frame, isMainFrame, Frame_t::Provision);

    if (it == m_PageFrames.end()) {
		FrameSet_t frameSet;
        frameSet.insert(f);

		m_PageFrames.insert(std::pair<Page_t, FrameSet_t >(pg, frameSet));
	} else {
        it->second.insert(f);
    }
}

void FrameNavigation::remove(const PageID_t page, const FrameID_t frame, const State changeTo)
{
	Page_t pg(page);
    NaviPageFrames_t::iterator it = m_PageFrames.find(pg);

    if (it != m_PageFrames.end()) {
		Frame_t f(frame);

        it->second.erase(f);
        if (it->second.size() == 0) {
            m_PageFrames.erase(page);

        }
    }
}

void FrameNavigation::markup(const PageID_t page, const FrameID_t frame)
{
	Page_t pg(page);
    NaviPageFrames_t::iterator it = m_PageFrames.find(pg);

	if (it != m_PageFrames.end()) {
		Frame_t f(frame);
    
		it->second.erase(f);
        it->second.insert(Frame_t(frame, true, Frame_t::FinishLoad));
    }
}

void FrameNavigation::markup(const PageID_t page, const Page_t::State state)
{
	Page_t oldPage(page);
    NaviPageFrames_t::iterator it = m_PageFrames.find(oldPage);
	
	FrameSet_t frameSet = it->second;

	m_PageFrames.erase(it);

	Page_t newPage(page, state);
	m_PageFrames.insert(std::pair<Page_t, FrameSet_t >(newPage, frameSet));
}

void FrameNavigation::remove(const PageID_t page)
{
	Page_t pg(page);
    m_PageFrames.erase(pg);
}

void FrameNavigation::dump() {
    NaviPageFrames_t::iterator pagePos = m_PageFrames.begin();
    NaviPageFrames_t::iterator pageEnd = m_PageFrames.end();

    while (pagePos != pageEnd) {
        FrameSet_t::iterator framePos = pagePos->second.begin();
        FrameSet_t::iterator frameEnd = pagePos->second.end();
		std::cout << "page:" << pagePos->first.id_ << std::endl;

        while (framePos != frameEnd) {
            std::cout << "frame:" << framePos->id_ << ", ";
            framePos++;
        }

        std::cout << std::endl;
        pagePos++;
    }
}

