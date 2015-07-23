#include "stdafx.h"
#include "windowbase.h"
#include <Uxtheme.h>

#include "UISDK\Kernel\Inc\Interface\ixmlwrap.h"
#include "UISDK\Kernel\Inc\Interface\iuires.h"
#include "UISDK\Kernel\Inc\Interface\imapattr.h"
#include "UISDK\Kernel\Src\Atl\image.h"
#include "UISDK\Kernel\Src\Helper\layout\layout.h"
#include "UISDK\Kernel\Src\RenderLayer2\layer\renderlayer.h"
#include "UISDK\Kernel\Src\Helper\topwindow\topwindowmanager.h"
#include "UISDK\Kernel\Src\Resource\skinres.h"
#include "UISDK\Kernel\Src\Util\dwm\dwmhelper.h"
#include "UISDK\Kernel\Src\Base\Message\message.h"
#include "UISDK\Kernel\Src\Base\Attribute\attribute.h"
#include "UISDK\Kernel\Src\Base\Attribute\long_attribute.h"
#include "UISDK\Kernel\Src\Base\Attribute\string_attribute.h"
#include "UISDK\Kernel\Src\Base\Application\uiapplication.h"

using namespace UI;

WindowBase::WindowBase():
m_oWindowRender(this),
m_oMouseManager(*this)
{
    m_pIWindowBase = NULL;
	this->m_hWnd = NULL;
	this->m_oldWndProc = NULL;
	this->m_pDefaultFont = NULL;

	m_bFirsetEraseBkgnd = true;
	this->m_bDoModal = false;
	this->m_bEndModal = false;
	this->m_lDoModalReturn = 0;

	m_lMinWidth = m_lMinHeight = NDEF;
	m_lMaxWidth = m_lMaxHeight = NDEF;
	memset(&m_windowStyle, 0, sizeof(m_windowStyle));

	// 取消Panel基本中的透明属性
	ObjStyle s = {0};
	s.default_transparent = 1;
	ModifyObjectStyle(0, &s);
}

WindowBase::~WindowBase()
{
	UIASSERT( !IsWindow(m_hWnd) );          // 确保窗口已被销毁
	UIASSERT( NULL == this->m_oldWndProc ); // 确保已经取消子类化
	m_hWnd = NULL;

	SAFE_RELEASE(m_pDefaultFont);
}

HRESULT  WindowBase::FinalConstruct(IUIApplication* p)
{
    DO_PARENT_PROCESS(IWindowBase, IPanel);

    this->m_oMouseManager.SetUIApplication(p);
    this->m_oDragDropManager.SetWindowBase(this);

    return S_OK;
}

void  WindowBase::OnSerialize(SERIALIZEDATA* pData)
{
	// 放在最前面，设置好Graphics Render Library
	m_oWindowRender.OnSerialize(pData);
	Panel::OnSerialize(pData);
	
	AttributeSerializer s(pData, TEXT("WindowBase"));

	s.AddLong(XML_WINDOW_MAX_WIDTH, m_lMaxWidth)->SetDefault(NDEF);
	s.AddLong(XML_WINDOW_MAX_HEIGHT, m_lMaxHeight)->SetDefault(NDEF);
	s.AddLong(XML_WINDOW_MIN_WIDHT, m_lMinWidth)->SetDefault(NDEF);
	s.AddLong(XML_WINDOW_MIN_HEIGHT, m_lMinHeight)->SetDefault(NDEF);

	s.AddString(XML_FONT, this, 
		memfun_cast<pfnStringSetter>(&WindowBase::SetDefaultRenderFont),
		memfun_cast<pfnStringGetter>(&WindowBase::GetDefaultRenderFontId))
        ->SetDefault(NULL);

    s.AddString(XML_TEXT, m_strConfigWindowText);   
}


//	[public] bool Attach( HWND hWnd )
//
//	目的：和一个现有的窗口进行关联，对该窗口进行子类化操作
//
//	参数：
//		ID
//			[in]	UI窗口在xml中的ID，用于加载子控件
//
//		hWnd
//			[in]	要进行关联的窗口句柄
//	
//	返回：成功返回true，失败返回false
//
//	备注：调用该函数的情况是：有一个窗口（例如派生自CDialog，而不是一个UI的window类），
//		  但它却想使用我们的UI控件，这个时候需要采用这个
//
bool WindowBase::CreateUI(LPCTSTR szId)
{
	if (!m_pSkinRes)
	{
		UI_LOG_ERROR(TEXT("未初始化皮肤"));
		return false;
	}
	LayoutManager& layoutmanager = m_pSkinRes->GetLayoutManager();

	if (szId && _tcslen(szId)>0)   
	{
		//	加载子控件
		LPCTSTR  szName = m_pIWindowBase->GetObjectName();

		IUIElement*  pUIElement = NULL;
		if (layoutmanager.FindWindowElement(
                szName, szId, &pUIElement))
		{
			// 自己的属性
			this->LoadAttributeFromXml(pUIElement, false);

			// 遍历子对象
			layoutmanager.ParseChildElement(
                    pUIElement, m_pIWindowBase);

			SAFE_RELEASE(pUIElement);
		}
		else
		{
			UI_LOG_FATAL(_T("获取窗口对应的xml结点失败：name=%s, id=%s"),
				szName, szId);

			return false;
		}

		m_strId = szId;   // 提前给id赋值，便于日志输出
	}

	//
	//	为了解决xp、win7上面的一个特性：只有在按了ALT键，或者TAB键之后，才会显示控件的focus rect
	//	在初始化后，主动将该特性清除。
	//	
	// ::PostMessage(m_hWnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEACCEL|UISF_HIDEFOCUS), 0);
	
	// 为了实现列表框，树控件的theme绘制，否则画出来的效果不正确，就是一个黑边
	// http://msdn.microsoft.com/zh-cn/library/windows/desktop/bb759827(v=vs.85).aspx
	// http://www.codeproject.com/Articles/18858/Fully-themed-Windows-Vista-Controls
	// http://stackoverflow.com/questions/14039984/which-class-part-and-state-is-used-to-draw-selection

	// The following example code gives a list-view control the appearance of a Windows Explorer list: 
	// SetWindowTheme(m_hWnd, L"explorer", NULL);


    // 分层
    SetRenderLayer(true);

	return true;
}

//
//	增加一个控件的通知者为自己，即当这个控件有事件发生时，将事件通知我
//
//	Parameter
//		idPath
//			[in]	要设置的控件ID，如window/panel/loginbtn，那么idPath应为 panel/loginbtn
//		nNotifyMapID
//			[in]	通知事件的处理ID，即ProcessMessage的第二个参数
//
// void WindowBase::Control_NotifyMe( const String&  idPath, int nNotifyMapID )
// {
// 	this->Control_NotifyMe( __super::GetChildObject( idPath ), nNotifyMapID );
// }
void WindowBase::Control_NotifyMe( Object*  pObj, int nNotifyMapID )
{
	if( NULL == pObj )
	{
		UI_LOG_ERROR( _T("Window::Control_NotifyMe, 无效的参数 pObj ") );
		return ;
	}

	pObj->SetNotify(this->GetIMessage(), nNotifyMapID);
}

WindowBase::operator HWND() const
{
	return this->m_hWnd;
}

HWND  WindowBase::GetHWND() 
{
    return m_hWnd; 
}

void  WindowBase::UpdateWindow(HDC hDC, RECT* prcInvalid)
{
#ifdef TRACE_DRAW_PROCESS
    if (prcInvalid)
    {
        UI_LOG_DEBUG(_T("InvalidRect: %d,%d [%d,%d]-----------------------"),
            prcInvalid->left, prcInvalid->top, RECTW(*prcInvalid), RECTH(*prcInvalid));
    }
    else
    {
        UI_LOG_DEBUG(_T("InvalidRect: NULL-----------------------"));
    }   
#endif

    m_oWindowRender.UpdateWindow(hDC, prcInvalid); 
}

void  WindowBase::DestroyWindow()
{
	if (m_hWnd)
	{
		::DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}
	m_strId.clear();
}

