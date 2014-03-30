#include "config.h"
#include "MySelectionController.h"

#include "ANPKeyCodes.h"

namespace WebCore {

void MySelectionController::init(const UChar *data, int length, int start, int end)
{
    clear();
    if (length == 0) {
        m_operated = INIT_ZERO;
        return;
    }

    m_operated = INIT_TEXT;    
    m_keyChars.push_back(getLine());
    for (int i=0; i<length; i++) {
        if (data[i] == '\n')
            m_keyChars.push_back(getLine());
        else {
            m_keyChars[INDEX(static_cast<int>(m_keyChars.size()))].push_back(data[i]);
        }        
    }

    updateRowAndCol(start, end);
    updateSelection();
}

void MySelectionController::init(const char *data, int length, int start, int end)
{
    clear();
    if (length == 0) {
        m_operated = INIT_ZERO;
        return;
    }

    m_operated = INIT_TEXT;    
    m_keyChars.push_back(getLine());
    for (int i=0; i<length; i++) {
        if (data[i] == '\n')
            m_keyChars.push_back(getLine());
        else {
            m_keyChars[INDEX(static_cast<int>(m_keyChars.size()))].push_back(data[i]);
        }        
    }

    updateRowAndCol(start, end);
    updateSelection();
}

void MySelectionController::clear()
{
    for (int i=0; i<static_cast<int>(m_keyChars.size()); i++) {
        m_keyChars[i].clear();
    }
    m_keyChars.clear();

    m_currRow = 0;
	m_currCol = 0;
	m_start = 0;
	m_end = 0;
}

void MySelectionController::sendEvent(const PlatformKeyboardEvent& event, int start, int end)
{
    if (kNewline_ANPKeyCode == event.nativeVirtualKeyCode()) {
        breakline();
    } else if (kDel_ANPKeyCode == event.nativeVirtualKeyCode()) {
        backspace();
    }else if (kDpadUp_ANPKeyCode == event.nativeVirtualKeyCode()){
        moveCursorUp(start, end);
    } else if (kDpadDown_ANPKeyCode == event.nativeVirtualKeyCode()){
        moveCursorDown(start, end);
    } else if (kDpadLeft_ANPKeyCode == event.nativeVirtualKeyCode()){
        moveCursorLeft();
    } else if (kDpadRight_ANPKeyCode == event.nativeVirtualKeyCode()){
        moveCursorRight();
    } else if (kTab_ANPKeyCode == event.nativeVirtualKeyCode()) {
        focusByTab();        
    } else {
        if (event.nativeVirtualKeyCode() > 0)
            addWord(event.nativeVirtualKeyCode());
        else
            addWord(event.unichar());
    }
    m_operated = SEND_EVENT;    
}

void MySelectionController::replaceWord(int oldStart, int oldEnd, int replace, int start, int end)
{
    updateRowAndCol(oldStart, oldEnd);
    if (!SAFE_CURRENT_COL()) return;    
    currentRow().erase(currentPosIt(), currentPosIt() + (oldEnd - oldStart));
    currentRow().insert(currentPosIt(), replace);
    
    updateRowAndCol(start, end);
    m_start = start;
    m_end = end;
}

void MySelectionController::replaceWords(int oldStart, int oldEnd, const UChar *replace, int length, int start, int end)
{
    m_operated = REPLACE_WORDS;
    if (initReplaceWords(oldStart, oldEnd, replace, length, start, end))
        return; // ??? m_operated = REPLACE_WORDS;

    if (length == 1) {
        replaceWord(oldStart, oldEnd, replace[0], start, end);
        return;
    }

    updateRowAndCol(oldEnd, oldEnd);
    if (!SAFE_CURRENT_COL()) return;    
    if (oldStart == oldEnd) {
        for (int i=0; i<length; i++) {
            currentRow().insert(currentPosIt(), replace[i]); // just insert words
            m_currCol++;
        }
    } else {
        for (int i=INDEX(start-oldEnd); i<start; i++) {
            currentRow().insert(currentPosIt(), replace[i]); // insert and skip overlapped        
            m_currCol++;
        }
    }

    updateRowAndCol(start, end);
    m_start = start;
    m_end = end;
}

bool MySelectionController::initReplaceWords(int oldStart, int oldEnd, const UChar *replace, int length, int start, int end)
{
	if (m_currRow == 0 && m_currCol == 0) {
        init(replace, length, start, end);
        return true;
    }
    return false;
}

std::vector<int> MySelectionController::getLine(void)
{
	std::vector<int> words;
	return words;
}

void MySelectionController::breakline()
{
	if (initBreakline())
		return;

    if (!SAFE_CURRENT_ROW()) return;
	m_keyChars.insert(currentRowIt(), getLine());
	int oldRow = m_currRow;
	int oldCol = m_currCol;
	
	m_currRow++;
	m_currCol = 0;

    if (!SAFE_ROW(oldRow)) return;
    if (!SAFE_CURRENT_ROW()) return;
    if (!SAFE_COL(oldRow, oldCol)) return;
   	std::vector<int>::iterator first = theRow(oldRow).begin() + oldCol;
    std::vector<int>::iterator last = theRow(oldRow).end();      
    while (first != last) {
        currentRow().push_back(*first);
        first++;
    }

    first = m_keyChars[INDEX(oldRow)].begin() + oldCol;
	last = m_keyChars[INDEX(oldRow)].end();
    m_keyChars[INDEX(oldRow)].erase(first, last);
    
	updateSelection();
}

bool MySelectionController::initBreakline()
{
	if (m_currRow == 0 && m_currCol == 0) {
        m_keyChars.push_back(getLine());
        m_keyChars.push_back(getLine());
        //m_keyChars.insert(m_keyChars.begin(), getLine());
        //m_keyChars.insert(m_keyChars.begin(), getLine());
        m_currRow = 2;
        m_currCol = 0;
        updateSelection();
		return true;
	}
	return false;
}

void MySelectionController::backspace(void)
{
	if (m_currCol == 0) {
		if (m_currRow == 0)
			return;

    	int oldRow = m_currRow;
    	int oldCol = m_currCol;

        m_currRow--;
        if (!SAFE_CURRENT_ROW()) return;
		m_currCol = static_cast<int>(m_keyChars[INDEX(m_currRow)].size());

        if (!SAFE_ROW(oldRow)) return;
        if (!SAFE_CURRENT_ROW()) return;
    	std::vector<int>::iterator first = theRow(oldRow).begin();
	    std::vector<int>::iterator last = theRow(oldRow).end();      
        while (first != last) {
            currentRow().push_back(*first);
            first++;
        }
        theRow(oldRow).clear();
        m_keyChars.erase(m_keyChars.begin() + (oldRow - 1));
    } else {
        int newCol = m_currCol-1;
        if (!SAFE_CURRENT_ROW()) return;
        currentRow().erase(currentRow().begin() + newCol);
        m_currCol--;

        deinitBackspace();
	}
	
    updateSelection();
}

void MySelectionController::deinitBackspace()
{
    // only de-init when last glyph in line.1(if there other line or other glyph, we should not deinit it)
    int rowSize = static_cast<int>(m_keyChars.size());
    if (rowSize == 1/*fix testMoves()*/ && m_keyChars[INDEX(rowSize)].size() == 0 && m_currRow == 1 && m_currCol == 0) {
        m_currRow = 0;
        m_keyChars.erase(currentRowIt());
    }
}

bool MySelectionController::isAtZero()
{
    if (m_currRow == 0 && m_currCol == 0)
        return true;
    return false;
}

void MySelectionController::moveCursorUp(int start, int end)
{
    if (isAtZero())
        return;

    if (initMoveCursorUp(start, end))
        return;
    
    if (m_currRow == 1)
        m_currCol = 0; // (up) will move (first col) if in (first row)

    if (m_currRow <= 1) {
        m_currRow = 0;
        m_currCol = 0;
    } else {
        m_currRow--;    
        m_currCol = start;
    }

    updateSelection();
}

bool MySelectionController::initMoveCursorUp(int start, int end)
{
    return initMoveCursor(start, end);
}

void MySelectionController::moveCursorDown(int start, int end)
{
    if (initMoveCursorDown(start, end))
        return;

    if (m_currRow == static_cast<int>(m_keyChars.size())) {
        if (!SAFE_CURRENT_ROW()) return;
        m_currCol = static_cast<int>(currentRow().size());
        //m_currCol = static_cast<int>(m_keyChars[m_currRow - 1].size());
    } else {
        m_currRow++;
        m_currCol = start;
    }

    updateSelection();
}

bool MySelectionController::initMoveCursorDown(int start, int end)
{
    return initMoveCursor(start, end);
}

bool MySelectionController::initMoveCursor(int start, int end)
{
    if (start == 0 && end == 0 && (m_operated == INIT_ZERO || m_operated == INIT_TEXT)) {
        if (m_operated == INIT_ZERO)
            m_currRow = 0;
        else if (m_operated == INIT_TEXT) // init row for INIT_TEXT
            m_currRow = 1;
        m_currCol = 0;
        updateSelection();
        return true;
    }
    return false;
}

void MySelectionController::moveCursorLeft()
{
    // TODO: if (atZero()) return;
	if (m_currCol == 0) {
		if (m_currRow == 0)
			return;

        if (!SAFE_CURRENT_ROW()) return;
        m_currCol = static_cast<int>(m_keyChars[INDEX(m_currRow)].size());
        m_currRow--;
    } else {
        m_currCol--;
    }

    updateSelection();
}

void MySelectionController::moveCursorRight()
{
    if (m_currRow == 0 && m_currCol == 0)
        return;

    if (!SAFE_CURRENT_ROW()) return;
	if (m_currCol == static_cast<int>(m_keyChars[INDEX(m_currRow)].size())) {
        if (m_currRow == static_cast<int>(m_keyChars.size()))
			return;

        m_currRow++;
        m_currCol = 0;
    } else {
        m_currCol++;
    }

    updateSelection();
}

void MySelectionController::focusByTab()
{
    initMoveCursor(0, 0);
}

bool MySelectionController::initAddWord(int keyCode)
{
	if (m_currRow == 0 && m_currCol == 0) {
        m_keyChars.insert(m_keyChars.begin(), getLine());
		m_keyChars[0].push_back(keyCode);		
		m_currRow = 1;
        m_currCol = 1;
		updateSelection();
		return true;
	}
	return false;
}

void MySelectionController::addWord(int keyCode)
{    
	if (initAddWord(keyCode))
		return;

    if (!SAFE_CURRENT_COL()) return;
	m_keyChars[INDEX(m_currRow)].insert(currentPosIt(), keyCode);
	m_currCol++;

    updateSelection();
}

int MySelectionController::INDEX(int index)
{
    if (index > 0)
        return index - 1;
    return 0;
}

std::vector<int>& MySelectionController::theRow(int row)
{
    return m_keyChars[INDEX(row)];
}

std::vector<int>& MySelectionController::currentRow()
{
    return theRow(m_currRow);
}

std::vector<std::vector<int> >::iterator& MySelectionController::currentRowIt() 
{	
	static std::vector<std::vector<int> >::iterator it;
	it = m_keyChars.begin() + m_currRow;
	return it;
}      

std::vector<int>::iterator& MySelectionController::currentPosIt() 
{
	static std::vector<int>::iterator it;
	it = m_keyChars[INDEX(m_currRow)].begin() + m_currCol;
	return it;
}      

void MySelectionController::updateSelection() 
{
	m_start = 0;
	m_end = 0;
	
	for (int i=0; i<m_currRow-1; i++)
		m_start += static_cast<int>(m_keyChars[i].size() + 1);
    m_start += m_currCol;
	m_end = m_start;    
}

void MySelectionController::updateRowAndCol(int start, int end)
{
    m_currRow = 0;
    m_currCol = 0;

    for (int i=0; i<static_cast<int>(m_keyChars.size()); i++) {
        if (start <= static_cast<int>(m_keyChars[i].size())) {
            m_currRow = i + 1;
            m_currCol = start;
            break;
        }
        start -= static_cast<int>(m_keyChars[i].size() + 1);
    }
}

bool MySelectionController::SAFE_ROW(int row)
{
    if (m_keyChars.empty())
        return false;

    if (row >= 0 && row <= static_cast<int>(m_keyChars.size()))
        return true;
    return false;
}

bool MySelectionController::SAFE_CURRENT_ROW()
{
    return SAFE_ROW(m_currRow);
}

bool MySelectionController::SAFE_COL(int row, int col)
{
    if (SAFE_ROW(row) && col >= 0 && col <= static_cast<int>(m_keyChars[INDEX(row)].size()))
        return true;
    return false;
}

bool MySelectionController::SAFE_CURRENT_COL()
{
    return SAFE_COL(m_currRow, m_currCol);
}

void MySelectionController::dump()
{
    for (int i=0; i<static_cast<int>(m_keyChars.size()); i++) {
        for (int j=0; j<static_cast<int>(m_keyChars[i].size()); j++) {
            printf("[%d],", m_keyChars[i][j]);
        }
        printf("\n");
    }
    printf("m_currRow=%d, m_currCol=%d, m_start=%d, m_end=%d\n", m_currRow, m_currCol, m_start, m_end);
}

}
