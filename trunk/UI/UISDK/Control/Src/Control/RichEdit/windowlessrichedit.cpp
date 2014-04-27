#include "StdAfx.h"
#include "windowlessrichedit.h"
#include "gifoleobject.h"
#include "UISDK\Control\Src\Control\RichEdit\richedit.h"
#include "UISDK\Kernel\Inc\Interface\iscrollbarmanager.h"
#include "UISDK\Kernel\Inc\Util\ibuffer.h"
#include "3rd\markup\markup.h"
#include "UISDK\Control\Inc\Interface\imenu.h"



// 备注：在CreateTextSerives之后，调用QueryInterface获取ITextServices接口时，总是返回E_NOINTERFACE
//       问题原来是从riched20.lib中的IID_ITextServices，是错误的。
const IID IID_ITextServices_Fix = { 0x8d33f740, 0xcf58, 0x11ce, {0xa8, 0x9d, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5} };
const IID IID_ITextHost = { 0x03bc25ff, 0x69bb, 0x25ff, {0x14, 0x0c, 0xbb, 0x69, 0xff, 0x25, 0x24, 0x0c} };

HMODULE WindowlessRichEdit::s_RichEditDll = NULL;
LONG    WindowlessRichEdit::s_refDll = 0;
UINT    WindowlessRichEdit::s_cfRichTextFormat = 0;
UINT    WindowlessRichEdit::s_cfRichTextAndObjects = 0;
UINT    WindowlessRichEdit::s_cfRichEditOleFormat = 0;
UINT    WindowlessRichEdit::s_cfUnicodeRichEditOleFormat = 0;
pfuncCreateTextServices  WindowlessRichEdit::s_funcCreateTextServices = NULL;

WindowlessRichEdit::WindowlessRichEdit()
{
	m_pRichEdit = NULL;
	m_pOleMgr = new RichEditOleObjectManager(this);
	m_nLastTimerKickCount = 0;
	SetRectEmpty(&m_rcInvalidate);
	m_bNeedRedrawScrollbar = false;
    m_bDuringTxDraw = false;

    m_nAutoResizeMaxSize = 0;
    m_bFocus = false;
	this->InitRichEidtDll();
}

void WindowlessRichEdit::BindControl(RichEdit* p)
{
	m_pRichEdit = p;
	m_pOleMgr->SetUIApplication(m_pRichEdit->GetIRichEdit()->GetUIApplication());
}
WindowlessRichEdit::~WindowlessRichEdit(void)
{
	// 销毁所有的ole对象
	this->ClearOleObjects();

	m_pRichEdit = NULL;

	// Revoke our drop target
	RevokeDragDrop();

	SAFE_DELETE(m_pOleMgr);
	__super::Destroy();
	this->ReleaseRichEidtDll();
}

//
// 有几个注意事项非常重要正确启用以这种功能的工作的： 
//
//  框架窗口将工具栏和状态栏的父项的需要创建具有 WS_CLIPCHILDREN 样式。如果不存在此样式应
//  用程序将在对象处于活动的 RichEdit 控件中时表现绘画的一些问题。 
//
//  应该用 WS_CLIPSIBLING 样式创建 RichEdit 控件本身。 此处太，如果不存在样式 RichEdit 控
//  件将出现绘画问题时对象处于活动的过程中创建子窗口。 
//
//  销毁 RichEdit 控制时，您的应用程序应停用任何处于活动对象和调用 IOleObject->Close() 
//  RichEdit 控件中的所有嵌入的对象上。如果这不进行某些对象应用程序可能不关闭，从而导致它
//  们保留在内存，即使 RichEdit 控件已被破坏。下面是演示如何处理结算的 OLE 对象的代码段：
//
void WindowlessRichEdit::ClearOleObjects()
{
	if (m_spOle)
	{
		HRESULT hr = 0;

		// 
		// Start by getting the total number of objects in the control.
		// 
		int objectCount = m_spOle->GetObjectCount();

		// 
		// Loop through each object in the control and if active
		// deactivate, and if open, close.
		// 
		for (int i = 0; i < objectCount; i++)
		{
			REOBJECT reObj;
			ZeroMemory(&reObj, sizeof(REOBJECT));
			reObj.cbStruct  = sizeof(REOBJECT);

			// 
			// Get the Nth object
			// 
			hr = m_spOle->GetObject(i,&reObj,REO_GETOBJ_POLEOBJ);
			if(SUCCEEDED(hr))
			{
				// 
				// If active, deactivate.
				// 
				if (reObj.dwFlags && REO_INPLACEACTIVE)
					m_spOle->InPlaceDeactivate();

				// 
				// If the object is open, close it.
				// 
				if(reObj.dwFlags&&REO_OPEN)
					hr = reObj.poleobj->Close(OLECLOSE_NOSAVE);

				reObj.poleobj->Release();  // release GetObject
			}
		}
	}
}

bool WindowlessRichEdit::Create(HWND hWndParent)
{
	if (NULL == m_pRichEdit)
		return false;
	if (NULL == s_funcCreateTextServices)
		return false;

	LRESULT lr = 0;

	HDC hdc = GetDC(hWndParent);
	m_nxPerInch = GetDeviceCaps(hdc, LOGPIXELSX); 
	m_nyPerInch = GetDeviceCaps(hdc, LOGPIXELSY); 
	ReleaseDC(hWndParent, hdc);

	if (!(m_dwStyle & (ES_AUTOHSCROLL | WS_HSCROLL)))
	{
		m_fWordWrap = true;  // 注：初始化m_fWordWrap不能放在CreateTextService后面，否则没作用
	}

	//////////////////////////////////////////////////////////////////////////
	// 创建Text Service

	//GdiSetBatchLimit(1);  ///??为什么都会加这么一句代码？

	IUnknown* pUnknown = NULL;
	HRESULT hr = s_funcCreateTextServices(NULL, static_cast<ITextHost*>(this), &pUnknown);
	if (FAILED(hr))
		return false;

	hr = pUnknown->QueryInterface(IID_ITextServices_Fix, (void**)&m_spTextServices);
 	pUnknown->Release();

	if (FAILED(hr) || NULL == m_spTextServices)
		return false;

	m_hParentWnd = hWndParent;

	RECT  rcClient;
	this->TxGetClientRect(&rcClient);
	hr = m_spTextServices->OnTxInPlaceActivate(&rcClient);
    m_spTextServices->OnTxUIActivate();
	UIASSERT(SUCCEEDED(hr));

	// 避免英文字体与中文字体不统一的问题
	if (m_spTextServices)
	{
		m_spTextServices->TxSendMessage(EM_SETLANGOPTIONS, IMF_DUALFONT, 0, &lr);
	}

	if (!(m_dwStyle & ES_READONLY))
	{
		// This isn't a read only window so we need a drop target.
		RegisterDragDrop();
	}

	IRichEditOle* pOle = NULL;
	hr = m_spTextServices->TxSendMessage(EM_GETOLEINTERFACE,0, (LPARAM)&pOle, &lr);
	if (SUCCEEDED(hr) && NULL != pOle)
	{
		m_spOle = pOle;
	}

	// 注：这里千万不要直接传递this进去，否则会导致txservice调用texthost时，指针错误。
	hr = m_spTextServices->TxSendMessage(EM_SETOLECALLBACK, 0, (LPARAM)static_cast<IRichEditOleCallback*>(this), &lr);
    
    // 4.1以上的RE可以支持TSF(Text Service Framework)，这样就可以解析微软拼音输入法显示不正确的问题
//    m_spTextServices->TxSendMessage(EM_SETEDITSTYLE, SES_USECTF, SES_USECTF, &lr);

    ModifyEventMask(ENM_CHANGE, 0);
	return true;
}


LPCTSTR WindowlessRichEdit::GetLibraryName()
{
#if (_RICHEDIT_VER >= 0x0200)
	return _T("RICHED20.DLL");
#else
	return _T("RICHED32.DLL");
#endif
}

void WindowlessRichEdit::InitRichEidtDll()
{
	if (NULL == s_RichEditDll)
	{
		s_RichEditDll = ::LoadLibrary(GetLibraryName());
		s_cfRichTextFormat = ::RegisterClipboardFormat(_T("Rich Text Format"));
		s_cfRichTextAndObjects = ::RegisterClipboardFormat(_T("RichEdit Text and Objects"));
		s_cfRichEditOleFormat = ::RegisterClipboardFormat(_T("UI_RichEdit_Ole_Format"));
		s_cfUnicodeRichEditOleFormat = ::RegisterClipboardFormat(_T("UI_Unicode_RichEdit_Ole_Format"));
		s_funcCreateTextServices = (pfuncCreateTextServices)GetProcAddress(s_RichEditDll, "CreateTextServices");
	}
	s_refDll++;
}
void WindowlessRichEdit::ReleaseRichEidtDll()
{
	s_refDll--;
	if (0 == s_refDll)
	{
		FreeLibrary(s_RichEditDll);
		s_RichEditDll = NULL;
	}
}

// 注：TxDraw的最后一个参数很讲究：
//     当窗口没有焦点时不能用TXTVIEW_ACTIVE，否则在TxDraw中又将光标显示出来
//     当窗口有焦点时不能用TXTVIEW_INACTIVE，否则光标caret位置不会刷新（例如窗口大小改变时）。
void WindowlessRichEdit::Draw(HDC hDC, bool bDrawShadow)
{
	if(NULL == m_spTextServices || NULL == hDC)  // Remark:如果传递NULL hdc给txdraw，居然会造成内存泄露...
		return;
    m_bDuringTxDraw = true;

	CRect  rcClient;
	this->TxGetClientRect(&rcClient);
    ::OffsetRect(&rcClient, -rcClient.left, -rcClient.top);

    if (bDrawShadow)
    {
        IUIApplication* pUIApp = m_pRichEdit->GetIRichEdit()->GetUIApplication();

        HBITMAP hMemBmp = pUIApp->GetCacheBitmap(rcClient.Width(), rcClient.Height());
        HDC hMemDC = pUIApp->GetCacheDC();
        HBITMAP hOldBmp = (HBITMAP)::SelectObject(hMemDC, hMemBmp);

        // 默认所有像素都有255的alpha channel
        // TxDraw之后，所有alpha channel为0的地方都是有文字的地方
        // 然后将所有的alpha反转即可
        Util::FixAlphaData data = {0};
        data.hBitmap = hMemBmp;
        data.lprc = &rcClient;
        data.eMode = Util::SET_ALPHA_255;
        data.bTopDownDib = TRUE;
        Util::FixBitmapAlpha(&data);

        m_spTextServices->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, hMemDC,
            NULL, (RECTL *)&rcClient, NULL, NULL, NULL, NULL, 
            m_pRichEdit->GetIRichEdit()->IsFocus()?TXTVIEW_ACTIVE:TXTVIEW_INACTIVE);

        data.eMode = Util::SET_ALPHA_INVERSE_0_255;
        Util::FixBitmapAlpha(&data);

        // 阴影颜色，简单起见，直接取一个值，懒得加接口了
        byte r = GetRValue(m_cf.crTextColor);
        byte g = GetGValue(m_cf.crTextColor);
        byte b = GetBValue(m_cf.crTextColor);
        pUIApp->ShadowBlur(hMemBmp, RGB(255-r,255-g,255-b), &rcClient, 5);
        
        // 提交到 hDC 上
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        ::AlphaBlend(hDC, rcClient.left, rcClient.top, rcClient.Width(), rcClient.Height(),
                hMemDC, 0, 0, rcClient.Width(), rcClient.Height(), bf);

        SelectObject(hMemDC, hOldBmp);
        pUIApp->ReleaseCacheDC(hMemDC);
    }

	m_spTextServices->TxDraw(DVASPECT_CONTENT, 0, NULL, NULL, hDC,
					NULL, (RECTL *)&rcClient, NULL, NULL, NULL, NULL, 
					m_pRichEdit->GetIRichEdit()->IsFocus()?TXTVIEW_ACTIVE:TXTVIEW_INACTIVE);

    m_bDuringTxDraw = false;
}

