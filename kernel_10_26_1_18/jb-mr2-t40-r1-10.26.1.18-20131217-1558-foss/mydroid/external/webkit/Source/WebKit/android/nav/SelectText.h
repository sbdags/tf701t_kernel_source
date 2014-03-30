/*
 * Copyright 2008, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef SelectText_h
#define SelectText_h

#include "DrawExtra.h"
#include "IntRect.h"
#include "PlatformString.h"

namespace android {

class SelectText : public RegionLayerDrawExtra {
public:
    enum HandleId {
        BaseHandle = 0,
        ExtentHandle = 1,
    };
    enum HandleType {
        LeftHandle = 0,
        CenterHandle = 1,
        RightHandle = 2,
    };

    IntRect& caretRect(HandleId id) { return m_caretRects[id]; }
    void setCaretRect(HandleId id, const IntRect& rect) { m_caretRects[id] = rect; }
    IntRect& textRect(HandleId id) { return m_textRects[id]; }
    void setTextRect(HandleId id, const IntRect& rect) { m_textRects[id] = rect; }
    //Start [Instant Dictionary]
    int selectionTextRectCount(){return m_selectionTextRect.size();}
    const IntRect selectionTextRect(int id) { return m_selectionTextRect[id];}
    void setSelectionTextRect(const WTF::Vector<IntRect>& rects,bool bAppend)
    {
        //if bAppend is false,  replace the rects arry, or append the new rects to the older one.
        if(!bAppend)
        {
            m_selectionTextRect = rects;
        }
        else
        {
            if(rects.size()>0)
            {
                for(unsigned int i=0;i<rects.size();i++)
                {
                    m_selectionTextRect.append(rects[i]);
                }
            }
        }
    }
    //End [Instant Dictionary]
    int caretLayerId(HandleId id) { return m_caretLayerId[id]; }
    void setCaretLayerId(HandleId id, int layerId) { m_caretLayerId[id] = layerId; }

    void setText(const String& text) { m_text = text.threadsafeCopy(); }
    String& getText() { return m_text; }
    HandleType getHandleType(HandleId id) { return m_handleType[id]; }
    void setHandleType(HandleId id, HandleType type) { m_handleType[id] = type; }

private:
    IntRect m_caretRects[2];
    IntRect m_textRects[2];
    //Start [Instant Dictionary]
    WTF::Vector<IntRect> m_selectionTextRect;
    //End [Instant Dictionary]
    int m_caretLayerId[2];
    HandleType m_handleType[2];
    String m_text;
};

}

namespace WebCore {

void ReverseBidi(UChar* chars, int len);

}

#endif