bool WindowBase::Create(LPCTSTR szId, HWND hWndParent, RECT* prc/*=NULL*/)
{
	// removed by libo 20120728 允许创建一个空的窗口，例如菜单窗口
// 	UIASSERT( ID != _T("") );
// 	if( _T("") == ID )
// 	{
// 		UI_LOG_FATAL( _T("Window::Create, 未指定窗口id") );
// 		return false;
// 	}

	if (false == this->CreateUI(szId))
	{
		UI_LOG_ERROR(_T("CreateUI failed. id=%s"), szId);
		return false;
	}

	//	创建窗口句柄
	CREATESTRUCT cs;
	::ZeroMemory(&cs, sizeof(CREATESTRUCT));

    cs.hwndParent = hWndParent;
	cs.style     = WS_OVERLAPPEDWINDOW|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CLIPCHILDREN;
	cs.dwExStyle = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_WINDOWEDGE;
	cs.lpszClass = WND_CLASS_NAME;
	cs.lpszName  = _T("");
    if (prc)
    {
        cs.x = prc->left;
        cs.y = prc->top;
        cs.cx = prc->right - prc->left;
        cs.cy = prc->bottom - prc->top;
        m_windowStyle.setcreaterect = 1;
    }
    else
    {
	    cs.x = cs.y  = 0;
	    cs.cx = cs.cy = CW_USEDEFAULT;  //500; 这里不能直接写一个值。有可能窗口配置的也是这个大小，将导致收不到WM_SIZE消息，布局失败
    }

    UISendMessage(this, UI_WM_PRECREATEWINDOW, (WPARAM)&cs);

	s_create_wnd_data.AddCreateWndData(&m_thunk.cd, this);
	this->m_hWnd = ::CreateWindowEx( 
		cs.dwExStyle,
		cs.lpszClass, cs.lpszName, cs.style,
		cs.x, cs.y, cs.cx, cs.cy,
		cs.hwndParent, 0, NULL, NULL );

	if (NULL == m_hWnd)
	{
		UI_LOG_FATAL( _T("%CreateWindowEx失败"));
		return false;
	}
	return true;
}

bool WindowBase::Attach(HWND hWnd, LPCTSTR szID)
{
	if (m_hWnd)
		return false;

	m_hWnd = hWnd;

	if (false == this->CreateUI(szID))
	{
		m_hWnd = NULL;
		UI_LOG_ERROR(_T("failed. id=%s"), m_strId.c_str());
		return false;
	}

	m_windowStyle.attach = 1;
	this->m_thunk.Init( &WindowBase::ThunkWndProc, this );
	WNDPROC pProc = this->m_thunk.GetWNDPROC();

	this->m_oldWndProc = (WNDPROC)::SetWindowLong(m_hWnd, GWLP_WNDPROC, (LONG)(LONG_PTR)pProc);

	BOOL bHandled = FALSE;
	this->_OnCreate(0,0,0,bHandled);

	return true;
}

void  WindowBase::Detach()
{
	if (NULL == m_hWnd)
		return;

	BOOL bHandled = FALSE;
	_OnNcDestroy(WM_NCDESTROY, 0, 0, bHandled);
	m_hWnd = NULL;
}

void WindowBase::EndDialog(INT_PTR nResult)
{
	UIASSERT( this->m_bDoModal );

	// 让ModalWindow::DoModal里的消息循环能够退出
	this->m_lDoModalReturn = (long)nResult ;
	this->m_bEndModal = true;
	::PostMessage( this->m_hWnd, WM_NULL, 0,0 );
}
long WindowBase::DoModal(HINSTANCE hResInst, UINT nResID, LPCTSTR szID, HWND hWndParent )
{
#if 0
	UIASSERT( NULL == m_hWnd );

	UI_AddCreateWndData(&m_thunk.cd, (void*)this);
	m_strID = ID;

	return DialogBox(hResInst, MAKEINTRESOURCE(nResID), hWndParent, (DLGPROC)WindowBase::StartDialogProc);
#endif


	HWND hWnd = this->DoModeless(hResInst, nResID, szID, hWndParent);
	if( NULL == hWnd )
	{
		return -1;
	}

	return this->ModalLoop(hWndParent);
}


//
// 创建一个空的模态对话框
//
long WindowBase::DoModal(LPCTSTR szID, HWND hWndParent, bool canResize)
{

#if 0
	UIASSERT( NULL == m_hWnd );

	CREATESTRUCT cs;
	cs.style        = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
	cs.lpszClass    = WND_CLASS_NAME;
	cs.lpszName     = _T("");
	cs.x  = cs.y    = 0;
	cs.cx = cs.cy   = 100;//CW_USEDEFAULT;
	DWORD dwStyleEx = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_WINDOWEDGE;
	this->PreCreateWindow( cs );

	HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
	if (!hgbl)
		return -1;
	LPDLGTEMPLATE lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

	lpdt->cdit = 0;  // Number of controls
	lpdt->style = cs.style;
	lpdt->dwExtendedStyle = dwStyleEx;
	lpdt->x = cs.x;
	lpdt->y = cs.y;
	lpdt->cx = cs.cx;
	lpdt->cy = cs.cy;

	LPWORD lpw = (LPWORD)(lpdt + 1);
	*lpw++ = 0;             // No menu
	*lpw++ = 0;             // Predefined dialog box class (by default)

	LPWSTR lpwsz = (LPWSTR)lpw;
	int nchar = 1 + MultiByteToWideChar(CP_ACP, 0, "", -1, lpwsz, 50);  // 窗口的标题名称
	lpw += nchar;
	GlobalUnlock(hgbl); 

	UI_AddCreateWndData(&m_thunk.cd, this);
	m_strID = ID;

	long lRet = DialogBoxIndirect(/*m_pUIApplication->GetModuleInstance()*/g_hInstance, 
		(LPDLGTEMPLATE)hgbl, 
		hWndParent, 
		(DLGPROC)WindowBase::StartDialogProc); 

	GlobalFree(hgbl); 
	return lRet;
#endif
	HWND hWnd = this->DoModeless(szID, hWndParent, canResize);
	if( NULL == hWnd )
	{
		return -1;
	}

	return this->ModalLoop(hWndParent);
}

long WindowBase::ModalLoop(HWND hWndParent)
{
    // 给外部一个设置模态窗口显示的机会
    UIMSG  msg;
    msg.message = UI_WM_SHOWMODALWINDOW;
    msg.pMsgFrom = msg.pMsgTo = m_pIWindowBase;
    LRESULT lRet = UISendMessage(&msg);

    if(msg.bHandled && lRet)
    {

    }
    else
    {
    	this->ShowWindow();
	    ::UpdateWindow(this->m_hWnd);
    }
	
	this->m_bDoModal = true;
	bool bEnableWindow = false;
	if (hWndParent && GetDesktopWindow() != hWndParent && IsWindowEnabled(hWndParent))
	{
		::EnableWindow( hWndParent, FALSE );
		bEnableWindow = true;
	}

#if 0
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (FALSE == m_pUIApplication->IsDialogMessage(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (this->m_bEndModal)
		{
			this->m_bEndModal = false;
			break;
		}
	}
#else
	m_pUIApplication->MsgHandleLoop(&m_bEndModal);
	m_bEndModal = false;  // 还原，用于下次DoModal

#endif
	// hide the window before enabling the parent, etc.
	if (m_hWnd != NULL)
	{
		SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW|SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
	}

	if (bEnableWindow)
	{
		::EnableWindow( hWndParent, TRUE );
	}
	if (hWndParent != NULL && ::GetActiveWindow() == m_hWnd)
	{
		::SetActiveWindow(hWndParent);  // 如果不调用该函数，将导致父窗口跑到屏幕Z次序的后面去了
	}
	::DestroyWindow(this->m_hWnd);    // 销毁窗口

	this->m_bDoModal = false;
	return this->m_lDoModalReturn;
}

BOOL WindowBase::PreTranslateMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pRet)
{
	return FALSE;
}