// handled表示richedit不处理这个消息
LRESULT WindowlessRichEdit::OnPreHandleMsg( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam )
{
	SetMsgHandled(FALSE);
	if (NULL == m_spTextServices) 
	{
		SetMsgHandled(TRUE);
		return 0;
	}

	return 0;
}
LRESULT WindowlessRichEdit::OnPostHandleMsg( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam )
{
	return OnDefaultHandle(Msg, wParam, lParam);
}

// RichEdit是坐标被设置为(0, 0, width, height)，因此这里需要将窗口坐标转成控件坐标
LRESULT  WindowlessRichEdit::OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	POINT  pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	m_pRichEdit->GetIRichEdit()->WindowPoint2ObjectPoint(&pt, &pt, true);

	return OnDefaultHandle(uMsg, wParam, MAKELPARAM(pt.x, pt.y));
}

LRESULT  WindowlessRichEdit::OnDefaultHandle(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lr = 0;
    HRESULT hr = m_spTextServices->TxSendMessage(uMsg, wParam, lParam, &lr);

    if (hr == S_FALSE)
    {
        lr = ::DefWindowProc(m_hParentWnd, uMsg, wParam, lParam);
    }
    return 1;
}

BOOL WindowlessRichEdit::OnSetCursor(HWND hWnd, UINT nHitTest, UINT message)
{
	SetMsgHandled(FALSE);
	POINT pt;
	GetCursorPos(&pt);
	::ScreenToClient(m_hParentWnd, &pt);
	m_pRichEdit->GetIRichEdit()->WindowPoint2ObjectClientPoint(&pt, &pt, true);

// 	if (false == HitTest(pt))
// 		return FALSE;

	RECT  rcClient;
	this->TxGetClientRect(&rcClient);

	HDC hdc = GetDC(m_hParentWnd);
	HRESULT hr = m_spTextServices->OnTxSetCursor(
		DVASPECT_CONTENT,	
		-1,
		NULL,
		NULL,
		hdc,
		NULL,
		&rcClient,
		pt.x, 
		pt.y);
	ReleaseDC(m_hParentWnd, hdc);

	if (SUCCEEDED(hr))
	{
		SetMsgHandled(TRUE);
	}
	return TRUE;
}

void WindowlessRichEdit::OnKillFocus(HWND wndFocus)
{
    m_bFocus = false;
	m_pRichEdit->GetCaret()->DestroyCaret(m_pRichEdit->GetIRichEdit(), false);
    m_pRichEdit->GetIRichEdit()->UpdateObject();
	SetMsgHandled(FALSE);
}

//
// TODO:
// 为了解决鼠标第一次点击RE拖动没反应的问题，
// 在LButtonDown中判断如果没有焦点则手动发送一次SETFOCUS
// 猜测可能是因为先来的WM_LBUTTONDOWN消息，发送给RE后，然后再触发WM_FUCOS消息发送给RE，导致拖拽中断。
void  WindowlessRichEdit::OnSetFocus(HWND wndOld)
{
    if (!m_bFocus)
    {
        m_bFocus = true;
        OnDefaultHandle(WM_SETFOCUS, 0, 0); 
    }
}
LRESULT WindowlessRichEdit::OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SetMsgHandled(FALSE);
    if (!m_bFocus)
    {
		POINT  pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		m_pRichEdit->GetIRichEdit()->WindowPoint2ObjectPoint(&pt, &pt, true);

        m_bFocus = true;
        OnDefaultHandle(uMsg, wParam, MAKELPARAM(pt.x, pt.y));  // 用于定位新光标位置。直接发送WM_SETFOCUS会导致旧的光标位置显示出来，然后再跑到新位置上去
        OnDefaultHandle(WM_SETFOCUS, 0, 0);     // 设置焦点，放弃WM_FUCOS消息中的处理
    }
    return 0;
}

void WindowlessRichEdit::OnWindowPosChanged(LPWINDOWPOS)
{
	SetMsgHandled(FALSE);

	if (m_spTextServices)
	{
		// 不加这条语句RICHEDIT就不通知我们TxSetScrollRange来显示滚动条
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_EXTENTCHANGE, TRUE);  
// 		UIDLL.dll!UI::WindowlessRichEdit::TxSetScrollRange(int fnBar=1, long nMinPos=0, int nMaxPos=464, int fRedraw=0)  Line 553	C++
// 		riched20.dll!CDisplayML::UpdateScrollBar()  + 0x12b56 bytes	
// 		riched20.dll!CDisplayML::RecalcScrollBars()  + 0x7994 bytes	
// 		riched20.dll!CDisplayML::RecalcView()  + 0x9a bytes	
// 		riched20.dll!CDisplay::UpdateView()  + 0x6e bytes	
// 		riched20.dll!CTxtEdit::NeedViewUpdate()  + 0x8 bytes	
// 		riched20.dll!CTxtEdit::OnTxPropertyBitsChange()  - 0xd bytes	
// 		UIDLL.dll!UI::WindowlessRichEdit::OnWindowPosChanged(tagWINDOWPOS * __formal=0x0049decc)  Line 326 + 0x30 bytes	C++

		// 让windowless richedit重新设置光标的位置
		if (m_pRichEdit->GetIRichEdit()->IsFocus())  
		{
			m_spTextServices->OnTxPropertyBitsChange(TXTBIT_CLIENTRECTCHANGE, TRUE);  // 会强制显示光标，因此加了个isfocus判断
		}

		// 该操作并不是立即去更新光标位置，而是会在下一个更新中调整，堆栈如下：
		// UIDLL.dll!CCaret::SetCaretPos(int x=232, int y=91)  行282	C++
		// UIDLL.dll!ITextHostImpl::TxSetCaretPos(int x=232, int y=91)  行317	C++
		// riched20.dll!CTxtSelection::CreateCaret()  + 0xfe 字节	
		// riched20.dll!CTxtSelection::UpdateCaret()  + 0x12d7f 字节	
		// riched20.dll!CDisplay::Draw()  + 0x151 字节	
		// riched20.dll!CTxtEdit::TxDraw()  + 0xeb 字节	
		// UIDLL.dll!WindowlessRichEdit::Draw(HDC__ * hDC=0xd90112c6)  行103 + 0x42 字节	C++
		// UIDLL.dll!RichEditBase::OnPaint(UI::IRenderDC * hRDC=0x01f98bd0)  行48	C++
	}
}

bool WindowlessRichEdit::HitTest(POINT pt)
{
	RECT  rcClient;
	this->TxGetClientRect(&rcClient);

	if (::PtInRect(&rcClient, pt))
	{
		return true;
	}

	return false;
}


LRESULT WindowlessRichEdit::OnChar(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  	LRESULT lr = 0;

	HRESULT hr = m_spTextServices->TxSendMessage(uMsg, wParam, lParam, &lr);

	// 	if (hr == S_FALSE)
	// 	{
	// 		lr = ::DefWindowProc(hWnd, Msg, wParam, lParam);
	// 	}
	return lr;
}

bool WindowlessRichEdit::DoGifOleUpdateRequst()
{
	int nNow = GetTickCount();
	int nDiff = nNow - m_nLastTimerKickCount;
	if (nDiff < 40)
	{
		return false;
	}
	m_nLastTimerKickCount = nNow;
	return true;
}
LRESULT WindowlessRichEdit::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SetMsgHandled(FALSE);
	if (m_pRichEdit)
	{
        if (0 == wParam)
        {
		    if (false == DoGifOleUpdateRequst()) // 该函数主要用于限制动画刷新帧数
			    return 0;
    		
		    m_pRichEdit->GetIRichEdit()->UpdateObject();
        }
        else  // 平滑滚动
        {
            TimerItem*  pItem = (TimerItem*)lParam;
            if (pItem->wParam == 1)
            {
                LRESULT lres = 0;
                m_spTextServices->TxSendMessage(WM_TIMER, pItem->nId, 0, &lres);
            }
        }
	}
    
	return 0;
}


//@cmember Create a timer with the specified timeout
BOOL WindowlessRichEdit::TxSetTimer(UINT idTimer, UINT uTimeout)
{
    //TODO:	fTimer = TRUE;
    TimerItem item ;
    item.pNotify = m_pRichEdit->GetIRichEdit();
    item.nId = idTimer;
    item.wParam = 1;
    item.lParam = 0;
    return m_pRichEdit->GetIRichEdit()->GetUIApplication()->SetTimer(uTimeout, &item);
//        return ::SetTimer(m_hParentWnd, idTimer, uTimeout, NULL);
}

//@cmember Destroy a timer
void WindowlessRichEdit::TxKillTimer(UINT idTimer)
{
   // ::KillTimer(m_hParentWnd, idTimer);
    //TODO:	fTimer = FALSE;
    m_pRichEdit->GetIRichEdit()->GetUIApplication()->KillTimerById(idTimer, m_pRichEdit->GetIRichEdit());
}

//@cmember Get the view rectangle relative to the inset
// 其实就是文字与边框的padding
// 由于UI控件的padding属性是绘制
HRESULT WindowlessRichEdit::TxGetViewInset(LPRECT prc)
{
	SetRectEmpty(prc);

#if 0 // TODO: delete this
	CRegion4 rPadding;
	m_pRichEditBase->GetPaddingRegion(&rPadding);
 	::CopyRect(prc, &rPadding);

	// Convert the inset to himetric that must be returned to ITextServices

	prc->left = DXtoHimetricX(prc->left, m_nxPerInch);
	prc->top  = DYtoHimetricY(prc->top, m_nyPerInch);
	prc->right  = DXtoHimetricX(prc->right, m_nxPerInch);
	prc->bottom = DYtoHimetricY(prc->bottom, m_nyPerInch);
#endif

	return S_OK;
}

// @cmember Retrieves the coordinates of a window's client area
HRESULT WindowlessRichEdit::TxGetClientRect(LPRECT prc)
{
	m_pRichEdit->GetIRichEdit()->GetClientRectAsWin32((CRect*)prc);
	
#if 0
	// 减去padding的大小，因为padding的已经在richedit中作为inset view rect来实现
	CRegion4 rPadding;
	m_pRichEditBase->GetPaddingRegion(&rPadding);
	prc->left -= rPadding.left;
	prc->top  -= rPadding.top;
	prc->right  += rPadding.right;
	prc->bottom += rPadding.bottom;
#endif

	return S_OK;
}

