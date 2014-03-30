#ifndef MySelectionController_h
#define MySelectionController_h

#include "PlatformString.h"
#include "PlatformKeyboardEvent.h"

#include <vector>

namespace WebCore {

class MySelectionController {
public:
    enum Operation {
        ZERO,
        INIT_ZERO,
        INIT_TEXT,
        SEND_EVENT,
        REPLACE_WORDS
    };
	
	MySelectionController() {
		m_currRow = 0;
		m_currCol = 0;			
		m_start = 0;
		m_end = 0;
        m_operated = ZERO;
	}

    int row() { return m_currRow; }
    int col() { return m_currCol; }
	int start() { return m_start; }
	int end() { return m_end; }

    void init(const UChar *data, int length, int start, int end);
    void init(const char *data, int length, int start, int end);

	//void sendEvent(const PlatformKeyboardEvent& event);
	void sendEvent(const PlatformKeyboardEvent& event, int start = 0, int end = 0);
    void replaceWords(int oldStart, int oldEnd, const UChar *replace, int length, int start, int end);		
    void updateRowAndCol(int start, int end);
    void dump();

private:
	std::vector<int> getLine(void);
	bool initBreakline(void);
	bool initAddWord(int keyCode);
    bool initReplaceWords(int oldStart, int oldEnd, const UChar *replace, int length, int start, int end);	
	void deinitBackspace(void);
    bool isAtZero(void);
	void clear();
	bool initMoveCursorDown(int start, int end);
    bool initMoveCursorUp(int start, int end);	
    bool initMoveCursor(int start, int end); // only call for initMoveCursorDown, initMoveCursorUp now

    void breakline(void);
    void backspace(void);

	void moveCursorUp(int start, int end);
	void moveCursorDown(int start, int end);
	void moveCursorLeft();
	void moveCursorRight();
	void focusByTab();    	
	void addWord(int keyCode);
	void replaceWord(int oldStart, int oldEnd, int replace, int start, int end);
	
	void updateSelection();

    int INDEX(int index);
	std::vector<int>& theRow(int row);
	std::vector<int>& currentRow();
	std::vector<std::vector<int> >::iterator& currentRowIt();
	std::vector<int>::iterator& currentPosIt();

    bool SAFE_ROW(int row);
    bool SAFE_CURRENT_ROW();
    bool SAFE_COL(int row, int col);
    bool SAFE_CURRENT_COL();
	
private:
	std::vector<std::vector<int> > m_keyChars;

	int m_currRow;
	int m_currCol;
	int m_start;
	int m_end;
	
    Operation m_operated;	
};

} // namespace WebCore

#endif // SelectionController_h
