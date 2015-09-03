#pragma once
#include "UISDK\Kernel\Src\Base\Message\message.h"
#include "UISDK\Kernel\Src\Base\Attribute\attribute.h"
#include "UISDK\Kernel\Inc\Interface\iscrollbarmanager.h"
#include "smoothscroll.h"


namespace UI
{
#define UI_MSGMAP_ID_INERTIA_MOUSE_CAPTURE     138191528
#define UI_MSGMAP_ID_INERTIA_KEYBOARD_CAPTURE  138191529

enum SETSCROLLINFO_RET
{
    SETSCROLLINFO_RET_ERROR,  // -1, 出错
    SETSCROLLINFO_RET_OK,     // 不需要再处理
    SETSCROLLINFO_RET_NEED_UPDATA_VISIBLE  // 需要通知滚动条数据进行刷新
};

class ScrollBarManager : public Message, public ISmoothScrollCallback
{
public:
    ScrollBarManager();
    ~ScrollBarManager();

    UI_BEGIN_MSG_MAP
        UIMSG_WM_GETSCROLLOFFSET(OnGetScrollOffset)
        UIMSG_WM_GETSCROLLRANGE(OnGetScrollRange)
        UIMSG_WM_HSCROLL(OnHScroll)
        UIMSG_WM_VSCROLL(OnVScroll)
        UIMSG_WM_MOUSEWHEEL(OnMouseWheel)
        UIMSG_WM_STATECHANGED2(OnStateChanged)
        UIMESSAGE_HANDLER_EX(UI_WM_GESTURE_BEGIN_REQ, OnGestureBeginReq)
    UIALT_MSG_MAP(UI_MSGMAP_ID_INERTIA_MOUSE_CAPTURE)
        UIMESSAGE_HANDLER_EX(WM_LBUTTONDOWN, OnCaptureStopInertiaScroll)
        UIMESSAGE_HANDLER_EX(WM_RBUTTONDOWN, OnCaptureStopInertiaScroll)
        UIMESSAGE_HANDLER_EX(WM_LBUTTONDBLCLK, OnCaptureStopInertiaScroll)
        UIMESSAGE_HANDLER_EX(WM_RBUTTONDBLCLK, OnCaptureStopInertiaScroll)
    UIALT_MSG_MAP(UI_MSGMAP_ID_INERTIA_KEYBOARD_CAPTURE)
        OnCaptureStopInertiaScroll(uMsg, wParam, lParam);
        return FALSE;
    UI_END_MSG_MAP

public:
    IScrollBarManager*  GetIScrollBarMgr();
    void  SetBindObject(IObject* pBindObj);
    IObject*  GetBindObject();

    void  SetHScrollBar(IMessage* p) { m_pHScrollBar = p; }
    void  SetVScrollBar(IMessage* p) { m_pVScrollBar = p; }
    IMessage*  GetHScrollBar() { return m_pHScrollBar; }
    IMessage*  GetVScrollBar() { return m_pVScrollBar; }

    void  SetScrollBarVisibleType(
			SCROLLBAR_DIRECTION_TYPE eDirType,
			SCROLLBAR_VISIBLE_TYPE eVisType);
    SCROLLBAR_VISIBLE_TYPE GetScrollBarVisibleType(
			SCROLLBAR_DIRECTION_TYPE eType);
    void  FireScrollMessage(
			SCROLLBAR_DIRECTION_TYPE eType,
			int nCode, 
			int nTrackPos = 0);
    
    // Set
    void  SetVScrollInfo(UISCROLLINFO* lpsi);

    bool  SetHScrollPos(int nPos);
    bool  SetVScrollPos(int nPos);
    bool  SetVScrollPosIgnoreBoundLimit(int nPos);

    void  SetVScrollLine(int nLine);
    void  SetHScrollLine(int nLine);
    void  SetVScrollWheel(int nWheel);
    void  SetHScrollWheel(int nWheel);

    void  SetScrollRange(int nX, int nY);
    void  SetHScrollRange(int nX);
    void  SetVScrollRange(int nY);
    void  SetScrollPageAndRange(const SIZE* pPage, const SIZE* pRange);