bool WindowlessRichEdit::GetInvalidateRect(RECT* prc, bool* pbNeedRedrawScrollbar, bool bClear)
{
	if (::IsRectEmpty(&m_rcInvalidate))
		return false;

	if (prc)
		::CopyRect(prc, &m_rcInvalidate);

	if (pbNeedRedrawScrollbar)
		*pbNeedRedrawScrollbar = m_bNeedRedrawScrollbar;

	if (bClear)
	{
		m_bNeedRedrawScrollbar = false;
		::SetRectEmpty(&m_rcInvalidate);
	}

	return true;
}
//@cmember InvalidateRect
// 注：prc可能只是一行的区域大小，仅刷新该区域的话能提高绘制效率！
void /*ITextHostImpl*/WindowlessRichEdit::TxInvalidateRect(LPCRECT prc, BOOL fMode)
{
    if (m_pRichEdit && m_pRichEdit->GetIRichEdit()->CanRedraw())
	{
        if (prc)
        {
		    UnionRect(&m_rcInvalidate, &m_rcInvalidate, prc);
        }
        else
        {
            // 单行模式的拖拽选中会触发该段代码，prc为空
            CRect rc;
			TxGetClientRect(&rc);

            UnionRect(&m_rcInvalidate, &m_rcInvalidate, &rc);
        }

        UIMSG msg;
        msg.message = UI_WM_REDRAWOBJECT;
        msg.pMsgTo = m_pRichEdit->GetIRichEdit();
        UIPostMessage(m_pRichEdit->GetIRichEdit()->GetUIApplication(), &msg);  // 由于RECT参数在post消息中不好传递，因此将区域保存为成员变量。同时也能起来合并无效区域的作用

//         if (prc)
//             UI_LOG_DEBUG(_T("%s RECT:%d,%d, %d,%d"), FUNC_NAME, prc->left, prc->top, prc->right, prc->bottom);
//         else
//             UI_LOG_DEBUG(_T("%s All"), FUNC_NAME);
	}

//	::InvalidateRect(m_pRichEdit->GetHWND(), prc, fMode);
//	::InvalidateRect(m_hParentWnd, prc, fMode);
}

//@cmember Get the background (either opaque or transparent)
// 注：设置TXTBACK_TRANSPARENT后，richedit将变成背景透明
//     不设置或设置其它值后，将会有白色背景
HRESULT /*ITextHostImpl*/WindowlessRichEdit::TxGetBackStyle(TXTBACKSTYLE *pstyle)
{
	// 默认使用RichEdit backrender的背景，便于统一扩展
// 	if (m_pRichEdit->IsTransparent())
 		*pstyle = TXTBACK_TRANSPARENT;
// 	else
// 		*pstyle = TXTBACK_OPAQUE;

	return S_OK;
}



//@cmember Show the scroll bar
BOOL WindowlessRichEdit::TxShowScrollBar(INT fnBar, BOOL fShow) 
{
    IScrollBarManager*  pScrollBarMgr = m_pRichEdit->GetScrollMgr();
    if (NULL == pScrollBarMgr)
        return FALSE;

	m_bNeedRedrawScrollbar = true;
	if (FALSE == fShow)
	{
		if (fnBar == SB_HORZ || fnBar == SB_BOTH)
		{
            pScrollBarMgr->SetHScrollRange(0);
		}
		if (fnBar == SB_VERT || fnBar == SB_BOTH)
		{
			//--为了解决删除文字时，删除到滚动条消失时，最上面几行仍然被隐藏的问题。此时应该将第一行显示出来
			if (0 != pScrollBarMgr->GetVScrollPos())
			{
				LRESULT lr = 0;
				m_spTextServices->TxSendMessage(WM_VSCROLL,MAKEWPARAM(SB_TOP,0),0, &lr);
				pScrollBarMgr->SetVScrollPos(0);
			}
			//--

			pScrollBarMgr->SetVScrollRange(0);
			m_pRichEdit->GetIRichEdit()->UpdateObject();
		}
	}
	return TRUE;
}

//@cmember Enable the scroll bar
BOOL WindowlessRichEdit::TxEnableScrollBar (INT fuSBFlags, INT fuArrowflags)
{
//	UI_LOG_DEBUG(FUNC_NAME);
	m_bNeedRedrawScrollbar = true;
	return TRUE;
}

//@cmember Set the scroll range
// PS: 这里RICHEDIT好像有一个BUG.在改变RICHEDIT大小时，RICHEDIT发出来TxShowScrollbar，TxSetScrollPos
//     的通知，但是居然没有发出TxSetScrollRagne的通知，导致滚动条不知如何显示。
//     (QQ2012 Input Edit也有这个问题，但Output Edit能正常显示。难道和READONLY有关系？）
BOOL WindowlessRichEdit::TxSetScrollRange(
									 INT fnBar,
									 LONG nMinPos,
									 INT nMaxPos, 
									 BOOL fRedraw)   // 这个fRedraw好像都是false 
{
    IScrollBarManager*  pScrollBarMgr = m_pRichEdit->GetScrollMgr();
    if (NULL == pScrollBarMgr)
        return FALSE;

	m_bNeedRedrawScrollbar = true;

	if (SB_HORZ == fnBar)
	{
		pScrollBarMgr->SetHScrollRange(nMaxPos - nMinPos);
	}
	else if (SB_VERT == fnBar)
	{
		pScrollBarMgr->SetVScrollRange(nMaxPos - nMinPos);
	}

	return TRUE;
}


//
//@cmember Set the scroll position
// 在输入内容的最后面敲入一个回车，结果会响应两次TxSetScrollPos，并且第一次的nPos是旧值，并且两次fRedraw都为1
// 最后导致滚动条会闪烁一次，不知道为何要这样。
//
// 为了规避这个问题，在这里做了一些处理：
//  1. 在这里不去根据fRedraw参数来刷新滚动条，而是在txinvalidateect刷新控件时同时刷新滚动条
//  2. 在TxInvalidateRect时，不能立即去刷新，否则也将导致滚动条闪烁。在这里通过模拟windows系统的行为，post一个
//     UI_WM_REDRAWOBJECT消息给richedit控件，由此来实现延迟刷新。
//  3. 在RichEditBase::OnRedrawObject中，检测是否需要刷新滚动条
//
BOOL WindowlessRichEdit::TxSetScrollPos (INT fnBar, INT nPos, BOOL fRedraw)   
{
    IScrollBarManager*  pScrollBarMgr = m_pRichEdit->GetScrollMgr();
    if (NULL == pScrollBarMgr)
        return FALSE;

	m_bNeedRedrawScrollbar = true;

	if (SB_HORZ == fnBar)
	{
        pScrollBarMgr->SetHScrollPos(nPos);
	}
	else if (SB_VERT == fnBar)
	{
	    pScrollBarMgr->SetVScrollPos(nPos);
	}
	return FALSE;
}


//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE WindowlessRichEdit::QueryInterface(REFIID riid,void **ppvObject)
{
	if (NULL == ppvObject)
		return E_INVALIDARG;

	if (::IsEqualIID(IID_IUnknown,riid) || ::IsEqualIID(IID_ITextHost,riid))
	{
		*ppvObject = static_cast<ITextHost*>(this);
	}
	else if (::IsEqualIID(riid, IID_IRichEditOleCallback))
	{
		*ppvObject = static_cast<IRichEditOleCallback*>(this);
	}
	else
		return E_NOINTERFACE;
	
	this->AddRef();
	return S_OK;
}
ULONG STDMETHODCALLTYPE WindowlessRichEdit::AddRef(void)
{
	return 1;
}
ULONG STDMETHODCALLTYPE WindowlessRichEdit::Release(void)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//               ITempHostImpl 
//
//////////////////////////////////////////////////////////////////////////

//@cmember Get the DC for the host
HDC ITextHostImpl::TxGetDC() 
{
	return ::GetDC(m_hParentWnd);
}

//@cmember Release the DC gotten from the host
INT ITextHostImpl::TxReleaseDC(HDC hdc)
{
	return ::ReleaseDC (m_hParentWnd, hdc);
}

//@cmember Send a WM_PAINT to the window
void ITextHostImpl::TxViewChange(BOOL fUpdate)
{
	// TODO: 在update richedit控件时，可能触发TxViewChange，导致又进入WindowBase::OnPaint
	//       rendertarget被二次begindraw

//	::UpdateWindow (m_hParentWnd);
}

//
//@cmember Create the caret
// hbmp [in] 
//      Handle to the bitmap that defines the caret shape.
//      If this parameter is NULL, the caret is solid. 
//      If this parameter is (HBITMAP) 1, the caret is gray. 
//      If this parameter is a bitmap handle, the caret is the specified bitmap. 
//      The bitmap handle must have been created by the CreateBitmap, 
//      CreateDIBitmap, or LoadBitmap function. 
//
//      If hBitmap is a bitmap handle, CreateCaret ignores 
//      the nWidth and nHeight parameters; the bitmap 
//      defines its own width and height.
//
BOOL WindowlessRichEdit::TxCreateCaret(HBITMAP hbmp, INT xWidth, INT yHeight)
{
    if (!m_bFocus)
        return FALSE;

    return m_pRichEdit->GetCaret()->CreateCaret(m_pRichEdit->GetIRichEdit(), hbmp, xWidth, yHeight, CARET_TYPE_UNKNOWN);
}

//@cmember Show the caret
BOOL WindowlessRichEdit::TxShowCaret(BOOL fShow) 
{
    // 如果是TxDraw发出来的消息，则不能刷新
	if (fShow)
		m_pRichEdit->GetCaret()->ShowCaret(m_pRichEdit->GetIRichEdit(), !m_bDuringTxDraw);
	else
		m_pRichEdit->GetCaret()->HideCaret(m_pRichEdit->GetIRichEdit(), !m_bDuringTxDraw);

	return TRUE;
}

//@cmember Set the caret position
BOOL WindowlessRichEdit::TxSetCaretPos(INT x, INT y)
{
    // 有可能是鼠标点击，导致位置修改，这个时候要刷新光标。但如果是TxDraw产生的消息，则不能刷新
	m_pRichEdit->GetCaret()->SetCaretPos(m_pRichEdit->GetIRichEdit(), x, y, !m_bDuringTxDraw);
	return TRUE;
}

//@cmember Scroll the content of the specified window's client area
void ITextHostImpl::TxScrollWindowEx (
	INT dx,
	INT dy,
	LPCRECT lprcScroll,
	LPCRECT lprcClip,
	HRGN hrgnUpdate,
	LPRECT lprcUpdate,
	UINT fuScroll)
{
	::ScrollWindowEx(m_hParentWnd, dx, dy, lprcScroll, lprcClip, hrgnUpdate, lprcUpdate, fuScroll);
}

//@cmember Get mouse capture
void ITextHostImpl::TxSetCapture(BOOL fCapture)
{
	if (fCapture)
		::SetCapture(m_hParentWnd);
	else
		::ReleaseCapture();
}

//@cmember Set the focus to the text window
void ITextHostImpl::TxSetFocus() 
{
	::SetFocus(m_hParentWnd);
}

//@cmember Establish a new cursor shape
// 该接口并不仅仅设置text的ibean光标，还包括sel bar的指针
// If the fText parameter is TRUE, the text services object 
// is trying to set the text cursor (the cursor appears as 
// an I-beam when it is over text that is not selected). In 
// this case, the host can set it to whatever the control 
// MousePointer property is. 
void ITextHostImpl::TxSetCursor(HCURSOR hcur, BOOL fText)
{
	::SetCursor(hcur);
}

//@cmember Converts screen coordinates of a specified point to the client coordinates
BOOL ITextHostImpl::TxScreenToClient (LPPOINT lppt)
{
	return ::ScreenToClient(m_hParentWnd, lppt);	
}

//@cmember Converts the client coordinates of a specified point to screen coordinates
BOOL ITextHostImpl::TxClientToScreen (LPPOINT lppt) 
{
	return ::ClientToScreen(m_hParentWnd, lppt);
}

//@cmember Request host to activate text services
HRESULT ITextHostImpl::TxActivate( LONG * plOldState )
{
	return S_OK;
}