// 2014.4.15
// 注：在win7 aero下面，无WS_THICKFRAME样式的Dialog会变大10px,10px
//     为了解决该问题，为每一个Dialog加上WS_THICKFRAME，同时修改WM_NCHITTEST消息、删除SysMenu中的SC_SIZE.
///    因此外部在调用该函数时，需要传递一个canResize标志
//
HWND WindowBase::DoModeless(LPCTSTR szId, HWND hWndOnwer, bool canResize)
{
	UIASSERT(NULL == m_hWnd);

	if (false == this->CreateUI(szId))
	{
		UI_LOG_ERROR(_T("CreateUI failed. id=%s"), szId);
		return false;
	}

	CREATESTRUCT cs;
	memset(&cs, 0, sizeof(CREATESTRUCT));
	cs.style        = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | WS_THICKFRAME;
	cs.lpszClass    = WND_CLASS_NAME;
	cs.lpszName     = _T("");
	cs.x  = cs.y    = 0;
	cs.cx = cs.cy   = 0;
	cs.dwExStyle    = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_WINDOWEDGE;
    UISendMessage(this, UI_WM_PRECREATEWINDOW, (WPARAM)&cs);

	HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
	if (!hgbl)
		return NULL;
	LPDLGTEMPLATE lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

	lpdt->cdit = 0;  // Number of controls
	lpdt->style = cs.style;
	lpdt->dwExtendedStyle = cs.dwExStyle;
	lpdt->x = (short)cs.x;
	lpdt->y = (short)cs.y;
	lpdt->cx = (short)cs.cx;
	lpdt->cy = (short)cs.cy;

	LPWORD lpw = (LPWORD)(lpdt + 1);
	*lpw++ = 0;             // No menu
	*lpw++ = 0;             // Predefined dialog box class (by default)

	LPWSTR lpwsz = (LPWSTR)lpw;
	int nchar = 1 + MultiByteToWideChar(CP_ACP, 0, "", -1, lpwsz, 50);  // 窗口的标题名称
	lpw += nchar;
	GlobalUnlock(hgbl); 

	s_create_wnd_data.AddCreateWndData(&m_thunk.cd, this);
	if (m_pUIApplication)
	{
		m_hWnd = CreateDialogIndirect(
					/*UIApplication::GetModuleInstance()*/g_hInstance, 
					(LPDLGTEMPLATE)hgbl, 
					hWndOnwer, 
					(DLGPROC)WindowBase::StartDialogProc); 
	}
	if (m_hWnd)
	{
		// fix默认的#32770窗口过程在大小改变时不刷新的问题
		// TODO:?? 为什么带THICKFRAME的窗口就能自己刷新
		SetClassLong(m_hWnd, GCL_STYLE, CS_HREDRAW | CS_VREDRAW |CS_DBLCLKS);

        // 删除最大化
        if (!canResize)
        {
            HMENU hMenu = GetSystemMenu(m_hWnd, FALSE);
            if (hMenu)
            {
                DeleteMenu(hMenu, SC_SIZE, 0);
            }
			m_windowStyle.dialog_noresize = 1;
        }
	}
	GlobalFree(hgbl); 
	return m_hWnd;
}
HWND WindowBase::DoModeless(HINSTANCE hResInst, UINT nResID, LPCTSTR szId, HWND hWndOnwer)
{
	UIASSERT( NULL == m_hWnd );

	if (false == this->CreateUI(szId))
	{
		UI_LOG_ERROR(_T("CreateUI failed. id=%s"), szId);
		return false;
	}

	s_create_wnd_data.AddCreateWndData(&m_thunk.cd, (void*)this);

    // 注： hWndParent仅是Owner，并不是parent
	m_hWnd = CreateDialog(hResInst, MAKEINTRESOURCE(nResID), hWndOnwer, (DLGPROC)WindowBase::StartDialogProc);

    long lStyle = GetWindowLong(m_hWnd, GWL_STYLE);
    if (!(lStyle & WS_CHILD))
    {
        if (! (lStyle&WS_THICKFRAME))
        {
            CRect  rcWindow;
            ::GetWindowRect(m_hWnd, &rcWindow);

            SetWindowLong(m_hWnd, GWL_STYLE, lStyle|WS_THICKFRAME);

            HMENU hMenu = GetSystemMenu(m_hWnd, FALSE);
            if (hMenu)
            {
               DeleteMenu(hMenu, SC_SIZE, 0);
            }
			m_windowStyle.dialog_noresize = 1;

            // 重新修改窗口为正确的大小
            SetWindowPos(m_hWnd, 0, rcWindow.left, rcWindow.top, rcWindow.Width(), rcWindow.Height(), SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
    }
	return m_hWnd;
}

//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                               消息映射                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WindowBase::StartWindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	// 获取this指针
	WindowBase* pThis = (WindowBase*)s_create_wnd_data.ExtractCreateWndData();
	UIASSERT(NULL != pThis);
	
	if (NULL == pThis)
		return 0;

	return pThis->StartProc(hwnd,uMsg,wParam,lParam, true);
}
//
//	[static] Dialog类型窗口的第一个窗口消息调用的窗口过程
//
LRESULT CALLBACK WindowBase::StartDialogProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	// 获取this指针
	WindowBase* pThis = (WindowBase*)s_create_wnd_data.ExtractCreateWndData();
	UIASSERT(NULL != pThis);

	if( NULL == pThis )
		return 0;

	return pThis->StartProc(hwnd,uMsg,wParam,lParam, false);
//	return (UINT_PTR)FALSE;
}
//
//	由StartWindowProc/StartDialogProc调用，将窗口过程转换为类对象的一个方法
//
LRESULT WindowBase::StartProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool bWindowOrDialog )
{
	// 子类化
	this->m_hWnd = hwnd;

	this->m_thunk.Init( &WindowBase::ThunkWndProc, this );
	WNDPROC pProc = this->m_thunk.GetWNDPROC();

	if (bWindowOrDialog)
	{
		this->m_oldWndProc = ::DefWindowProc;
		::SetWindowLong( m_hWnd, GWLP_WNDPROC, (LONG)(LONG_PTR)pProc);
	}
	else
	{
		this->m_oldWndProc = NULL;
		::SetWindowLong( m_hWnd, DWLP_DLGPROC, (LONG)(LONG_PTR)pProc);  
	}

	// 调用新的窗口过程 ThunkWndProc
	return pProc(hwnd, uMsg, wParam, lParam);
}

//
//	[static] LRESULT CALLBACK ThunkWndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
//
//	被ATL的thunk替换过的窗口过程
//
//	Parameter：
//		hwnd
//			[in]	这里由于被替换过了，这里的hwnd是this指针
//
//		uMsg,wParam,lParam
//			[in]	消息信息
//
LRESULT  WindowBase::ThunkWndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	WindowBase* pThis = (WindowBase*)hwnd;
	LRESULT lRet = 0;

	if (FALSE == pThis->PreTranslateMessage(uMsg, wParam, lParam, &lRet))
		lRet = pThis->WndProc( uMsg, wParam, lParam );

	return lRet;
}

LRESULT WindowBase::DefWindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if (m_oldWndProc)
	{
		return ::CallWindowProc( m_oldWndProc, m_hWnd, uMsg, wParam, lParam );
	}
	return 0;
}

//
//	[private] LRESULT WndProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
//
//	窗口被子类化过之后的窗口过程
//
LRESULT	WindowBase::WndProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	LRESULT lRes;
	UIMSG*  pOldMsg = m_pCurMsg;
	BOOL bHandled = this->ProcessWindowMessage(m_hWnd, uMsg, wParam, lParam, lRes, 0);  // 调用BEGIN_MSG_MAP消息映射列表
	if (bHandled)
	{
		return WndProc_GetRetValue(uMsg, wParam, lParam, bHandled, lRes);
	}
    
    // 直接发给当前窗口处理
    UIMSG  msg;
	msg.message = uMsg;
	msg.pMsgTo = this->GetIMessage();
	msg.wParam = wParam;
	msg.lParam = lParam;

	// 如果这个消息被处理过了，直接返回，不用再调用旧的窗口过程了
	//if (static_cast<IMessage*>(this)->ProcessMessage(&msg, 0))
    UISendMessage(&msg);
    lRes = WndProc_GetRetValue(uMsg, wParam, lParam,  msg.bHandled, msg.lRet);

	if (uMsg == WM_NCDESTROY)
	{
		// 注：为什么不在这里直接调用OnFinalMessage，却还要再加一个状态位？
		// 因为WM_NCDESTROY函数由DestroyWindow api触发，而DestroyWindow api
		// 可能位于任何一个当前窗口的消息响应中，因此当pOldMsg==NULL时，即表示
		// 没有消息嵌套了，在检查一次WINDOW_STYLE_DESTROYED标志即可。
		m_windowStyle.destroyed = 1;
	}
	if (m_windowStyle.destroyed && pOldMsg == NULL)
	{
		m_windowStyle.destroyed = 0;
	}

	return lRes;
}

// 设置对话框的DialogProc返回值，见MSDN中对DialogProc返回值的说明
LRESULT  WindowBase::WndProc_GetRetValue(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL bHandled, LRESULT lRet)
{
    LRESULT lResult = 0; 

    if (NULL == m_oldWndProc)  // Dialog窗口过程
    {
        switch(uMsg)
        {
        case WM_INITDIALOG: 
            if (GetMouseMgr()->GetFocusObject())
                lResult = FALSE;  // 不使用其焦点设置
            else
                lResult = lRet;
            break;

        case WM_CHARTOITEM:
        case WM_COMPAREITEM:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORMSGBOX:
        case WM_QUERYDRAGICON:
        case WM_VKEYTOITEM:
            // return directly，这几个消息不按DialogProc返回值规定走
            lResult = lRet;
            break;

        default:
            SetWindowLong(m_hWnd, DWL_MSGRESULT, lRet) ;
            lResult = bHandled ? 1:0;
            break;
        }
    }
    else
    {
        if (bHandled)
        {
            lResult = lRet;
        }
        else
        {
            lResult = DefWindowProc(uMsg, wParam, lParam);
        }
    }
    return lResult;
}

