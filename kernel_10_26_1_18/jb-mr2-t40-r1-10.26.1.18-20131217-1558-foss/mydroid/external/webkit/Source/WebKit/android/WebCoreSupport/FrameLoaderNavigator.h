#ifndef FRAMELOADER_NAVIGATION_H_
#define FRAMELOADER_NAVIGATION_H_

#include <set>
#include <map>


typedef int PageID_t;
typedef int FrameID_t;
struct Frame_t {
    enum State {
        Provision = 1,
        FinishLoad
    };

    Frame_t(const int id) {
        id_ = id;
        isMainFrame_ = true;
        state_ = FinishLoad;
    }

    Frame_t(const int id, const bool isMainFrame, const State state) {
        id_ = id;
        isMainFrame_ = isMainFrame;
        state_ = state;
    }

    bool operator<(const Frame_t& o) const {
        return id_ < o.id_;
    }

    bool operator==(const Frame_t& o) const {
        if (o == *this)
            return true;

        if (o.id_ == id_)
            return true;

        return false;
    }

    FrameID_t id_;
    bool isMainFrame_;
    State state_;
};
typedef std::set<Frame_t> FrameSet_t;

struct Page_t {
    enum State {
        Provision = 1,
        FinishDocumentLoadByMainFrame,
        FinishDocumentLoadBySubFrame
    };

	Page_t(const int id) {
        id_ = id;
        state_ = Provision;
    }

    Page_t(const int id, const State state) {
		id_ = id;
        state_ = state;
	}

	PageID_t id_;
	State state_;
};

class sortPage {
public:
    bool operator() (const Page_t& rhs, const Page_t& lhs) const {
        if (rhs.id_ < lhs.id_)
            return true;
        else 
            return false;
    }
};

typedef std::map<Page_t, FrameSet_t, sortPage> NaviPageFrames_t;


class FrameNavigation {
public:
    enum Type {
        MainFrame = 1,
        AllFrames,
        MainFrameResources,
        AllFramesResources        
    };

    enum State {
        Provision = 1,
        FirstLayout,
        FinishDocumentLoad,
        FailLoad,
        FinishLoad
    };

    static FrameNavigation& getInstance() {
        static FrameNavigation s_Instance;
        return s_Instance;
    }

    void navi(const PageID_t page, const FrameID_t frame, const bool isMainFrame, const State changeTo);
    bool isFinish(const PageID_t page, const FrameID_t frame);
    void dump();

private:
    FrameNavigation() {}

    void create(const PageID_t page, const FrameID_t frame, const bool isMainFrame, const State changeTo);
    void remove(const PageID_t page, const FrameID_t frame, const State changeTo);
    void markup(const PageID_t page, const Page_t::State state);
    void markup(const PageID_t page, const FrameID_t frame); // only for main frame
    void remove(const PageID_t page);

    NaviPageFrames_t m_PageFrames;
};

#endif