//@cmember Request host to deactivate text services
HRESULT ITextHostImpl::TxDeactivate( LONG lNewState )
{
	return S_OK;
}

// 由派生类实现
//@cmember Retrieves the coordinates of a window's client area
// HRESULT ITextHostImpl::TxGetClientRect(LPRECT prc)
// {
// 	*prc = m_rectInWindow;
// 	return S_OK;
// }

//@cmember Get the default character format for the text
HRESULT ITextHostImpl::TxGetCharFormat(const CHARFORMATW **ppCF )
{
	*ppCF = &m_cf;
	return S_OK;
}

//@cmember Get the default paragraph format for the text
HRESULT ITextHostImpl::TxGetParaFormat(const PARAFORMAT **ppPF)
{
	*ppPF = &m_pf;
	return S_OK;
}

//@cmember Get the background color for the window
COLORREF ITextHostImpl::TxGetSysColor(int nIndex) 
{
	return ::GetSysColor(nIndex);
}

//@cmember Get the maximum length for the text
HRESULT ITextHostImpl::TxGetMaxLength(DWORD *plength)
{
	*plength = m_dwMaxLength;
	return S_OK;
}

//@cmember Get the bits representing requested scroll bars for the window
HRESULT ITextHostImpl::TxGetScrollBars(DWORD *pdwScrollBar)
{
	*pdwScrollBar =  m_dwStyle & (WS_VSCROLL | WS_HSCROLL
		 | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_DISABLENOSCROLL);

	return S_OK;
}

//@cmember Get the character to display for password input
HRESULT ITextHostImpl::TxGetPasswordChar(TCHAR *pch)
{
#ifdef UNICODE
	*pch = m_chPasswordChar;
#else
	WideCharToMultiByte(CP_ACP, 0, &m_chPasswordChar, 1, pch, 1, NULL, NULL) ;
#endif
	return NOERROR;
}

//@cmember Get the accelerator character
HRESULT ITextHostImpl::TxGetAcceleratorPos(LONG *pcp) 
{
	*pcp = m_laccelpos;
	return S_OK;
}

//@cmember Get the native size
HRESULT ITextHostImpl::TxGetExtent(LPSIZEL lpExtent)
{
// 	CRect  rcClient;
// 	this->TxGetClientRect(&rcClient);
// 
// 	HDC hDC = GetDC(m_hParentWnd);
// 	int xPerInch = GetDeviceCaps(hDC, LOGPIXELSX); 
// 	int yPerInch = GetDeviceCaps(hDC, LOGPIXELSY); 
// 	::ReleaseDC(m_hParentWnd, hDC);
// 
// 	// The extent matches the full client rectangle in HIMETRIC
// 	lpExtent->cx = DXtoHimetricX(rcClient.Width(), xPerInch);
// 	lpExtent->cy = DYtoHimetricY(rcClient.Height(), yPerInch);

	return E_NOTIMPL;
}

//@cmember Notify host that default character format has changed
HRESULT ITextHostImpl::OnTxCharFormatChange (const CHARFORMATW * pcf)
{
	// TODO:
	return S_OK;
}

//@cmember Notify host that default paragraph format has changed
HRESULT ITextHostImpl::OnTxParaFormatChange (const PARAFORMAT * ppf)
{
	// TODO:
	return S_OK;
}

//@cmember Bulk access to bit properties
HRESULT ITextHostImpl::TxGetPropertyBits(DWORD dwMask, DWORD *pdwBits)
{
	DWORD dwProperties = 0;

	if (m_fRich)
	{
		dwProperties = TXTBIT_RICHTEXT;
	}

	if (m_dwStyle & ES_MULTILINE)
	{
		dwProperties |= TXTBIT_MULTILINE;
	}

	if (m_dwStyle & ES_READONLY)
	{
		dwProperties |= TXTBIT_READONLY;
	}

	if (m_dwStyle & ES_PASSWORD)
	{
		dwProperties |= TXTBIT_USEPASSWORD;
	}

	if (!(m_dwStyle & ES_NOHIDESEL))
	{
		dwProperties |= TXTBIT_HIDESELECTION;
	}

// 	if (fEnableAutoWordSel)
// 	{
// 		dwProperties |= TXTBIT_AUTOWORDSEL;
// 	}
// 
// 	if (fVertical)
// 	{
// 		dwProperties |= TXTBIT_VERTICAL;
// 	}
 
	if (m_fWordWrap)
	{
		dwProperties |= TXTBIT_WORDWRAP;
	}
 
// 	if (fAllowBeep)
// 	{
// 		dwProperties |= TXTBIT_ALLOWBEEP;
// 	}
// 
// 	if (fSaveSelection)
// 	{
// 		dwProperties |= TXTBIT_SAVESELECTION;
// 	}

	*pdwBits = dwProperties & dwMask; 
	return NOERROR;
}

// Far East Methods for getting the Input Context
//#ifdef WIN95_IME
// 调用堆栈：
//  窗口接收到WM_IME_STARTCOMPOSITION消息后，转发给richedit
//  
// UICtrls.dll!UI::ITextHostImpl::TxImmGetContext()  行983	C++
// riched20.dll!CTxtEdit::GetImmContext()  + 0x2e 字节	
// riched20.dll!LocalGetImmContext()  + 0x1b 字节	
// riched20.dll!CIme::SetCompositionFont()  + 0x28 字节	
// riched20.dll!CIme_Lev2::StartComposition()  + 0x1a 字节	
// riched20.dll!StartCompositionGlue()  + 0x15d 字节	
// riched20.dll!CTextMsgFilter::HandleMessage()  + 0x3b6 字节	
// riched20.dll!CTxtEdit::TxSendMessage()  + 0x1d4f 字节	
HIMC ITextHostImpl::TxImmGetContext()
{
    return ImmGetContext(m_hParentWnd);
}
void ITextHostImpl::TxImmReleaseContext(HIMC himc)
{
    ImmReleaseContext(m_hParentWnd, himc);
}
//#endif

//@cmember Returns HIMETRIC size of the control bar.
// 注：如果不设置该变量，会导致cursor显示异常，显示成为一个行选择样式（箭头朝向右上角）
//	   像VS的一个功能
//
// 从google上找到的有关该设置的信息：
// Re: What ES_SELECTIONBAR option Does for a RichEdit 
//		ES_SELECTIONBAR is a new rich edit control style. it is really confusing since 
//		it has been defined with the same value as WS_MAXIMIZE, therefore it cannot be 
//		used as style passed to Create.
//
//		ES_SELECTIONBAR is equivalent to ECO_SELECTIONBAR that (I think) should replace 
//		confusing ES_SELECTIONBAR. 
//
//		ECO_SELECTIONBAR can be used as parameter when SetOption member is called or 
//		EM_SETOPTIONS message is sent.
//
//		When ECO_SELECTIONBAR for reach edit control is set, there is small left margin 
//		(wider than default) where cursor changes to right-up arrow allowing full line(s)
//		selection. 