LRESULT WindowBase::_OnSetCursor( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = TRUE;

	if (0 == HIWORD(lParam))  // is menu popup
	{ 
		::SetCursor( ::LoadCursor(NULL, IDC_ARROW) );
		return TRUE;
	}

	HWND hWnd = (HWND)wParam;
	if (0 != wParam && hWnd != m_hWnd)
	{
		return DefWindowProc(uMsg,wParam,lParam);  // 鼠标位于窗口上的其它windows控件上面，不处理
	}  
	else if (hWnd == m_hWnd)  
	{
		// TODO: 这里需要区分window/dialog吗
        UINT nHitTest = LOWORD(lParam);
        if (m_oldWndProc)
        {
            if (nHitTest != HTCLIENT)   
            {
                SetCursorByHitTest(nHitTest);
                return TRUE;
            }
        }
        else
        {
            // 用于鼠标位于system window的边缘时，应该调用默认的处理过程。
            // 包括当自己弹出一个模态框，返回HTERROR时
		    if (nHitTest != HTCLIENT)    
		    {
			    //return DefWindowProc(uMsg,wParam,lParam);  // 2014.4.10 dialog直接return 0居然没反应，不会根据hittest设置鼠标。因此直接调用defwindowproc
                return ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
		    }
        }

//      Removed 20120824 -- 在实现windowless richedit时，不能采用延时发送setcursor的办法，否则在
//      选择文本时鼠标样式一直闪烁。因此修改方案为：先发送，然后在WM_MOUSEMOVE中检测如果hover obj
//      发生了变化的话，重新再发送一次WM_SETCURSOR
	}
	if (TRUE == m_oMouseManager.HandleMessage(uMsg, wParam, lParam, &bHandled)) // 仅发给当前hover object
	{
		return TRUE;
	}

	// 让窗口去处理这个消息
	UIMSG msg;
	msg.message = uMsg;
	msg.pMsgTo = this->GetIMessage();
	msg.wParam  = wParam;
	msg.lParam  = lParam;
	if ((this->GetIMessage())->ProcessMessage(&msg, 0)) 
		return msg.lRet;

	// 还是没有人处理
	::SetCursor(::LoadCursor(NULL,IDC_ARROW));
	return TRUE;
}

void  WindowBase::SetCursorByHitTest(UINT nHitTest)
{
    static HCURSOR _arrow = ::LoadCursor(NULL, IDC_ARROW);

    switch(nHitTest)
    {
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
        {
            static HCURSOR _cursor = ::LoadCursor(NULL, IDC_SIZENWSE);
            ::SetCursor(_cursor);
        }
        break;

    case HTTOP:
    case HTBOTTOM:
        {
            static HCURSOR _cursor = ::LoadCursor(NULL, IDC_SIZENS);
            ::SetCursor(_cursor);
        }
        break;

    case HTLEFT:
    case HTRIGHT:
        {
            static HCURSOR _cursor = ::LoadCursor(NULL, IDC_SIZEWE);
            ::SetCursor(_cursor);
        }
        break;

    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
        {
            static HCURSOR _cursor = ::LoadCursor(NULL, IDC_SIZENESW);
            ::SetCursor(_cursor);
        }
        break;

    case HTCAPTION:
        ::SetCursor(_arrow);
        break;

    default:
        ::SetCursor(_arrow);
        break;	
    }
}

LRESULT  WindowBase::_OnNcHitTest( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
    bHandled = FALSE;
    if (!m_oldWndProc)
    {
        if (m_windowStyle.dialog_noresize)
        {
            LRESULT lr = ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
            if (lr >= HTLEFT && lr <= HTBORDER)  // 用HTTOPRIGHT还有是问题，非得用HTBORDER，为什么
            {
                lr = HTBORDER;
                bHandled = TRUE;
                return lr;
            }
        }
    }
    return 0;
}

// 该消息已在WM_PAINT中分发。
// 为了实现无闪烁的绘制，必须将所有的绘制代码放在一个地方处理，要么在WM_ERASEBKGND，要么在WM_PAINT
// 但WM_ERASEBKGND不是每次都会触发的（在处理异形窗口时出现该问题），因此考虑将绘制代码都放在WM_PAINT中处理
// 
// 如果 lParam == 0，则表示是系统自己发出来的消息，由于系统消息会导致和WM_PAINT DC不一致，从而产生闪烁
// 因此将WM_ERASEBKGND消息放在WM_PAINT中由我们自己发出，并且将lParam置为非0
LRESULT WindowBase::_OnEraseBkgnd( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = FALSE;
	if (0 == lParam)
	{
		bHandled = TRUE;
		if (m_bFirsetEraseBkgnd)
		{
			m_bFirsetEraseBkgnd = false;

            //m_oWindowRender.UpdateWindow((HDC)wParam);
            //ValidateRect(m_hWnd, NULL);  // 清空无效区域-- 注掉，见下文
            
			// 如果什么也不做，会导致窗口第一次显示时，窗口先显示一次黑色，例如combobox.listbox/menu
			// 如果直接调用DefWindowProc会导致窗口显示白色，但最终显示的界面不一定就是白色的，也会导致闪烁
            // 因此在这里先做一次全量绘制
			// DefWindowProc(uMsg, wParam, lParam); 

            // Bug 2014.3.7 在分层窗口中，第一次触发该函数时，有这种情况:
            //   ShowWindow(SW_SHOW);
            //   SetWidowPos(x, y, cx, cy);
            // 结果在SetWindowPos中触发第一次erasebkgnd，但还没有走到windowposchanging中，导致
            // layeredwindow::commit中使用旧的窗口位置提交分层窗口，新的窗口位置无效（但分层窗口中
            // 保存的仍然是x,y,下一次刷新才能更新到正确位置)，因此将ValidateRect(NULL)注掉，避免出现这种问题
            // 
		}
		
		return 1;   // 对于Dialog类型，需要返回1来阻止系统默认绘制
	}
	return 0;
}

//
// [注]分层窗口的InvalidateRect，拿到的ps.rcPaint永远是空的
//
LRESULT WindowBase::_OnPaint( UINT uMsg, WPARAM wParam,LPARAM lParam, BOOL& bHandled )
{
	PAINTSTRUCT ps;
	HDC  hDC = NULL;
	RECT rcInvalid = {0};

	if (NULL == wParam)
	{
		hDC = ::BeginPaint(this->m_hWnd ,&ps);
		rcInvalid = ps.rcPaint;

		if (IsRectEmpty(&rcInvalid) && 
			GetWindowLong(m_hWnd, GWL_EXSTYLE)&WS_EX_LAYERED)
		{
			GetClientRect(m_hWnd, &rcInvalid);
		}
	}
	else
	{
		GetClipBox(hDC, &rcInvalid);
        if (IsRectEmpty(&rcInvalid))
        {
            GetClientRect(m_hWnd, &rcInvalid);
        }
	}

	if (!IsRectEmpty(&rcInvalid))
	{
		UpdateWindow(hDC, &rcInvalid);
	}
	
	if(NULL == wParam)
	{
		EndPaint(m_hWnd,&ps);
	}
	
	return 1;  //  在_OnPaint中返回0，会导致dialog类型的窗口，被其它窗口覆盖后移出来刷新异常!!!
}