    // Get
    void  GetScrollInfo(SCROLLBAR_DIRECTION_TYPE eDirType, UISCROLLINFO* pInfo);

    void  GetScrollPos(int* pX, int* pY);
    int   GetScrollPos(SCROLLBAR_DIRECTION_TYPE eDirType);
    int   GetHScrollPos();
    int   GetVScrollPos();
	int   GetVScrollMaxPos();

    void  GetScrollPage(int* pX, int* pY);
    int   GetScrollPage(SCROLLBAR_DIRECTION_TYPE eDirType);
    int   GetHScrollPage();
    int   GetVScrollPage();

    void  GetScrollRange(int *pX, int* pY);
    int   GetScrollRange(SCROLLBAR_DIRECTION_TYPE eDirType);
    int   GetHScrollRange();
    int   GetVScrollRange();

    // Operator
    bool  DoPageDown();
    bool  DoPageUp();
    bool  DoLineDown();
    bool  DoLineUp();
    bool  DoHome();
    bool  DoEnd();

    bool  IsVScrollBarAtTop();
    bool  IsVScrollBarAtBottom();

	void  Serialize(SERIALIZEDATA* pData);

    void  DoMouseWheel(UINT nFlags, 
			short zDelta, POINT pt,
			BOOL& bHandled, BOOL& bNeedRedraw);

    bool  IsSmoothScrolling();

protected:
    void  OnGetScrollOffset(int* pxOffset, int* pyOffset);
    void  OnGetScrollRange(int* pxRange, int* pyRange);
    BOOL  OnMouseWheel(UINT nFlags, short zDelta, POINT pt);
    void  OnVScroll(int nSBCode, int nPos, IMessage* pMsgFrom);
    void  OnHScroll(int nSBCode, int nPos, IMessage* pMsgFrom);
    void  OnStateChanged(UINT nMask);
    LRESULT  OnGestureBeginReq(UINT uMsg, WPARAM, LPARAM);

protected:
    long  NotifyHScrollInfoChanged(bool bNeedUpdateVisibleState);
    long  NotifyVScrollInfoChanged(bool bNeedUpdateVisibleState);
    void  NotifyScrollbarVisibleChanged();
	void  NotifyStateChanged(UINT nMask);

    bool  _SetScrollInfo1(
			SCROLLBAR_DIRECTION_TYPE eType,
			UISCROLLINFO* lpsi,
			bool bCheckPosBound=true);
    SETSCROLLINFO_RET  _SetScrollInfo2(
			SCROLLBAR_DIRECTION_TYPE eType, 
			UISCROLLINFO* lpsi,
			bool bCheckPosBound=true);

    void  UpdateBindObjectNonClientRect();

    BOOL  OnHMouseWheel(short zDelta, POINT pt);
    BOOL  OnVMouseWheel(short zDelta, POINT pt);

    // 惯性滚动相关函数
    LRESULT  OnCaptureStopInertiaScroll(UINT uMsg, WPARAM, LPARAM);
    virtual  void SmoothScroll_Start();
    virtual  void SmoothScroll_Stop();
	virtual  SmoothScrollResult SmoothScroll_Scroll(
				MOUSEWHEEL_DIR eDir, uint nDeltaPos);
	virtual  SmoothScrollResult SmoothScroll_BounceEdge(
				MOUSEWHEEL_DIR eDir, uint nBounceHeight);
	virtual  int  SmoothScroll_GetScrolledBounceHeight();

private:
    IScrollBarManager*  m_pIScrollBarManager;
    IObject*       m_pBindObject;

    // 数据
    UISCROLLINFO   m_hScrollInfo;
    UISCROLLINFO   m_vScrollInfo;

    // 渲染
    IMessage*  m_pHScrollBar;
    IMessage*  m_pVScrollBar;
    
    SCROLLBAR_VISIBLE_TYPE  m_evScrollbarVisibleType;
    SCROLLBAR_VISIBLE_TYPE  m_ehScrollbarVisibleType;

    SmoothScroll  m_smoothScroll;
};

}