HRESULT ITextHostImpl::TxGetSelectionBarWidth (LONG *lSelBarWidth)
{
	*lSelBarWidth = m_lSelBarWidth;
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
//               ITempHostImpl 其它设置方法
//
//////////////////////////////////////////////////////////////////////////

ITextHostImpl::ITextHostImpl()
{
	m_nxPerInch = m_nyPerInch = 96;
	m_hParentWnd = NULL;
	m_dwStyle = ES_MULTILINE|ES_NOHIDESEL|WS_VSCROLL | /*WS_HSCROLL |*/ ES_AUTOVSCROLL /*| ES_AUTOHSCROLL | ES_DISABLENOSCROLL*/;
	m_dwMaxLength = INFINITE;
	m_lSelBarWidth = 0;

	m_chPasswordChar = L'*';
	m_laccelpos = -1;
	m_fWordWrap = false;
	m_fRich = true;
	m_fRegisteredForDrop = false;
	m_fPassword = false;

	memset(&m_cf, 0, sizeof(CHARFORMAT2W));
    m_cf.cbSize = sizeof(CHARFORMAT2W);
    m_cf.dwMask = CFM_ALL;
    
    _tcscpy(m_cf.szFaceName, _T("宋体"));
	InitDefaultParaFormat();

	if (!(m_dwStyle & ES_LEFT))
	{
		if (m_dwStyle & ES_CENTER)
			m_pf.wAlignment = PFA_CENTER;
		else if (m_dwStyle & ES_RIGHT)
			m_pf.wAlignment = PFA_RIGHT;
	}
}

ITextHostImpl::~ITextHostImpl()
{
}
void  ITextHostImpl::Destroy()
{
	if (m_spOle)
	{
		m_spOle.Release();
	}
	if (m_spTextServices)
	{
		m_spTextServices->OnTxInPlaceDeactivate();
		m_spTextServices.Release();
		m_spTextServices = NULL;
	}
}

bool ITextHostImpl::SetCharFormatByLogFont(LOGFONT* plf)
{
    if (NULL == plf)
        return false;

    HWND hWnd = GetDesktopWindow();
    HDC  hDC = GetDC(hWnd);
    LONG yPixPerInch = GetDeviceCaps(hDC, LOGPIXELSY);
    m_cf.yHeight = abs(plf->lfHeight * 1440 / yPixPerInch);
    ReleaseDC(hWnd, hDC);

    if (plf->lfWeight >= FW_BOLD)
        m_cf.dwEffects |= CFE_BOLD;
    else
        m_cf.dwEffects &= ~CFE_BOLD;

    if (plf->lfItalic)
        m_cf.dwEffects |= CFE_ITALIC;
    else
        m_cf.dwEffects &= ~CFE_ITALIC;

    if (plf->lfUnderline)
        m_cf.dwEffects |= CFE_UNDERLINE;
    else
        m_cf.dwEffects &= ~CFE_UNDERLINE;

    if (plf->lfStrikeOut)
        m_cf.dwEffects |= CFE_STRIKEOUT;
    else
        m_cf.dwEffects &= ~CFE_STRIKEOUT;
        
    m_cf.bCharSet = plf->lfCharSet;
    m_cf.bPitchAndFamily = plf->lfPitchAndFamily;
#ifdef UNICODE
    _tcscpy(m_cf.szFaceName, plf->lfFaceName);
#else
    //need to thunk pcf->szFaceName to a standard char string.in this case it's easy because our thunk is also our copy
    MultiByteToWideChar(CP_ACP, 0, plf->lfFaceName, LF_FACESIZE, m_cf.szFaceName, LF_FACESIZE) ;
#endif

    if (m_spTextServices)
    {
        m_spTextServices->OnTxPropertyBitsChange(TXTBIT_CHARFORMATCHANGE, 
            TXTBIT_CHARFORMATCHANGE);
    }

    return true;
}
bool ITextHostImpl::SetCharFormat(CHARFORMAT2* pcf)
{
	if (NULL == pcf)
		return false;

	// Set CHARFORMAT structure
    if (pcf->dwMask & CFM_SIZE)
        m_cf.yHeight = pcf->yHeight;

    if (pcf->dwMask & CFM_COLOR)
    {
        m_cf.crTextColor = pcf->crTextColor; 
    }
    if (pcf->dwMask & CFM_BACKCOLOR)
    {
        m_cf.crBackColor = pcf->crBackColor; 
    }

    if (pcf->dwMask & CFM_BOLD)
    {
        if (pcf->dwEffects & CFE_BOLD)
            m_cf.dwEffects |= CFE_BOLD;
        else
            m_cf.dwEffects &= ~CFE_BOLD;
    }
    if (pcf->dwMask & CFM_ITALIC)
    {
        if (pcf->dwEffects & CFE_ITALIC)
            m_cf.dwEffects |= CFE_ITALIC;
        else
            m_cf.dwEffects &= ~CFE_ITALIC;
    }
    if (pcf->dwMask & CFM_UNDERLINE)
    {
        if (pcf->dwEffects & CFM_UNDERLINE)
            m_cf.dwEffects |= CFM_UNDERLINE;
        else
            m_cf.dwEffects &= ~CFM_UNDERLINE;
    }

    if (pcf->dwMask & CFM_FACE)
    {
        _tcscpy(m_cf.szFaceName, pcf->szFaceName);
    }
// 	m_cf.dwEffects = CFM_EFFECTS | CFE_AUTOBACKCOLOR;
// 	m_cf.dwEffects &= ~(CFE_PROTECTED | CFE_LINK | CFE_AUTOCOLOR);

//  LRESULT lr = 0;
//  m_spTextServices->TxSendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&m_cf, &lr);

    if (m_spTextServices)
    {
        m_spTextServices->OnTxPropertyBitsChange(TXTBIT_CHARFORMATCHANGE, 
            TXTBIT_CHARFORMATCHANGE);
    }
	return true;
}

//
// SCF_ALL
// Applies the formatting to all text in the control. Not valid with SCF_SELECTION or SCF_WORD.
// SCF_SELECTION
// Applies the formatting to the current selection. If the selection is empty, the character formatting is applied to the insertion point, and the new character format is in effect only until the insertion point changes.
// SCF_WORD
// Applies the formatting to the selected word or words. If the selection is empty but the insertion point is inside a word, the formatting is applied to the word. The SCF_WORD value must be used in conjunction with the SCF_SELECTION value.
// SCF_ASSOCIATEFONT
// RichEdit 4.1: Associates a font to a given script, thus changing the default font for that script. To specify the font, use the following members of CHARFORMAT2: yHeight, bCharSet, bPitchAndFamily, szFaceName, and lcid.
// SCF_ASSOCIATEFONT2
// RichEdit 4.1: Associates a surrogate (plane-2) font to a given script, thus changing the default font for that script. To specify the font, use the following members of CHARFORMAT2: yHeight, bCharSet, bPitchAndFamily, szFaceName, and lcid.
// SCF_DEFAULT
// RichEdit 4.1: Sets the default font for the control. 
// SCF_NOKBUPDATE
// RichEdit 4.1: Prevents keyboard switching to match the font. For example, if an Arabic font is set, normally the autokeyboard feature for Bidi languages changes the keyboard to an Arabic keyboard.
// SCF_USEUIRULES
// RichEdit 4.1: Used with SCF_SELECTION. Indicates that format came from a toolbar or other UI tool, so UI formatting rules should be used instead of literal formatting.
//
LRESULT  ITextHostImpl::SetCharFormatEx(UINT nPart, CHARFORMAT2* pcf)
{
	LRESULT  lr = 0;
	m_spTextServices->TxSendMessage(EM_SETCHARFORMAT, (WPARAM)nPart, (LPARAM)pcf, &lr);
	return lr;
}

void  ITextHostImpl::GetCharFormat(CHARFORMAT2* pcf)
{
    if (NULL == pcf)
        return;

    if (pcf->dwMask & CFM_SIZE)
        pcf->yHeight = m_cf.yHeight;

    if (m_cf.dwMask & CFM_COLOR)
        pcf->crTextColor = m_cf.crTextColor; 
    if (m_cf.dwMask & CFM_BACKCOLOR)
        pcf->crBackColor = m_cf.crBackColor; 

    if (pcf->dwMask & CFM_BOLD)
    {
        if (m_cf.dwEffects & CFE_BOLD)
            pcf->dwEffects |= CFE_BOLD;
    }
    if (pcf->dwMask & CFM_ITALIC)
    {
        if (m_cf.dwEffects & CFE_ITALIC)
            pcf->dwEffects |= CFE_ITALIC;
    }
    if (pcf->dwMask & CFM_UNDERLINE)
    {
        if (m_cf.dwEffects & CFM_UNDERLINE)
            pcf->dwEffects |= CFM_UNDERLINE;
    }

    if (pcf->dwMask & CFM_FACE)
    {
        _tcscpy(pcf->szFaceName, m_cf.szFaceName);
    }
}

void ITextHostImpl::InitDefaultParaFormat() 
{	
	memset(&m_pf, 0, sizeof(PARAFORMAT));

	m_pf.cbSize = sizeof(PARAFORMAT);
	m_pf.dwMask = PFM_ALL;
	m_pf.wAlignment = PFA_LEFT;
	m_pf.cTabCount = 1;
	m_pf.rgxTabs[0] = lDefaultTab;
}

WCHAR ITextHostImpl::SetPasswordChar(WCHAR ch_PasswordChar)
{
	WCHAR chOldPasswordChar = m_chPasswordChar;

	m_chPasswordChar = ch_PasswordChar;

	// notify text services of property change
	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_USEPASSWORD, 
			(m_chPasswordChar != 0) ? TXTBIT_USEPASSWORD : 0);
	}

	return chOldPasswordChar;
}

LONG ITextHostImpl::SetAccelPos(LONG l_accelpos)
{
	LONG laccelposOld = l_accelpos;

	m_laccelpos = l_accelpos;

	// notify text services of property change
	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_SHOWACCELERATOR, 0);
	}

	return laccelposOld;
}

void ITextHostImpl::SetWordWrap(bool fWordWrap)
{
    if (m_fWordWrap == (int)fWordWrap)
        return;

	m_fWordWrap = fWordWrap;
	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_WORDWRAP, fWordWrap ? TXTBIT_WORDWRAP : 0);
	}
}


void ITextHostImpl::SetReadOnly(bool fReadOnly)
{
	DWORD dwUpdatedBits = 0;

	if (fReadOnly)
	{
		m_dwStyle |= ES_READONLY;

		// Turn off Drag Drop 
		RevokeDragDrop();
		dwUpdatedBits |= TXTBIT_READONLY;
	}
	else
	{
		m_dwStyle &= ~ES_READONLY;

		// Turn drag drop back on
		RegisterDragDrop();	
	}

	// Notify control of property change
	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_READONLY, dwUpdatedBits);
	}
}
void  ITextHostImpl::SetMultiLine(bool b)
{
    if (IsMultiLine() == b)
        return;

    if (b)
    {
        m_dwStyle |= ES_MULTILINE; 
        m_dwStyle &= ~ES_AUTOHSCROLL;
    }
    else
    {
        m_dwStyle  &= ~ES_MULTILINE;  // 为单行时，需要能够在输满时自动滚动
        m_dwStyle |= ES_AUTOHSCROLL;
    }

    // Notify control of property change
    if (m_spTextServices)
    {
        m_spTextServices->OnTxPropertyBitsChange(TXTBIT_MULTILINE, b ? 1:0);
    }
}
bool  ITextHostImpl::IsMultiLine()
{
    return (m_dwStyle & ES_MULTILINE)?true:false;
}

void ITextHostImpl::SetMaxLegnth(DWORD dw)
{
	m_dwMaxLength = dw;
	
	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_MAXLENGTHCHANGE, 
			TXTBIT_MAXLENGTHCHANGE);
	}
}

LONG ITextHostImpl::GetSelBarWidth()
{
	return Util::HimetricXtoDX(m_lSelBarWidth, m_nxPerInch);
}

LONG ITextHostImpl::SetSelBarWidth(LONG l_SelBarWidth)
{
	LONG lOldSelBarWidth = m_lSelBarWidth;

	m_lSelBarWidth = Util::DXtoHimetricX(l_SelBarWidth, m_nxPerInch);

	if (m_lSelBarWidth)
	{
		m_dwStyle |= ES_SELECTIONBAR;
	}
	else
	{
		m_dwStyle &= (~ES_SELECTIONBAR);
	}

	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_SELBARCHANGE, TXTBIT_SELBARCHANGE);
	}
	return lOldSelBarWidth;
} 

void ITextHostImpl::SetRichTextFlag(bool b)
{
	m_fRich = b;

	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_RICHTEXT, 
			b ? TXTBIT_RICHTEXT : 0);
	}
}


void ITextHostImpl::RevokeDragDrop(void)
{
	if (m_fRegisteredForDrop)
	{
		::RevokeDragDrop(m_hParentWnd);
		m_fRegisteredForDrop = FALSE;
	}
}

//
// 备注：
// Applications that use the following functionality must call OleInitialize 
// before calling any other function in the COM library: 
//		Clipboard
//		Drag and drop
//		Object linking and embedding (OLE)
//		In-place activation
// 用oleInitialize初始化COM库，而不是使用CoInitialize
//
void ITextHostImpl::RegisterDragDrop(void)
{
	IDropTarget *pdt = NULL;

	if(!m_fRegisteredForDrop && m_spTextServices->TxGetDropTarget(&pdt) == NOERROR)
	{
		HRESULT hr = ::RegisterDragDrop(m_hParentWnd, pdt);

		if(hr == NOERROR || hr == 0x80040101)
		{	
			m_fRegisteredForDrop = true;
		}
		else
		{
			UIASSERT(0);
		}

		pdt->Release();
	}
}

void ITextHostImpl::SetPasswordMode(bool b)
{
	m_fPassword = b;

	if (m_spTextServices)
	{
		m_spTextServices->OnTxPropertyBitsChange(TXTBIT_USEPASSWORD, 
			b? TXTBIT_USEPASSWORD:0);
	}
}

bool WindowlessRichEdit::SetText(const TCHAR* szText)
{
	if (NULL == m_spTextServices)
		return false;

	// For RichEdit 1.0, the max text length would be reset by a settext so 
	// we follow pattern here as well.

	LRESULT lr = 0;
	HRESULT hr = m_spTextServices->TxSendMessage(WM_SETTEXT, 0, (LPARAM)szText, &lr);

	if (SUCCEEDED(hr))
	{
		// Update succeeded.
		DWORD cNewText = _tcslen(szText);

		// If the new text is greater than the max set the max to the new
		// text length.
		if (cNewText > m_dwMaxLength)
		{
			m_dwMaxLength = cNewText;                
		}
		return true;
	}

	return false; 
}

int  WindowlessRichEdit::GetText(TCHAR* szBuf, int nLen)
{
    if (NULL == m_spTextServices)
        return 0;

    GETTEXTEX textex = {0};
    textex.cb = (nLen+1)*sizeof(TCHAR);  // nSize需要包括末尾的\0大小
    textex.flags = GT_DEFAULT;
    textex.codepage = 1200; // unicode

    LRESULT lr = 0;
    return m_spTextServices->TxSendMessage(EM_GETTEXTEX, (WPARAM)&textex, (LPARAM)szBuf, &lr);
}

// [注意]应该返回的是lr，而不是TxSendMessage!!
int   WindowlessRichEdit::GetTextLengthW()
{
    LRESULT lr = 0;

    GETTEXTLENGTHEX data = {0};
    data.flags = GTL_DEFAULT;
    data.codepage = 1200; // unicode
    m_spTextServices->TxSendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&data, (LPARAM)0, &lr);
    return lr;

    // m_spTextServices->TxSendMessage(WM_GETTEXTLENGTH, 0, 0, &lr);
}
void  WindowlessRichEdit::GetTextW(IBuffer** ppBuffer)
{
    int nSize = GetTextLengthW();
    if (0 == nSize)
        return;

    TCHAR*  szBuffer = new TCHAR[nSize];
    GetText(szBuffer, nSize);

    UI::CreateTCHARBuffer(ppBuffer);
    (*ppBuffer)->SetBuffer((void*)szBuffer, nSize);
}