LRESULT WindowBase::_OnSize( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = FALSE;
	if (SIZE_MINIMIZED == wParam)
        return 0;
	
	RECT rcWindow;
	::GetWindowRect(m_hWnd, &rcWindow);
	::GetClientRect(m_hWnd, &m_rcParent);

	if (GetConfigWidth() > 0)
		SetConfigWidth(rcWindow.right-rcWindow.left);
	if (GetConfigHeight() > 0)
		SetConfigHeight(rcWindow.bottom-rcWindow.top);

	bHandled = TRUE;
	notify_WM_SIZE(wParam, LOWORD(lParam), HIWORD(lParam));

    m_oWindowRender.OnWindowSize(LOWORD(lParam), HIWORD(lParam));

	// 注：这里不要调用InvalidateRect。对于CustomWindow窗口拉大时，当前窗口的RGN还是老的设置，
	//     导致接收到WM_PAINT时的ps.rcPaint也被该老RGN剪裁，因此刷新区域不完整。这
	//     也导致接下来的CustomWindow::UpdateWindowRgn拿到的窗口背景不完整，计算剪裁
	//     区域错误。
	// 因此这里还是直接调用了窗口的UpdateObject
	//::InvalidateRect(m_hWnd, NULL, FALSE);
	
	if (m_oWindowRender.CanCommit())
	{
		// 如果!cancommit，有可能是窗口刚创建时的初始化，直接走WM_PAINT消息
		// 然后由windowrender解除cancommit限制

		this->UpdateObject();
		ValidateRect(m_hWnd, NULL);
	}
	else
	{
		InvalidateRect(m_hWnd, NULL, TRUE);
	}
	
	return 0;
}

LRESULT WindowBase::_OnCreate(
        UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;

    // 特殊处理：
    // 用于将D2D RenderTarget绑定到窗口上面.D2D与GDI/GDIPlus
    // 不一样，它不需要再弄一个双缓冲，直接begindraw/enddraw即可
	if (m_pRenderLayer)
	{
		IRenderTarget*  pRT = m_pRenderLayer->GetRenderTarget();
		if (pRT)
		{
			pRT->BindHWND(m_hWnd);
		}
	}

	//
	//  有可能m_strID为空（不加载资源，例如临时的popupconotrolwindow）
	//	因此没有将AddTopWindowObject、OnInitWindow放在CreateUI中执行
	//
	// if (!IsChildWindow())--子窗口也是一个UI窗口，也维护起来
	{
		TopWindowManager* pTopWndMgr = 
            m_pUIApplication->GetTopWindowMgr();
		if (pTopWndMgr)
			pTopWndMgr->AddTopWindowObject(this);
	}

	// 布局
	if (m_windowStyle.attach)  // attach的窗口直接使用外部的大小
	{
		::GetClientRect(m_hWnd, &m_rcParent);

        // 避免此时调用GetDesiredSize又去测量窗口大小了，
        // 导致窗口被修改为自适应大小 
		CRect rcWindow;               
		::GetWindowRect(m_hWnd, &rcWindow);
		SetConfigWidth(rcWindow.Width());
		SetConfigHeight(rcWindow.Height());

        // 因为Attach到的窗口初始化时已经收不到WM_SIZE了，
        // 因此自己再发一次，
        // 通知创建RenderTarget，否则后面的一些刷新将失败
		notify_WM_SIZE(0, m_rcParent.Width(), m_rcParent.Height());
        this->UpdateLayout(false);
	}
    else
    {
        if (m_windowStyle.setcreaterect)
        {
            // 避免此时调用GetDesiredSize又去测量窗口大小了，
            // 导致窗口被修改为自适应大小 
            CRect rcWindow;    
            ::GetWindowRect(m_hWnd, &rcWindow);
            SetConfigWidth(rcWindow.Width());
            SetConfigHeight(rcWindow.Height());

            ::GetClientRect(m_hWnd, &m_rcParent);
            this->UpdateLayout(false);
        }
        else
        {
            // 不能放在 OnInitialize 后面。
            // 因为有可能OnInitialize中已经调用过 SetWindowPos
            DesktopLayout dl;  
            dl.Arrange(this);
        }
    }

    if (!m_strConfigWindowText.empty())
        ::SetWindowText(m_hWnd, m_strConfigWindowText.c_str());

    // 创建默认字体
    if (!m_pDefaultFont)
        SetDefaultRenderFont(EMPTYTEXT);

    // 给子类一个初始化的机会 (virtual)，
    // 例如设置最大化/还原按钮的状态
    this->OnInnerInitWindow();

    // 防止在实现显示动画时，先显示了一些初始化中刷新的内容。
    // 注：不能只限制一个layer
    m_oWindowRender.SetCanCommit(false); 
    {
        UISendMessage(m_pIMessage, UI_WM_INITIALIZE2);

        UIMSG msg;
        UIMSG msg2;
        msg.message = UI_WM_INITIALIZE;
        msg2.message = UI_WM_INITIALIZE2;
        ForwardMessageToChildObject2(this, &msg, &msg2);

        UISendMessage(m_pIMessage, UI_WM_INITIALIZE);
    }
	m_oWindowRender.SetCanCommit(true);

	// 设置默认对象
	m_oMouseManager.SetDefaultObject(
            m_oMouseManager.GetOriginDefaultObject(),
            false);

    if (m_windowStyle.attach) // 主动触发刷新
    {
        m_oWindowRender.UpdateWindow(NULL, NULL);
    }
	return 0;
}
LRESULT WindowBase::_OnNcDestroy( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;

	SyncWindowHelper<WindowBase>::_OnNcDestroy();

//	if (!IsChildWindow())
	{
		TopWindowManager* pTopWndMgr = m_pUIApplication->GetTopWindowMgr();
		if (pTopWndMgr)
			pTopWndMgr->RemoveTopWindowObject(this);
	}

    this->m_oDragDropManager.OnWindowDestroy();
	m_oMouseManager.HandleMessage( uMsg, wParam, lParam, &bHandled);

	if (m_oldWndProc)
	{
		::SetWindowLong( m_hWnd, GWLP_WNDPROC, (LONG)(LONG_PTR)m_oldWndProc);
		m_oldWndProc = NULL;
	}
	m_hWnd = NULL;
	
	this->DestroyChildObject();   // 删除子对象
	return 0;
}

LRESULT WindowBase::_OnHandleMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (m_bSizeMove)  // 拖拽过程中不处理
	{
		bHandled = FALSE;
		return 0;
	}

	bHandled = FALSE;
	LRESULT lRet = m_oMouseManager.HandleMouseMessage(uMsg, wParam, lParam, &bHandled);

    if (bHandled)
        return lRet;

	if (m_oMouseManager.GetHoverObject() || NULL != m_oMouseManager.GetPressObject())
	{
		bHandled = TRUE;
	}
	return 0;
}
LRESULT  WindowBase::_OnHandleKeyBoardMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    bHandled = FALSE;
    return m_oMouseManager.HandleKeyboardMessage( uMsg, wParam, lParam, &bHandled );
}

LRESULT  WindowBase::_OnHandleTouchMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    bHandled = FALSE;
    return m_oMouseManager.HandleTouchMessage( uMsg, wParam, lParam, bHandled);
}


LRESULT WindowBase::_OnSetFocus( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = TRUE;


	m_oMouseManager.HandleMessage( uMsg, wParam, lParam, &bHandled);

	UIMSG msg;
	msg.message = uMsg;
	msg.pMsgTo = this->GetIMessage();
	msg.wParam  = wParam;
	msg.lParam  = lParam;
    UISendMessage(&msg);

	return 1;    // 注：由于Dialog默认的WM_SETFOCUS会将焦点再设置到自己的第一个控件上面，因此如果在这里return 0或者bHandled = FALSE
	             //     将导致::SetFocus(m_hWnd)的焦点到窗口上面后，又被窗口自己设置到控件上面，导致SetFocus达不到我们的本意
}
LRESULT WindowBase::_OnKillFocus( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = TRUE;

	m_oMouseManager.HandleMessage( uMsg, wParam, lParam, &bHandled);

	UIMSG msg;
	msg.message = uMsg;
	msg.pMsgTo = this->GetIMessage();
	msg.wParam = wParam;
	msg.lParam = lParam;
//	this->ProcessMessage( &msg );
    UISendMessage(&msg);

	return 1 ;
}

// 用于解决如果WindowBase是一个子窗口，放在一个普通顶层窗口上，那么点击WindowBase
// 将无法得到焦点。因此在这里响应MouseActive消息，将自己SetFocus
// 至少像系统按钮控件是如何处理的点击得到焦点就不得而知了，但WS_TABSTOP不是原因，试过了，没用。
LRESULT WindowBase::_OnMouseActive( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
    bHandled = FALSE;
    if (IsChildWindow() && ::GetFocus() != m_hWnd)
    {
        ::SetFocus(m_hWnd);
    }
    return 0;
}

LRESULT WindowBase::_OnThemeChange( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = FALSE;
	
    UIMSG  msg;
    msg.message = uMsg;
    msg.wParam = wParam;
    msg.lParam = lParam;

	Message::ForwardMessageToChildObject(this, &msg);
	return 0;
}

HMONITOR  WindowBase::GetPrimaryMonitor()
{
    POINT pt = {0,0};
    return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY); 
}


// minmax.c xxxInitSendValidateMinMaxInfo
//
// 1. 如果最大化到副屏上时，ptMaxSize, ptMaxPosition指的是主屏的大小。
//    For systems with multiple monitors, the ptMaxSize and ptMaxPosition members
//    describe the maximized size and position of the window on the primary monitor, 
//    even if the window ultimately maximizes onto a secondary monitor. 

// 2. 系统内部会判断如果pinfo的maxsize大于主屏宽高时，将相应的调整为副屏的大小。
//    In that case, the window manager adjusts these values to compensate for differences
//    between the primary monitor and the monitor that displays the window. Thus, if 
//    the user leaves ptMaxSize untouched, a window on a monitor larger than the primary
//    monitor maximizes to the size of the larger monitor.
//
// 3. 但是!!!对于一个窗口 -- 与是否有边框无关。
//
//     . 有边框的窗口会多出若干像素border，它的ptMaxSize本身就比主屏分辨率大6~8px,不容易出现这个问题。
//     . 无边框的窗口最大化会出现覆盖任务栏变成全屏的问题，因此需要对ptMaxSize作缩进，很容易出现这种问题
//       (测试发现，只有同时具备了WS_CAPTION|WS_MAXIMIZEBOX属性的窗口最大化后，才不会覆盖任务栏）
//  
//    如果我们给出的ptMaxSize（与ptMaxPosition无关）
//    小于主屏分辨率大小时，最大化到副屏的窗口不会自适应,仍然是参数中的ptMaxSize。
//    如果副屏更大的话，就造成最大化不完全的问题。
//
// 4. ptMaxPosition，Windows内部是基于主屏的workarea。当任务栏位于左侧时,ptMaxPostion.x=1会自动调整会
//    任务栏的右侧1px处
//
// 5. ptMaxSize，Windows内部是与主屏的monitorarea比较的。当ptMaxSize大小主屏大小时，Windows会自动
//    将窗口调整为workarea大小。但如果ptMaxSize小于主屏大小时，Windows不做调整
//
LRESULT WindowBase::_OnGetMinMaxInfo( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
    // 对于WindowBase，只对窗口最小值和最大值对处理。
    // 其它逻辑由CustomWindow去处理

	bHandled = TRUE;
	MINMAXINFO* pInfo = (MINMAXINFO*)lParam;

	if (NDEF != m_lMaxWidth)
		pInfo->ptMaxSize.x = pInfo->ptMaxTrackSize.x = m_lMaxWidth;

	if (NDEF != m_lMaxHeight)
		pInfo->ptMaxSize.y = pInfo->ptMaxTrackSize.y = m_lMaxHeight;

	if (NDEF != m_lMinWidth)
		pInfo->ptMinTrackSize.x = m_lMinWidth;

	if (NDEF != m_lMinHeight)
		pInfo->ptMinTrackSize.y = m_lMinHeight;

	return 0;
}

LRESULT WindowBase::_OnWindowPosChanging( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	SyncWindowHelper<WindowBase>::_OnWindowPosChanging((LPWINDOWPOS)lParam, bHandled);
	return 0;
}
LRESULT WindowBase::_OnSyncWindow( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	switch(wParam)
	{
	case ADD_SYNC_WINDOW:
		{
			const SyncWindowData& data = *(SyncWindowData*)(lParam);
			this->AddAnchorItem(data);
		}
		break;
	case MODIFY_SYNC_WINDOW:
		{
			const SyncWindowData& data = *(SyncWindowData*)(lParam);
			this->ModifyAnchorItem(data);
		}
		break;
	case REMOVE_SYNC_WINDOW:
		{
			const SyncWindowData& data = *(SyncWindowData*)(lParam);
			this->RemoveAnchorItem(data.m_hWnd);
		}
		break;
	case ADD_SYNC_WINDOW_RESULT:
		{
			this->OnAddAnchorItem((HWND)lParam);
		}
		break;
	case MODIFY_SYNC_WINDOW_RESULT:
		{
			this->OnModifyAnchorItem((HWND)lParam);
		}
		break;
	case REMOVE_SYNC_WINDOW_RESULT:
		{
			this->OnRemoveAnchorItem((HWND)lParam);
		}
		break;
	case HOST_WINDOW_DESTROYED:
		{
			this->OnRemoveAnchorItem((HWND)lParam);
		}
		break;
	case HOST_WINDOW_POSCHANGING:
		{
			this->OnHostWindowPosChanging();
		}
		break;
	}
	return 0;
}
LRESULT WindowBase::_OnEnterSizeMove( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = FALSE;
	SyncWindowHelper<WindowBase>::_OnEnterSizeMove();

	LRESULT lRet = m_oMouseManager.HandleMouseMessage(uMsg, wParam, lParam, &bHandled);
	if (bHandled)
		return lRet;

	return 0;
}
LRESULT WindowBase::_OnExitSizeMove( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	bHandled = FALSE;
	SyncWindowHelper<WindowBase>::_OnExitSizeMove();

	LRESULT lRet = m_oMouseManager.HandleMouseMessage(uMsg, wParam, lParam, &bHandled);
	if (bHandled)
		return lRet;
	return 0;
}	

LRESULT  WindowBase::_OnGetObject( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	if (!m_hWnd)
		return 0;

	IAccessible *pAccessible = NULL;
	//CreateWindowAccessible(*this, &pAccessible);
	this->CreateAccesible(&pAccessible);
	if (pAccessible)
	{
		LRESULT lres = LresultFromObject(IID_IAccessible, wParam, pAccessible);
		pAccessible->Release();
		return lres;
	}
	return 0;
}

void  WindowBase::OnEraseBkgnd(IRenderTarget* pRenderTarget)
{
	if (NULL == pRenderTarget)
		return;

	if (m_pBkgndRender || m_pForegndRender)
	{
        // 如果m_pBkgndRender没有处理，在这里调用系统过程
		__super::OnEraseBkgnd(pRenderTarget);  
	}
	else
	{
#if 0 // 废弃。由xml去配置吧。在layered下面居然导致alpha通道混乱了

		if (NULL == m_oldWndProc)   // Dialog类型，直接填充系统背景色
		{
			CRect rc;
			::GetClientRect(m_hWnd,&rc);

            Color c(::GetSysColor(COLOR_BTNFACE));
            c.a = 255;
			pRenderTarget->DrawRect(&rc, &c);
		}
		else  // Window类型，直接调用系统过程
		{
			HDC hDC = pRenderTarget->GetHDC();
            if (hDC)
            {
                // 与原始消息进行区分
			    DefWindowProc(WM_ERASEBKGND, (WPARAM)hDC, 1);  
            }
		}

#endif
	}	
}

LRESULT  WindowBase::OnGetMouseKeyboardMgr(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return (LRESULT)static_cast<IMKMgr*>(&m_oMouseManager);
}

//
//	将双缓存数据提交到窗口DC上
//
void WindowBase::CommitDoubleBuffet2Window(HDC hDCWnd, RECT* prcCommit, int nRectCount)
{
	IRenderTarget* pRenderTarget = m_pRenderLayer->GetRenderTarget();

	POINT  ptSrc = {0, 0};
    m_pRenderLayer->GetOffsetDrawInBuffer(&ptSrc);

	HDC hDC = hDCWnd;
	if (NULL == hDC)
    {
        if (!m_hWnd)  // 窗口已销毁了。
            return;

		hDC = GetDC(m_hWnd);
    }
    
	CRect rcCommitTemp;
	if (NULL == prcCommit)
	{
		::GetClientRect(m_hWnd, &rcCommitTemp);
		nRectCount = 1;

        prcCommit = &rcCommitTemp;
	}

    CRect  rcSrc;
//	m_MgrCWBL.DoPre(hDC, pRenderTarget, prcSrc, nRectCount);
	for (int i = 0; i < nRectCount; i++)
	{
		rcSrc.CopyRect(&prcCommit[i]);
        OffsetRect(&rcSrc, ptSrc.x, ptSrc.y);

        Render2TargetParam  param = {0};
        param.xDst = prcCommit[i].left;
        param.yDst = prcCommit[i].top;
        param.wDst = rcSrc.Width();
        param.hDst = rcSrc.Height();
        param.xSrc = rcSrc.left;
        param.ySrc = rcSrc.top;
        param.wSrc = rcSrc.Width();
        param.hSrc = rcSrc.Height();
        param.bAlphaBlend = false;
        param.opacity = 255;

        pRenderTarget->Render2DC(hDC, &param);

#ifdef TRACE_DRAW_PROCESS
        UI_LOG_DEBUG(_T("Commit2Wnd: %d,%d [%d,%d]"),
            prcCommit[i].left, prcCommit[i].top,
            RECTW(prcCommit[i]), RECTH(prcCommit[i]));
#endif
	}
//	m_MgrCWBL.DoPost(hDC, pRenderTarget, prcSrc, nRectCount);

	if (NULL == hDCWnd)
	{
		ReleaseDC(m_hWnd, hDC);
	}
}