bool  WindowlessRichEdit::GetEncodeTextW(IBuffer** ppBuffer)
{
    if (NULL == ppBuffer)
        return false;

    int nSize = GetTextLengthW();
    if (0 == nSize)
        return false;

    TCHAR*  szBuffer = new TCHAR[nSize+1];
    GetText(szBuffer, nSize);

    CMarkup markup;
    markup.AddElem(L"RE");
    markup.AddAttrib(L"version", L"1.0");

    // 保存字体信息
    if (m_cf.dwEffects & CFE_BOLD)
        markup.AddAttrib(L"bold", 1);
    if (m_cf.dwEffects & CFE_ITALIC)
        markup.AddAttrib(L"italic", 1);
    if (m_cf.dwEffects & CFE_UNDERLINE)
        markup.AddAttrib(L"underline", 1);
    
    markup.AddAttrib(L"color", m_cf.crTextColor);
    markup.AddAttrib(L"face", m_cf.szFaceName);
    markup.AddAttrib(L"height", (int)m_cf.yHeight);
    markup.IntoElem();

    // 保存文本与ole
    int nTextStartIndex = 0;
    int i = 0;
    for (; i < nSize; i++)
    {
        if (szBuffer[i] == WCH_EMBEDDING)
        {
            int nCount = i - nTextStartIndex;
            if (nCount > 0)
            {
                String str;
                str.append(szBuffer+nTextStartIndex, nCount);
                markup.AddElem(L"text", str);
                markup.AddAttrib(L"length", nCount);

                nTextStartIndex = i+1;
            }

            IRichEditOleObjectItem* pOleObj = GetOleObjectByCharPos(i);
            if (pOleObj)
            {
                CComBSTR bstrEncode;
                pOleObj->GetEncodeText(&bstrEncode);
                if (bstrEncode.Length() > 0)
                {
                    markup.AddSubDoc((BSTR)bstrEncode);
                }
            }
        }
    }
    // 最后一段文本
    int nCount = i - nTextStartIndex;
    if (nCount > 0)
    {
        String str;
        str.append(szBuffer+nTextStartIndex, nCount);
        markup.AddElem(L"text", str);
        markup.AddAttrib(L"length", nCount);
    }
    SAFE_ARRAY_DELETE(szBuffer);

    UI::CreateTCHARBuffer(ppBuffer);

    szBuffer = new TCHAR[markup.GetDoc().length()+1];
    _tcscpy(szBuffer, markup.GetDoc().c_str());
    (*ppBuffer)->SetBuffer(szBuffer, markup.GetDoc().length());
    return true;
}

bool  WindowlessRichEdit::ReplaceSelectionText(const TCHAR* szText, int nSize)
{
    // EM_REPLACESEL居然只接受以null字符结尾的字符串
    if (m_spTextServices)
    {
        if (-1 == nSize || szText[nSize] == _T(''))
        {
            LRESULT lr = 0;
            m_spTextServices->TxSendMessage(EM_REPLACESEL, TRUE, (LPARAM)szText, &lr);
        }
        else
        {
            TCHAR* szTemp = new TCHAR[nSize+1];
            memcpy(szTemp, szText, nSize*sizeof(TCHAR));
            szTemp[nSize] = _T('');

            LRESULT lr = 0;
            m_spTextServices->TxSendMessage(EM_REPLACESEL, TRUE, (LPARAM)szTemp, &lr);

            SAFE_ARRAY_DELETE(szTemp);
        }
    }
    return true;
}
bool  WindowlessRichEdit::AppendText(const TCHAR* szText, int nSize)
{
    SetSel(-1, 0);
    return ReplaceSelectionText((TCHAR*)szText, nSize);
}
bool  WindowlessRichEdit::AppendEncodeTextW(const TCHAR* szText, int nSize)
{
    if (NULL == szText)
        return false;

    CMarkup markup;
    if (!markup.SetDoc(szText))
        return false;

    if (!markup.FindElem(L"RE"))
        return false;

    // 解析字体样式
    String  strColor = markup.GetAttrib(L"color");
    String  strFace = markup.GetAttrib(L"face");
    String  strHeight = markup.GetAttrib(L"height");
    String  strBold = markup.GetAttrib(L"bold");
    String  strItalic = markup.GetAttrib(L"italic");
    String  strUnderline = markup.GetAttrib(L"underline");

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_ALL;
    cf.yHeight = _ttoi(strHeight.c_str());
    cf.crTextColor = _ttoi(strColor.c_str());
    _tcscpy(cf.szFaceName, strFace.c_str());
    if (strBold == L"1")
        cf.dwEffects |= CFE_BOLD;
    if (strItalic == L"1")
        cf.dwEffects |= CFE_ITALIC;
    if (strUnderline == L"1")
        cf.dwEffects |= CFE_UNDERLINE;
    
    LRESULT lr = 0;
    SetSel(-1, 0);
    m_spTextServices->TxSendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf, &lr);

    // 解析文本与ole对象
    if (markup.IntoElem())
    {
        while (markup.FindElem())
        {
            String strTag = markup.GetTagName();
            if (strTag == _T("text"))
            {
                AppendText(markup.GetData().c_str(), -1);
            }
            else if (strTag == _T("com"))
            {
                String strCLSID = markup.GetAttrib(_T("clsid"));
    
                CLSID clsid = {0};
                CLSIDFromString((LPOLESTR)strCLSID.c_str(), &clsid);
                InsertComObject(clsid);
            }
            else if (strTag == _T("emotion"))
            {
                String strId = markup.GetAttrib(_T("id"));
                InsertSkinGif(strId.c_str());
            }
            else if (strTag == _T("gif"))
            {
                String strPath = markup.GetAttrib(_T("path"));
                InsertGif(strPath.c_str());
            }
        }
    }
    AppendText(_T("\r\n"), -1);
    return true; 
}

//
//	如果InsertObject时不去提供/创建storage对象,那么复制对象后粘贴操作将失败!
//
bool WindowlessRichEdit::InsertOleObject(IRichEditOleObjectItem* pItem)
{
	if (NULL == pItem)
		return false;

	bool       bRet = false;
	HRESULT    hr = E_FAIL;
	IOleObject*     pOleObject = NULL;
	IStorage*       pStorage = NULL;
	ILockBytes*     pLockbytes = NULL;
	LPOLECLIENTSITE pClientSite = NULL;

	do 
	{
		SCODE sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &pLockbytes);
		if (sc != S_OK)
			break;

		sc = ::StgCreateDocfileOnILockBytes(pLockbytes, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &pStorage);
		if (sc != S_OK)
			break;

		hr = m_spOle->GetClientSite(&pClientSite);
		if (FAILED(hr))
			break;

		pItem->GetOleObject(&pOleObject);
		if (NULL == pOleObject)
			break;

		REOBJECT reObj;
		ZeroMemory(&reObj, sizeof(REOBJECT));
		reObj.cbStruct = sizeof(REOBJECT);
		reObj.poleobj = pOleObject;
		reObj.polesite = pClientSite;
		reObj.dvaspect = DVASPECT_CONTENT;
		reObj.dwFlags = REO_BELOWBASELINE;
		reObj.pstg = pStorage;
		reObj.dwUser = (DWORD)pItem;
		reObj.cp = REO_CP_SELECTION;

		SIZEL  sizel = {0,0};
		reObj.sizel.cx = 0;
		reObj.sizel.cy = 0;

		// The rich edit control automatically increments the reference count for the interfaces 
		// if it holds onto them, so the caller can safely release the interfaces if they are not 
		// needed. 
		HRESULT hr = m_spOle->InsertObject(&reObj);
		if (FAILED(hr))
			break;

		OleSetContainedObject(static_cast<IOleObject*>(pOleObject), TRUE);

		this->m_pOleMgr->AddOleItem(pItem);
		bRet = true;
	} while (0);
	
	
	SAFE_RELEASE(pClientSite);
	SAFE_RELEASE(pOleObject);
	SAFE_RELEASE(pStorage);
	SAFE_RELEASE(pLockbytes);
	
	return bRet;
}
bool WindowlessRichEdit::InsertGif(const TCHAR* szGifPath)
{
	GifOleObject* pGifOle = new GifOleObject(
        m_pRichEdit->GetIRichEdit()->GetUIApplication(), 
        static_cast<IMessage*>(this->m_pRichEdit->GetIRichEdit()));

	HRESULT hr = pGifOle->LoadGif(szGifPath);
	if (FAILED(hr))
	{
		SAFE_DELETE(pGifOle);
		return false;
	}

	return this->InsertOleObject(pGifOle);
}

bool WindowlessRichEdit::InsertSkinGif(const TCHAR* szGifId)
{
    GifOleObject* pGifOle = new GifOleObject(
        m_pRichEdit->GetIRichEdit()->GetUIApplication(), 
        static_cast<IMessage*>(this->m_pRichEdit->GetIRichEdit()));

    HRESULT hr = pGifOle->LoadSkinGif(szGifId);
    if (FAILED(hr))
    {
        SAFE_DELETE(pGifOle);
        return false;
    }

    return this->InsertOleObject(pGifOle);
}

bool WindowlessRichEdit::InsertComObject(REFCLSID clsid)
{
	RichEditOleObjectItem_Com* pItem = new RichEditOleObjectItem_Com;
	pItem->Attach(clsid);

	bool bRet = this->InsertOleObject(pItem);

	IOleObject* p = NULL;
	pItem->GetOleObject(&p, false);
	p->Release();
	
	return bRet;
}