void WindowBase::OnGetDesiredSize(SIZE* pSize)
{
    if (NULL == pSize)
        return;

	Panel::OnGetDesiredSize(pSize);

    // 这里获取到的是client大小，需要转换成window大小
    this->CalcWindowSizeByClientSize(*pSize, pSize);
}

//
//	如果已知一个窗口的client区域大小，求这个窗口的window区域大小
//
//	参数
//		sizeClient
//			[in]	该窗口的客户区大小
//		pSizeWindow
//			[out]	返回窗口的window大小
//
void WindowBase::CalcWindowSizeByClientSize( SIZE sizeClient, SIZE* pSizeWindow )
{
	if( NULL == pSizeWindow )
		return;

	CRect  tempRcWindow, tempRcClient;

	NCCALCSIZE_PARAMS     np;
	WINDOWPOS             wp;

	// 通过 WM_NCCALCSIZE 消息，让系统告诉我们一个window rect 对应的client rect是多少
	np.lppos = &wp;
	::GetWindowRect( this->m_hWnd, &np.rgrc[0] );
	tempRcWindow.CopyRect(&np.rgrc[0]);
	::SendMessage( this->m_hWnd, WM_NCCALCSIZE, (WPARAM)FALSE, (LPARAM)&np );
	tempRcClient.CopyRect(&np.rgrc[0] );

	pSizeWindow->cx = sizeClient.cx + tempRcWindow.Width() - tempRcClient.Width();
	pSizeWindow->cy = sizeClient.cy + tempRcWindow.Height() - tempRcClient.Height();
}

//
//	如果已知一个窗口的window区域大小，求这个窗口的client区域大小
//
//	参数
//		rcWindow
//			[in]	该窗口的客大小
//		rcClient
//			[out]	返回窗口的client大小（默认left=0,top=0，即只返回窗口大小）
//
void WindowBase::CalcClientRectByWindowRect( RECT* rcWindow, RECT* rcClient )
{
	NCCALCSIZE_PARAMS     np;
	WINDOWPOS             wp;

	// 通过 WM_NCCALCSIZE 消息，让系统告诉我们一个window rect 对应的client rect是多少
	np.lppos = &wp;
	::CopyRect( &np.rgrc[0], rcWindow );
	::SendMessage( this->m_hWnd, WM_NCCALCSIZE, (WPARAM)FALSE, (LPARAM)&np );

	rcClient->left   = rcClient->top = 0;
	rcClient->right  = np.rgrc[0].right - np.rgrc[0].left;
	rcClient->bottom = np.rgrc[0].bottom - np.rgrc[0].top;
}

BOOL WindowBase::IsChildWindow()
{
	LONG lStyle = ::GetWindowLong(m_hWnd, GWL_STYLE );
	if( lStyle & WS_CHILD )
		return TRUE;
	else
		return FALSE;
}

BOOL  WindowBase::IsWindowVisible()
{
	if (!m_hWnd)
		return FALSE;

	return ::IsWindowVisible(m_hWnd);
}
// CommitWindowBufferListenerMgr&  WindowBase::GetCommitWindowBufferListenerMgr()
// {
// 	return m_MgrCWBL;
// }
// void  WindowBase::AddCommitWindowBufferListener(ICommitWindowBufferListener* p) 
// { 
//     m_MgrCWBL.AddListener(p); 
// }
// void  WindowBase::RemoveCommitWindowBufferListener(ICommitWindowBufferListener* p) 
// { 
//     m_MgrCWBL.RemoveListener(p); 
// }

Object* WindowBase::GetHoverObject()
{
	return m_oMouseManager.GetHoverObject();
}
Object* WindowBase::GetPressObject()
{
	return m_oMouseManager.GetPressObject();
}
void  WindowBase::SetPressObject(Object* p)
{
	m_oMouseManager.SetHoverObject(p);
	m_oMouseManager.SetPressObject(p);
}

void WindowBase::ShowWindow()
{
	::ShowWindow( m_hWnd, SW_SHOW );
}
void WindowBase::HideWindow()
{
	::ShowWindow( m_hWnd, SW_HIDE );
}

void  WindowBase::CenterWindow(HWND hWndCenter)
{
    // Copy  CWindowImpl::CenterWindow()
    CWindow wnd;
    wnd.Attach(m_hWnd);
    wnd.CenterWindow(hWndCenter);
    wnd.Detach();
}

void  WindowBase::CenterWindow(HMONITOR hMonitor)
{
    CRect  rcDlg;
    CRect  rcArea;

    ::GetWindowRect(m_hWnd, &rcDlg);
    
    if (!hMonitor)
    {
        POINT pt = {0};
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    if(hMonitor)
    {
        MONITORINFO  mi = {sizeof(mi)};
        GetMonitorInfo(hMonitor, &mi);
        rcArea.CopyRect(&mi.rcWork);
    }
    else
    {
        ::SystemParametersInfo(SPI_GETWORKAREA, NULL, &rcArea, NULL);
    }

    int DlgWidth = rcDlg.Width();
    int DlgHeight = rcDlg.Height();

    int xLeft = (rcArea.left + rcArea.right) / 2 - DlgWidth / 2;
    int yTop = (rcArea.top + rcArea.bottom) / 2 - DlgHeight / 2;

    // if the dialog is outside the screen, move it inside
    if(xLeft + DlgWidth > rcArea.right)
        xLeft = rcArea.right - DlgWidth;
    if(xLeft < rcArea.left)
        xLeft = rcArea.left;

    if(yTop + DlgHeight > rcArea.bottom)
        yTop = rcArea.bottom - DlgHeight;
    if(yTop < rcArea.top)
        yTop = rcArea.top;

    ::SetWindowPos(m_hWnd, NULL, xLeft, yTop, -1, -1,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// 使用桌面布局重新更新窗口位置大小，主要用于在UIEditor中修改了
// 窗口对象的大小。而使用Object::UpdateLayout仅能重新布局子对象的问题
void  WindowBase::UpdateDesktopLayout()
{
    DesktopLayout dl;
    dl.Arrange(this);
}

void WindowBase::SaveMemBitmap(TCHAR* szFile)
{
	UIASSERT(0 && _T("TODO:"));
	return;
#ifdef _DEBUGx
	if (NULL == m_hMemBitmap)
		return;

	::SelectObject(m_hMemDC, m_hOldBitmap);
	Image image;
	image.Attach(m_hMemBitmap);
//	image.ForceUseAlpha();
	image.Save(szFile, Gdiplus::ImageFormatBMP);
	image.Detach();
	::SelectObject(m_hMemDC, m_hMemBitmap);
#endif
}

// 将内存位图绘图到指定区域
void WindowBase::DrawMemBitmap(HDC hDC, RECT* prc, bool bAlphaBlend)
{
    HDC hMemDC = m_pRenderLayer->GetRenderTarget()->GetHDC();
    POINT  ptOffset;
    m_pRenderLayer->GetOffsetDrawInBuffer(&ptOffset);

	if (bAlphaBlend) 
	{
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(hDC, prc->left, prc->top, RECTW(prc), RECTH(prc), hMemDC, ptOffset.x, ptOffset.y, RECTW(prc), RECTH(prc), bf);
	}
	else
	{
        HRGN hRgn = CreateRectRgn(0,0,0,0);
		int nRet = ::GetWindowRgn(m_hWnd, hRgn);
        if (ERROR == nRet)
        {
            BitBlt(hDC, prc->left, prc->top, RECTW(prc), RECTH(prc), hMemDC, ptOffset.x, ptOffset.y, SRCCOPY);
        }
        else
        {
		    ::SelectClipRgn(hDC, hRgn);
		    ::OffsetClipRgn(hDC, prc->left, prc->top);
    		
            BitBlt(hDC, prc->left, prc->top, RECTW(prc), RECTH(prc), hMemDC, ptOffset.x, ptOffset.y, SRCCOPY);

		    ::SelectClipRgn(hDC, NULL);
        }
        SAFE_DELETE_GDIOBJECT(hRgn);
	}
}

void WindowBase::EnableDwmTransition(bool b)
{
    DwmHelper::GetInstance()->EnableTransition(m_hWnd, b);
}

void WindowBase::SetActive(bool b)
{
	if (b)
		this->setStateBit( WSB_ACTIVE );
	else
		this->clearStateBit( WSB_ACTIVE );
}
bool WindowBase::IsActive()
{
	return this->testStateBit( WSB_ACTIVE );
}

//
// PS: 20130108 已使用CommitWindowBufferListenerMgr来实现
//
// 用于解决EDIT控件中光标在显示的情况下，重新刷新窗口背景将导致
// 光标的显示数据被破坏。最后隐藏光标时，反而将光标显示出来了。
// 因此在EDIT::OnEraseBkgnd中为窗口置上该标识，在WindowBase::CommitDoubleBuffet2Window
// 中检查该标识，如果该标识为true，则先隐藏光标，再提交数据，再将光标显示出来。
//
// void WindowBase::SetCaretBlinking(bool b)
// {
// 	if (b)
// 		this->setStateBit(WSB_CARETBLINKING);
// 	else
// 		this->clearStateBit(WSB_CARETBLINKING);
// }
// bool WindowBase::IsCaretBlinking(bool bClear)
// {
// 	bool b = this->testStateBit(WSB_CARETBLINKING);
// 	
// 	if (b && bClear)
// 		this->clearStateBit(WSB_CARETBLINKING);
// 
// 	return b;
// }


#if 1   // CreateData

CREATE_WND_DATA WindowBase::s_create_wnd_data;
CREATE_WND_DATA::CREATE_WND_DATA()
{
	m_pCreateWndList = NULL;
	::InitializeCriticalSection(&m_cs);
}
CREATE_WND_DATA::~CREATE_WND_DATA()
{
	::DeleteCriticalSection(&m_cs);
}

void  CREATE_WND_DATA::AddCreateWndData(_AtlCreateWndData* pData, void* pThis)
{
	UIASSERT(pData != NULL && pThis != NULL);

	pData->m_pThis = pThis;
	pData->m_dwThreadID = ::GetCurrentThreadId();

	EnterCriticalSection(&m_cs);

	pData->m_pNext = this->m_pCreateWndList;
	this->m_pCreateWndList = pData;

	LeaveCriticalSection(&m_cs);
}
void*  CREATE_WND_DATA::ExtractCreateWndData()
{
	void* pv = NULL;
	EnterCriticalSection(&m_cs);

	_AtlCreateWndData* pEntry = this->m_pCreateWndList;
	if(pEntry != NULL)
	{
		DWORD dwThreadID = ::GetCurrentThreadId();
		_AtlCreateWndData* pPrev = NULL;
		while(pEntry != NULL)
		{
			if(pEntry->m_dwThreadID == dwThreadID)
			{
				if(pPrev == NULL)
					this->m_pCreateWndList = pEntry->m_pNext;
				else
					pPrev->m_pNext = pEntry->m_pNext;
				pv = pEntry->m_pThis;
				break;
			}
			pPrev = pEntry;
			pEntry = pEntry->m_pNext;
		}
	}

	LeaveCriticalSection(&m_cs);
	return pv;
}

void  WindowBase::SetFocusObject(Object* pObj)
{
    m_oMouseManager.SetFocusObject(pObj);
}

// 获取当前鼠标下的对象 
Object*  WindowBase::GetObjectUnderCursor()
{
    POINT  pt;
    GetCursorPos(&pt);
    ::ScreenToClient(m_hWnd, &pt);

    return m_oMouseManager.GetObjectByPos(this, &pt, NULL);
}
Object*  WindowBase::GetObjectByPos(Object* pObjParent, POINT* pt, bool bSkinBuilderInvoke)
{
    if (bSkinBuilderInvoke)
	{
        return m_oMouseManager.GetObjectByPos_UIEditor(pObjParent, pt, NULL);
	}
    else
	{
        return m_oMouseManager.GetObjectByPos(pObjParent, pt, NULL);
	}
}
#endif

// 不负责刷新
void WindowBase::OnSetDefId(IObject* pButton)
{
    if (pButton)
        m_oMouseManager.SetOriginDefaultObject(pButton->GetImpl());
    else
        m_oMouseManager.SetOriginDefaultObject(NULL);
}

IObject* WindowBase::OnGetDefId()
{
    Object* pObj = m_oMouseManager.GetOriginDefaultObject();
    if (pObj)
    {
        return pObj->GetIObject();
    }
    else
    {
        return NULL;
    }
}

IRenderFont*  WindowBase::GetWindowDefaultRenderFont() 
{
	return m_pDefaultFont; 
}

void  WindowBase::SetDefaultRenderFont(LPCTSTR szFontId)
{
	SAFE_RELEASE(m_pDefaultFont);
    if (!szFontId)
        return;

	FontRes* pFontRes = NULL;
	if (m_pSkinRes)
		pFontRes = &m_pSkinRes->GetFontRes();
	else if (m_pUIApplication)
		pFontRes = m_pUIApplication->GetActiveSkinFontRes();

	if (NULL == pFontRes)
		return;

    // 默认，如果传入空字符串，则直接创建默认字体
    if (szFontId[0] == TEXT('\0'))
    {
        pFontRes->GetDefaultFont(m_oWindowRender.GetGraphicsRenderType(), &m_pDefaultFont);
    }
    else
    {
	    pFontRes->GetFont(szFontId, m_oWindowRender.GetGraphicsRenderType(), &m_pDefaultFont);
    }

	// 创建默认字体
	if (NULL == m_pDefaultFont)
	{
		// GDI 窗口字体
		HFONT hFont = (HFONT)::SendMessage(m_hWnd, WM_GETFONT, 0,0);
		if (hFont)
		{
			UI_AttachFont(&m_pDefaultFont, hFont, m_oWindowRender.GetGraphicsRenderType());
		}
		else
		{
			// UI Font Res Defualt Font
			pFontRes->GetDefaultFont(m_oWindowRender.GetGraphicsRenderType(), &m_pDefaultFont);
			if (NULL == m_pDefaultFont)
			{
				// System Default Font
				hFont = (HFONT)GetStockObject(SYSTEM_FONT);
				UI_AttachFont(&m_pDefaultFont, hFont, m_oWindowRender.GetGraphicsRenderType());
			}
		}
	}
}

LPCTSTR  WindowBase::GetDefaultRenderFontId()
{
	if (!m_pDefaultFont)
		return NULL;

	if (m_pDefaultFont->IsAttach())
		return NULL;
	
	FontRes* pFontRes = GetUIApplication()->GetActiveSkinFontRes();
	if (!pFontRes)
		return NULL;

	return pFontRes->GetRenderFontId(m_pDefaultFont);
}

bool  WindowBase::IsDoModal() 
{ 
    return m_bDoModal;
}
HRESULT  WindowBase::SetCanDrop(bool b)
{
    return m_oDragDropManager.SetDroppable(b); 
}

void  WindowBase::SetObjectPos( int x, int y, int cx, int cy, int nFlag)
{
	// 对于窗口来说，这里设置的是非客户区的大小
	::SetWindowPos(m_hWnd, NULL, x, y, cx, cy, SWP_NOZORDER|SWP_NOACTIVATE);
	::GetClientRect(m_hWnd, &m_rcParent);
}

void  WindowBase::virtualSetVisibleEx(VISIBILITY_TYPE eType)
{
	bool bVisibleCompatible = eType==VISIBILITY_VISIBLE ? true:false;
	::ShowWindow(m_hWnd, bVisibleCompatible?SW_SHOW:SW_HIDE);
}

void  WindowBase::virtualSetEnable(bool b)
{
	::EnableWindow(m_hWnd, b ? TRUE:FALSE);
}

bool  WindowBase::IsVisible()
{
	return ::IsWindowVisible(((WindowBase*)this)->m_hWnd)?true:false;
}

bool  WindowBase::IsEnable()
{
	return ::IsWindowEnabled(m_hWnd)?true:false;
}
void  WindowBase::SetHardComposite(bool b)
{
	if (b)
	{
		if (!m_pUIApplication->IsGpuCompositeEnable())
			b = false;
	}

	if (b == m_windowStyle.hard_composite)
		return;

	m_windowStyle.hard_composite = b;
	m_oWindowRender.OnHardCompositeChanged(b);

    UI_LOG_DEBUG(TEXT("hard composite enable, hWnd=0x%08x"), m_hWnd);
}

bool  WindowBase::IsHardComposite()
{
	return m_windowStyle.hard_composite;
}
void  WindowBase::DirectComposite()
{
    m_oWindowRender.HardComposite();
}