// 检测pDataObject中的各种格式，按优先级进行插入 
HRESULT WindowlessRichEdit::DoPaste(LPDATAOBJECT pDataObject, BOOL fReally, CLIPFORMAT* pOutClipFormat)
{
	if (NULL == pDataObject)
		return E_INVALIDARG;

	// 我们自己实现的OLE对象，如动画
	do 
	{
		FORMATETC format = {0};
		format.cfFormat = s_cfUnicodeRichEditOleFormat;
		format.tymed = TYMED_HGLOBAL;
		format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1; // 有时设置0会报无效的结构

		if (!fReally)
		{
			if (FAILED(pDataObject->QueryGetData(&format)))
				break;
			return S_OK;
		}

		STGMEDIUM medium = {0};
		medium.tymed = TYMED_HGLOBAL;
		if (FAILED(pDataObject->GetData(&format, &medium)))
			break;

		if (TYMED_HGLOBAL != medium.tymed)
		{
			::ReleaseStgMedium(&medium);
			break;
		}

		HGLOBAL hGlobal = medium.hGlobal;
		LPWSTR lpwstr = (LPWSTR)::GlobalLock(hGlobal);
		bool bRet = m_pOleMgr->ParseOleFormatXml(lpwstr);
		GlobalUnlock(hGlobal);

		if (false == bRet)
		{
			::ReleaseStgMedium(&medium);
			break;
		}
		
		return S_OK;
	}
	while(0);

	// static object类型：如CF_DIB/CF_BITMAP

	return E_FAIL;


#if 0
	LPOLEOBJECT     lpOleObject = NULL;
	IStorage*       pStorage = NULL;
	ILockBytes*     pLockbytes = NULL;
	LPOLECLIENTSITE pClientSite = NULL;

	bool  bRet = false;
	do 
	{
		SCODE sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &pLockbytes);
		if (sc != S_OK)
			break;

		sc = ::StgCreateDocfileOnILockBytes(pLockbytes, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &pStorage);
		if (sc != S_OK)
			break;

		HRESULT hr = m_spOle->GetClientSite(&pClientSite);
		if (FAILED(hr))
			break;

// 		sc = ::OleCreateFromData(pDataObject, IID_IUnknown, OLERENDER_DRAW, NULL, pClientSite, pStorage, (void**)&lpOleObject);
// 		if (FAILED(sc))
		{
			// CF_METAFILEPICT, CF_DIB, or CF_ BITMAP, and CF_ENHMF
			sc = ::OleCreateStaticFromData(pDataObject, IID_IUnknown, OLERENDER_DRAW, NULL, pClientSite, pStorage, (void**)&lpOleObject);
		}

		if (sc != S_OK)
			break;

		// all items are "contained" -- this makes our reference to this object
		//  weak -- which is needed for links to embedding silent update.
		OleSetContainedObject(lpOleObject, TRUE);


		REOBJECT reObj;
		memset(&reObj, 0, sizeof(reObj));
		reObj.cbStruct = sizeof(REOBJECT);
		reObj.cp = REO_CP_SELECTION;

		reObj.poleobj = lpOleObject;
		reObj.polesite = pClientSite;
		reObj.dvaspect = DVASPECT_CONTENT;
		reObj.dwFlags = REO_BELOWBASELINE;
		reObj.pstg = pStorage;
		reObj.dwUser = 0;
		reObj.cp = REO_CP_SELECTION;

		hr = m_spOle->InsertObject(&reObj);
		if (FAILED(hr))
			break;

		bRet = true;
	} while (0);

	SAFE_RELEASE(pLockbytes);
	SAFE_RELEASE(pStorage);
	SAFE_RELEASE(pClientSite);
	if (!bRet)
	{
		SAFE_RELEASE(lpOleObject);
	}
#endif
	//return bRet ? S_OK:E_FAIL;
}

IOleObject* WindowlessRichEdit::CreateOleObjectFromData(LPDATAOBJECT pDataObject, bool bOleCreateFromDataOrOleCreateStaticFromData, OLERENDER render, CLIPFORMAT cfFormat, LPFORMATETC lpFormatEtc)
{
	LPOLEOBJECT     lpOleObject = NULL;
	IStorage*       pStorage = NULL;
	ILockBytes*     pLockbytes = NULL;
	LPOLECLIENTSITE pClientSite = NULL;

	bool  bRet = false;
	do 
	{
		SCODE sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &pLockbytes);
		if (sc != S_OK)
			break;

		sc = ::StgCreateDocfileOnILockBytes(pLockbytes, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &pStorage);
		if (sc != S_OK)
			break;

		HRESULT hr = m_spOle->GetClientSite(&pClientSite);
		if (FAILED(hr))
			break;

		if (bOleCreateFromDataOrOleCreateStaticFromData)
			sc = ::OleCreateFromData(pDataObject, IID_IUnknown, render, NULL, pClientSite, pStorage, (void**)&lpOleObject);
		else
			sc = ::OleCreateStaticFromData(pDataObject, IID_IUnknown, render, NULL, pClientSite, pStorage, (void**)&lpOleObject);

		if (sc != S_OK)
			break;

		// all items are "contained" -- this makes our reference to this object
		//  weak -- which is needed for links to embedding silent update.
		OleSetContainedObject(lpOleObject, TRUE);

		bRet = true;
	} while (0);

	SAFE_RELEASE(pLockbytes);
	SAFE_RELEASE(pStorage);
	SAFE_RELEASE(pClientSite);
	if (!bRet)
	{
		SAFE_RELEASE(lpOleObject);
	}

	return lpOleObject;
}

WORD  WindowlessRichEdit::GetSelectionType() const
{
    LRESULT lr = 0;
    m_spTextServices->TxSendMessage(EM_SELECTIONTYPE, 0, 0L, &lr);
    return (WORD)lr;
}
// int WindowlessRichEdit::GetObjectTypeByOleObject(LPOLEOBJECT*  pOleObject)
// {
// 	if (NULL == pOleObject)
// 		return OT_STATIC;
// 
// 	// check for linked object
// 	LPOLELINK lpOleLink = NULL;
// 	m_lpObject->QueryInterface(IID_IOleLink, (void**)&lpOleLink);
// 	if (lpOleLink != NULL)
// 	{
// 		lpOleLink->Release();
// 		return OT_LINK;
// 	}
// 
// 	// check for static object
// 	DWORD dwStatus;
// 	if (pOleObject->GetMiscStatus(DVASPECT_CONTENT, &dwStatus) == S_OK
// 		&& (dwStatus & OLEMISC_STATIC) == 0)
// 	{
// 		return OT_EMBEDDED;
// 	}
// 
// 	// not not link, not embedding -- must be static
// 	return OT_STATIC;
// }

IRichEditOleObjectItem* WindowlessRichEdit::GetSelectionOleObject()
{
	REOBJECT reObj;
	ZeroMemory(&reObj, sizeof(REOBJECT));
	reObj.cbStruct = sizeof(REOBJECT);

	HRESULT hr = m_spOle->GetObject(REO_IOB_SELECTION, &reObj, REO_GETOBJ_NO_INTERFACES);
    if (SUCCEEDED(hr) && reObj.dwUser)
    {
        return (IRichEditOleObjectItem*)reObj.dwUser;
    }
	return NULL;
}
IRichEditOleObjectItem*  WindowlessRichEdit::GetOleObjectByCharPos(int ncp)
{
    REOBJECT reObj;
    ZeroMemory(&reObj, sizeof(REOBJECT));
    reObj.cbStruct = sizeof(REOBJECT);
    reObj.cp = ncp;

    HRESULT hr = m_spOle->GetObject(REO_IOB_USE_CP, &reObj, REO_GETOBJ_NO_INTERFACES);
    if (SUCCEEDED(hr) && reObj.dwUser)
    {
        return (IRichEditOleObjectItem*)reObj.dwUser;
    }
    return NULL;
}

long  WindowlessRichEdit::GetEventMask()
{
    long lRet = 0;
    HRESULT hr = m_spTextServices->TxSendMessage(EM_GETEVENTMASK, 0, 0, (LRESULT*)&lRet);
    return lRet;
}
void  WindowlessRichEdit::SetEventMask(long n)
{
    LRESULT lRet = 0;
    HRESULT hr = m_spTextServices->TxSendMessage(EM_SETEVENTMASK, 0, n, &lRet);
}
void  WindowlessRichEdit::ModifyEventMask(long nAdd, long nRemove)
{
    long lOldMask = GetEventMask();
    long lNewMask = lOldMask | nAdd;
    lNewMask = lNewMask &~ nRemove;

    if (lNewMask != lOldMask)
    {
        SetEventMask(lNewMask);
    }
}

//
// 注：autoresize模式RE内部不会再通知滚动条显示了
//     另外对于单行模式，RE自适应宽度.多行模式,RE自适应高度
//
void  WindowlessRichEdit::SetAutoResize(bool b, int nMaxSize)
{
    if (b)
    {
        ModifyEventMask(ENM_REQUESTRESIZE, 0);
        m_nAutoResizeMaxSize = nMaxSize;
    }
    else
    {
        ModifyEventMask(0, ENM_REQUESTRESIZE);
        m_nAutoResizeMaxSize = 0;
    }
}


//@cmember Notify host of events
HRESULT WindowlessRichEdit::TxNotify(DWORD iNotify, void *pv)
{
    BOOL bHandled = FALSE;
    HRESULT hRes = m_pRichEdit->OnTxNotify(iNotify, pv, bHandled);
    if (bHandled)
        return hRes;

    return DefTxNotify(iNotify, pv);
}
HRESULT WindowlessRichEdit::DefTxNotify(DWORD iNotify, void* pv)
{
    switch (iNotify)
    {
    case EN_REQUESTRESIZE:
        {
            REQRESIZE* pData = (REQRESIZE*)pv;
            OnRequestResize(&pData->rc);
        }
        break;
    }

    return S_OK;
}
void  WindowlessRichEdit::OnRequestResize(RECT* prc)
{
    int nDesiredWidth = prc->right - prc->left;
    int nDesiredHeight = prc->bottom - prc->top;

    nDesiredWidth += m_pRichEdit->GetIRichEdit()->GetNonClientW();
    nDesiredHeight += m_pRichEdit->GetIRichEdit()->GetNonClientH();

    CRect rcParent;
    m_pRichEdit->GetIRichEdit()->GetParentRect(&rcParent);

    if (IsMultiLine())  // 宽度固定，高度自适应
    {
        if (m_nAutoResizeMaxSize > 0 && nDesiredHeight > m_nAutoResizeMaxSize)
            nDesiredHeight = m_nAutoResizeMaxSize;

        rcParent.bottom = rcParent.top + nDesiredHeight;
    }
    else  // 高度固定，宽度自适应
    {
        if (m_nAutoResizeMaxSize > 0 && nDesiredWidth > m_nAutoResizeMaxSize)
            nDesiredWidth = m_nAutoResizeMaxSize;

        rcParent.right = rcParent.left + nDesiredWidth;
    }

    //  这个消息就是从CTxEdit::TxDraw触发的，再刷新就死循环了
    m_pRichEdit->GetIRichEdit()->SetObjectPos(&rcParent, SWP_NOREDRAW);
}

#pragma region  // IRichEditOleCallback
// This method must be implemented to allow cut, copy, paste, drag, 
// and drop operations of Component Object Model (COM) objects.
// 例如向richedit中随便拖入一个桌面上的图标，就会调用该函数
HRESULT __stdcall WindowlessRichEdit::GetNewStorage(LPSTORAGE FAR * lplpstg)
{
	if (NULL == lplpstg)
	{
		return E_INVALIDARG;
	}
	LPLOCKBYTES lpLockBytes = NULL;
	SCODE sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &lpLockBytes);
	if (sc != S_OK)
	{
		return E_OUTOFMEMORY;
	}

	sc = ::StgCreateDocfileOnILockBytes(lpLockBytes,
		STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, lplpstg);
	if (sc != S_OK)
	{
		return E_OUTOFMEMORY;
	}

	return S_OK;
}
HRESULT __stdcall WindowlessRichEdit::GetInPlaceContext(LPOLEINPLACEFRAME FAR * lplpFrame,
									LPOLEINPLACEUIWINDOW FAR * lplpDoc,
									LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	return E_NOTIMPL;
}
HRESULT __stdcall WindowlessRichEdit::ShowContainerUI(BOOL fShow)
{
	return E_NOTIMPL;
}
// 调用richedit ole的InsertObject函数时将触发这个回调函数
// 在从外部拖入一个文件到richedit时，先响应了GetNewStorage成功后，就会再调到这个接口函数
// 当返回S_OK时，这个对象将被插入，返回其它值时，对象将不会被插入
HRESULT __stdcall WindowlessRichEdit::QueryInsertObject(LPCLSID lpclsid, LPSTORAGE lpstg, LONG cp)
{
	return S_OK;
}

// 例如将richedit中的一个COM对象删除，则会调用一次该接口函数
// 例如将richedit中的一个COM对象用鼠标拖拽到另一个位置，则会调用一次该接口函数
// 该函数仅是一个通知，告诉我们有一个对象要被deleted from rich edit control;
// 这里不用调用release lpoleobj?
HRESULT __stdcall WindowlessRichEdit::DeleteObject(LPOLEOBJECT lpoleobj)
{
	return S_OK;
}

//  
//  在richedit中使用 CTRL+V粘贴、拖拽时被询问是否接受
//
// 1. 如何粘贴HTML格式？
//    在MSDN中搜索 HTML Clipboard Format
//
// 2. BOOL fReally是做什么的？
//   a. 从MSDN上看好像是用于EM_CANPASTE消息的，这个时候仅check，不去import
//   b. 在拖拽过程中直到鼠标放下前fReally都是0，放下鼠标左键时才为1
//
//   堆栈如下：
// >UIDLL.dll!UI::WindowlessRichEdit::QueryAcceptData(IDataObject * lpdataobj=0x0030f518, unsigned short * lpcfFormat=0x002ed81c, unsigned long reco=1, int fReally=0, void * hMetaPict=0x00000000)  行1634	C++
// riched20.dll!CLightDTEngine::CanPaste()  + 0x69 字节	
// riched20.dll!CDropTarget::DragEnter()  + 0x5e 字节	
// ole32.dll!CPrivDragDrop::PrivDragDrop()  + 0x1c9 字节	
// ole32.dll!PrivDragDrop()  + 0xa3 字节	
// ole32.dll!CDragOperation::GetDropTarget()  + 0x5e 字节	
// ole32.dll!CDragOperation::UpdateTarget()  + 0x242 字节	
// ole32.dll!_DoDragDrop@16()  + 0xaa 字节	
// riched20.dll!CW32System::DoDragDrop()  + 0x33 字节	
// riched20.dll!CLightDTEngine::StartDrag()  + 0x16c 字节	
// riched20.dll!CTxtEdit::OnTxMouseMove()  + 0x1aa 字节	
// riched20.dll!CTxtEdit::TxSendMessage()  + 0x24d4 字节	
// UIDLL.dll!UI::WindowlessRichEdit::OnPostHandleMsg(HWND__ * hWnd=0x00120626, unsigned int Msg=512, unsigned int wParam=1, long lParam=28377121)  行270 + 0x39 字节	C++
//
HRESULT __stdcall WindowlessRichEdit::QueryAcceptData(LPDATAOBJECT lpdataobj, CLIPFORMAT FAR * lpcfFormat, DWORD reco, BOOL fReally, HGLOBAL hMetaPict)
{
	if (!fReally) // not actually pasting
		return S_OK;

	// if direct pasting a particular native format allow it
	if (*lpcfFormat == CF_TEXT ||
		*lpcfFormat == WindowlessRichEdit::s_cfRichTextFormat ||
		*lpcfFormat == WindowlessRichEdit::s_cfRichTextAndObjects)
	{
		return S_OK;
	}

	// if format is 0, then force particular formats if available
	// ?? TODO: 如果是拖拽操作，也能用IsClipboardFormatAvailable来检测吗?
	if (*lpcfFormat == 0)
	{
		if (IsClipboardFormatAvailable((CLIPFORMAT)WindowlessRichEdit::s_cfRichTextAndObjects)) // native avail, let richedit do as it wants
		{
			*lpcfFormat = (CLIPFORMAT)WindowlessRichEdit::s_cfRichTextAndObjects;
			return S_OK;
		}
		else if (IsClipboardFormatAvailable((CLIPFORMAT)WindowlessRichEdit::s_cfRichTextFormat))
		{
			*lpcfFormat = (CLIPFORMAT)WindowlessRichEdit::s_cfRichTextFormat;
			return S_OK;
		}
	}

	// paste OLE formats
	if (SUCCEEDED(DoPaste(lpdataobj, fReally, lpcfFormat)))
		return S_FALSE;  // A return of a successful SCODE other than S_OK means that the callback either checked the data itself (if fReally is FALSE) or imported the data itself (if fReally is TRUE).

	// 最后检查一下文本格式
	if (IsClipboardFormatAvailable(CF_TEXT))
	{
		*lpcfFormat = CF_TEXT;
		return S_OK;
	}

	return E_FAIL;
}
HRESULT __stdcall WindowlessRichEdit::ContextSensitiveHelp(BOOL fEnterMode)
{
	return E_NOTIMPL;
}
// 在richedit中使用 CTRL+C/拖拽 时被调用，获取要复制的数据
// This method returns success status. If the SCODE of the return value is E_NOTIMPL, the 
// rich edit control creates its own clipboard object. 
// If the SCODE of the return value is a failure other than E_NOTIMPL, the operation fails.
//
// 之后richedit内部将会调用OleSetClipboard, SetClipboardDataObject (将IDataObject AddRef)
//
// WCH_EMBEDDING
// 
HRESULT __stdcall WindowlessRichEdit::GetClipboardData(CHARRANGE FAR * lpchrg, DWORD reco, LPDATAOBJECT FAR * lplpdataobj)
{
	WORD wSelType = this->GetSelectionType();
	if (SEL_OBJECT == wSelType)
	{
		IRichEditOleObjectItem* pItem = this->GetSelectionOleObject();
		if (!pItem)
			return S_OK;

		if (FAILED(pItem->GetClipboardData(lpchrg, reco, lplpdataobj)))
		{
			// 调用默认的方法。内部堆栈为：
			// 0. COM组件的：     ATL::IOleObjectImpl<XXX>::GetClipboardData <-- 默认ATL内部没有实现
			// 1. riched20.dll的: CDataTransferObj::Create()
			// 2. riched20.dll的: CLightDTEngine::RangeToDataObject()
			// 3. riched20.dll的: CTxtEdit::GetClipboardData()
			// 4. UIDLL.dll的:    这里m_spOle->GetClipboardData
			m_spOle->GetClipboardData(lpchrg,reco,lplpdataobj);
			return S_OK;
		}

		return S_OK;   // 注：如果SEL_OBJECT == wSelType时返回不是S_OK会造成内存pItem被AddRef，最后导致内存泄露.什么原理?/
		               // 貌似是因为为IDataObject放到clipboard中增加了一个引用计数。当再去复制一个其它内容时，引用计数就会正常

	}
	return E_NOTIMPL;
}

// 在richedit中使用鼠标拖拽时被调用
HRESULT __stdcall WindowlessRichEdit::GetDragDropEffect(BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect)
{
	if (!fDrag) // allowable dest effects
	{
		DWORD dwEffect;
		// check for force link
		if ((grfKeyState & (MK_CONTROL|MK_SHIFT)) == (MK_CONTROL|MK_SHIFT))
			dwEffect = DROPEFFECT_LINK;
		// check for force copy
		else if ((grfKeyState & MK_CONTROL) == MK_CONTROL)
			dwEffect = DROPEFFECT_COPY;
		// check for force move
		else if ((grfKeyState & MK_ALT) == MK_ALT)
			dwEffect = DROPEFFECT_MOVE;
		// default -- recommended action is move
		else
			dwEffect = DROPEFFECT_MOVE;
		if (dwEffect & *pdwEffect) // make sure allowed type
			*pdwEffect = dwEffect;
	}
	return S_OK;
}

//#include "E:\\编程\\workingpath\\test\\richeditole\\richeditole_i.h"
#include "tom.h"

// 右击RichEdit时被调用，根据鼠标右键时，鼠标下面的对象的不同，得到的参数也不同，
// 例如在空白处右击，seltype=0, lpoleobj=NULL
// 例如在一个COM对象处右击，可能seltype=2, lpoleobj = xxx;
HRESULT __stdcall WindowlessRichEdit::GetContextMenu(WORD seltype, LPOLEOBJECT lpoleobj, CHARRANGE FAR * lpchrg, HMENU FAR * lphmenu)
{
// #ifdef _DEBUG
// 	HMENU& hMenu = *lphmenu;
// 	TCHAR szInfo[128] = _T("");
// 	_stprintf(szInfo, _T("GetContextMenu Args: seltype=%d, lpoleobj=%08x, lpchrg=%d,%d"),
// 		seltype, lpoleobj, lpchrg->cpMin, lpchrg->cpMax);
// 
// 	hMenu = CreatePopupMenu();
// 	BOOL bRet = ::AppendMenu(hMenu, MF_STRING, 10001, szInfo);
//     POINT pt;
//     GetCursorPos(&pt);
//     ::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, m_hParentWnd, NULL);
//     DestroyMenu(hMenu);
// #endif

    IMenu* pMenu = NULL;
    IMenu::CreateInstance(m_pRichEdit->GetIRichEdit()->GetUIApplication(), &pMenu);
    pMenu->InitDefaultAttrib();

#define MENU_ID_CUT     1
#define MENU_ID_COPY    2
#define MENU_ID_PASTE   3
#define MENU_ID_SELALL  4

    IListItemBase* pCutItem   = pMenu->AppendString(_T("剪切"), MENU_ID_CUT);
    IListItemBase* pCopyItem  = pMenu->AppendString(_T("复制"), MENU_ID_COPY);
    IListItemBase* pPasteItem = pMenu->AppendString(_T("粘贴"), MENU_ID_PASTE);
    pMenu->AppendSeparator(-1);
    IListItemBase* pSelAllItem = pMenu->AppendString(_T("全选"), MENU_ID_SELALL);

    int nSel = 0, nSelLength = 0;
    GetSel(&nSel, &nSelLength);
    if (0 == nSelLength)
    {
        pCutItem->SetDisable(true);
        pCopyItem->SetDisable(true);
    }
    if (!::IsClipboardFormatAvailable(CF_TEXT) &&
        !::IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        pPasteItem->SetDisable(true);
    }
    if (0 == GetTextLengthW())
    {
        pSelAllItem->SetDisable(true);
    }
	if (IsReadOnly())
	{	
		pCutItem->SetDisable(true);
		pPasteItem->SetDisable(true);
	}
    CPoint pt;
    ::GetCursorPos(&pt);
    UINT nRetCmd = pMenu->TrackPopupMenu(TPM_RETURNCMD, pt.x, pt.y, m_pRichEdit->GetIRichEdit());
    SAFE_DELETE_Ixxx(pMenu);

    switch (nRetCmd)
    {
    case MENU_ID_CUT:
        {
            LRESULT lr = 0;
            m_spTextServices->TxSendMessage(WM_CUT, 0, 0, &lr);
        }
        break;

    case MENU_ID_COPY:
        {
            LRESULT lr = 0;
            m_spTextServices->TxSendMessage(WM_COPY, 0, 0, &lr);
        }
        break;

    case MENU_ID_PASTE:
        {
            LRESULT lr = 0;
            m_spTextServices->TxSendMessage(WM_PASTE, 0, 0, &lr);
        }
        break;

    case MENU_ID_SELALL:
        SetSel(0, -1);
        break;
    }

	return S_OK;
}

void  WindowlessRichEdit::SetSel(int nPos, int nLen)
{
    if (m_spTextServices)
    {
        CHARRANGE range = {nPos, nPos+nLen};
        LRESULT lr = 0;
        m_spTextServices->TxSendMessage(EM_EXSETSEL, NULL, (LPARAM)&range, &lr);
    }
}
void  WindowlessRichEdit::GetSel(int* pnPos, int* pnLen)
{
    if (m_spTextServices)
    {
        CHARRANGE range ={0, 0};
        LRESULT lr = 0;
        m_spTextServices->TxSendMessage(EM_EXGETSEL, NULL, (LPARAM)&range, &lr);

        if (pnPos)
            *pnPos = range.cpMin;
        if (pnLen)
            *pnLen = range.cpMax-range.cpMin;
    }
}
#pragma endregion