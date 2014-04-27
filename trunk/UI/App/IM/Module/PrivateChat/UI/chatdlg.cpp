#include "StdAfx.h"
#include "ChatDlg.h"
#include "UISDK\Control\Inc\Interface\isliderctrl.h"
#include "UISDK\Kernel\Inc\Interface\ipanel.h"
#include "UISDK\Kernel\Inc\Interface\ianimate.h"
#include "UISDK\Control\Inc\Interface\irichedit.h"
#include "privatechatui.h"
#include "UISDK\Control\Inc\Interface\isplitterbar.h"
#include "UISDK\Control\Inc\Interface\icheckbutton.h"
#include "UISDK\Control\Inc\Interface\icombobox.h"
#include "UISDK\Kernel\Inc\Util\ibuffer.h"
#include "UISDK\Control\Inc\Interface\ilistbox.h"

#define FRAME_SHADOW_SIZE  6
#define ANIMATE_ID_RIGHT_PANEL  1
#define RIGHT_PANEL_WIDTH  209

CChatDlg::CChatDlg(void)
{
	m_pRightPanel = NULL;
	m_pLeftPanel = NULL;
	m_pBtnAnimate = NULL;
//	m_pListView = NULL;
    m_pSplitterBar = NULL;
    m_pBtnFont = NULL;
    m_pBtnEmotion = NULL;
    m_pBtnClose = NULL;
    m_pBtnSend = NULL;
    m_pBtnSendMode = NULL;

	m_eRightPanelState = RIGHT_PANEL_STATE_EXPAND;
	m_nRightPanelConfigWidth = 0;
	m_nLeftPanelWidth = 0;

    m_pRichEditInput = m_pRichEditOutput = NULL;
    m_pPanelFontToolbar = NULL;
    m_pBtnFontBold = NULL;
    m_pBtnFontItalic = NULL;
    m_pBtnFontUnderline = NULL;
    m_pComboFontSize = NULL;
    m_pComboFontFace = NULL;
    m_pBtnFontColor = NULL;
}

CChatDlg::~CChatDlg(void)
{

}

void CChatDlg::OnInitWindow()
{
    ICustomWindow::nvProcessMessage(GetCurMsg(), 0, 0);
    this->SetWindowResizeType(WRSB_ALL);

	m_pLeftPanel = (UI::IPanel*)this->FindChildObject(_T("panel_left"));
	m_pRightPanel = (UI::IPanel*)this->FindChildObject(_T("right_panel"));
	m_pBtnAnimate = (UI::IButton*)this->FindChildObject(_T("btn_expand_right_panel"));
    m_pSplitterBar = (UI::ISplitterBar*)this->FindChildObject(_T("splitterbar"));
    m_pPanelFontToolbar = (UI::IPanel*)this->FindChildObject(_T("panel_font"));

    if (m_pSplitterBar)
    {
        m_pBtnFont = (UI::ICheckButton*)m_pSplitterBar->FindChildObject(_T("toolbar_btn_font"));
        m_pBtnEmotion = (UI::IButton*)m_pSplitterBar->FindChildObject(_T("toolbar_btn_face"));
        if (m_pBtnEmotion)
        {
            m_pBtnEmotion->ModifyStyleEx(BUTTON_STYLE_CLICK_ONDOWN, 0, false);
        }
    }

	if (m_pRightPanel)
	{
		m_nRightPanelConfigWidth = m_pRightPanel->GetConfigWidth();
	}
    if (m_pLeftPanel)
    {
        m_pRichEditOutput = (UI::IRichEdit*)m_pLeftPanel->FindChildObject(_T("outputedit"));
        m_pRichEditInput = (UI::IRichEdit*)m_pLeftPanel->FindChildObject(_T("inputedit"));
        m_pBtnClose = (UI::IButton*)m_pLeftPanel->FindChildObject(_T("btnclose"));
        m_pBtnSend = (UI::IButton*)m_pLeftPanel->FindChildObject(_T("btnsend"));
        m_pBtnSendMode = (UI::IButton*)m_pLeftPanel->FindChildObject(_T("btnsendmode"));
    }

    if (m_pPanelFontToolbar)
    {
        m_pBtnFontBold = (UI::ICheckButton*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_btn_font_bold"));
        m_pBtnFontItalic = (UI::ICheckButton*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_btn_font_italic"));
        m_pBtnFontUnderline = (UI::ICheckButton*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_btn_font_underline"));
        m_pComboFontSize = (UI::IComboBox*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_combo_font_size"));
        m_pComboFontFace = (UI::IComboBox*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_combo_font_face"));
        m_pBtnFontColor = (UI::IButton*)m_pPanelFontToolbar->FindChildObject(_T("toolbar_btn_font_color"));
    }

    if (m_pRichEditOutput)
    {
        m_pRichEditOutput->SetReadOnly(true);
        //m_pRichEditOutput->SetMultiLine(false);
        //m_pRichEditOutput->SetAutoResize(true, 200);
    }
    if (m_pRichEditInput)
    {
    }

    if (m_pComboFontSize)
    {
        TCHAR szText[4];
        for (int i = 8; i <= 22; i++)
        {
            _stprintf(szText, _T("%d"), i);
            m_pComboFontSize->AddStringEx(szText)->SetData(i);
        }
        if (m_pRichEditInput)
        {
            CHARFORMAT2 cf;
            memset(&cf, 0, sizeof(cf));
            cf.dwMask = CFM_SIZE|CFM_FACE;
            m_pRichEditInput->GetCharFormat(&cf);

            HWND hWnd = GetDesktopWindow();
            HDC  hDC = GetDC(hWnd);
            LONG yPixPerInch = GetDeviceCaps(hDC, LOGPIXELSY);
            int nSize = abs(UI::Util::FontHeight2Size(cf.yHeight * yPixPerInch / 1440));
            ReleaseDC(hWnd, hDC);

            m_pComboFontSize->SetCurSel(nSize-8);
            if (m_pComboFontFace)
            {
                m_pComboFontFace->AddStringEx(cf.szFaceName);
                m_pComboFontFace->SetCurSel(0);
            }
        }
    }
    

// 	if (m_pRightPanel)
// 	{
// 		m_pListView = (QQContactMgrListView*)m_pRightPanel->FindChildObject(_T("listview"));
// 	}
// 	if (m_pListView)
// 	{
// 		m_pListView->InsertColumn(0, _T("First"), 90);
// 		m_pListView->InsertColumn(1, _T("Second"), 90);
// 
// 		m_pListView->InsertItem(0, _T("航迹云"), LISTCTRL_ITEM_OPERATOR_FLAG_NOALL);
// 		m_pListView->InsertItem(1, _T("leeihcy"), LISTCTRL_ITEM_OPERATOR_FLAG_NOALL);
// 
// 		m_pListView->UpdateItemRect(m_pListView->GetFirstItem(), false);
// 	}

//	SetWindowText(GetHWND(), _T("IM2012_Demo"));
}


void  CChatDlg::SetSkin(UI::IRenderBase* p)
{
    if (NULL == m_pLeftPanel->GetTextureRender())
    {
        m_pLeftPanel->SetTextureRender(p);
    }
}

void  CChatDlg::OnTextureAlphaChanged(int nNewAlpha)
{
//	if (nScrollType == SB_ENDSCROLL)
//	{
//         UI::IImageManager* pImageMgr = NULL;
// 		g_pUIApp->GetActiveSkinImageMgr(&pImageMgr);
// 		if (pImageMgr)
// 			pImageMgr->ModifyImageItemAlpha(_T("chatframe.png"), nPos);
// 
// 		g_pUIApp->RedrawTopWindows();
//	}


#if 0
    UI::IRenderBase* p = m_pLeftPanel->GetForeRender();
    if (NULL == p)
        return;

    UI::IImageRender*  pBkgndRender = (UI::IImageRender*)p->QueryInterface(uiiidof(IImageRender));
    if (NULL == pBkgndRender)
        return;

    pBkgndRender->SetAlpha(nNewAlpha);
#endif
}

void CChatDlg::OnBnClickAnimate()
{
	if (m_eRightPanelState == RIGHT_PANEL_STATE_ANIMATING_EXPAND ||
		m_eRightPanelState == RIGHT_PANEL_STATE_ANIMATING_COLLAPSE )
		return;

    UI::IAnimateManager* pAnimateMgr = GetUIApplication()->GetAnimateMgr();
    UI::IStoryboard*  pStoryboard = pAnimateMgr->CreateStoryboard(this, ANIMATE_ID_RIGHT_PANEL);
    UI::IIntTimeline* pIntTimeline = pStoryboard->CreateIntTimeline(0);

	if (m_eRightPanelState == RIGHT_PANEL_STATE_COLLAPSE)
	{
        pIntTimeline->SetLinerParam1(0, m_nRightPanelConfigWidth, 300);
		m_eRightPanelState = RIGHT_PANEL_STATE_ANIMATING_EXPAND;

        // -- 有bug，SetWindowPos之后会有残留阴影，无法解决，以后再看。
        // 增加窗口大小，用于显示左侧面板
//         if (!::IsZoomed(GetHWND()))
//         {
//             if (m_pLeftPanel)
//             {
//                 m_pLeftPanel->SetConfigRight(RIGHT_PANEL_WIDTH);
//             }
// 
//             CRect rcWindow;
//             ::GetWindowRect(GetHWND(), &rcWindow);
//             rcWindow.right += RIGHT_PANEL_WIDTH;
//             GetRenderChain()->SetCanCommit(false);
//             ::SetWindowPos(GetHWND(), 0, 0, 0, rcWindow.Width(), rcWindow.Height(), SWP_NOZORDER|SWP_NOMOVE);
//             GetRenderChain()->SetCanCommit(true);
//         }
	}
	else
	{
        pIntTimeline->SetLinerParam1(m_nRightPanelConfigWidth, 0, 300);
		m_eRightPanelState = RIGHT_PANEL_STATE_ANIMATING_COLLAPSE;
	}
	if (m_pLeftPanel)
	{
		m_nLeftPanelWidth = m_pLeftPanel->GetWidth();
	}

    pStoryboard->Begin();
}

void CChatDlg::OnAnimateTick(int nCount, UI::IStoryboard** ppArray)
{
	for (int i = 0; i < nCount; i++)
	{
		switch (ppArray[i]->GetId())
		{
		case ANIMATE_ID_RIGHT_PANEL:
			{
				int nWidth = 0;
				ppArray[i]->FindTimeline(0)->GetCurrentValue(&nWidth);
				m_pRightPanel->SetConfigRight(m_nRightPanelConfigWidth-nWidth);
				
				if (::IsZoomed(GetHWND()) && NULL != m_pLeftPanel)
				{ 
					int nRightWidth = nWidth-FRAME_SHADOW_SIZE;  // 6px 阴影
					if (nRightWidth < 0)
						nRightWidth = 0;
					m_pLeftPanel->SetConfigRight(nRightWidth);
				}

				this->UpdateLayout(true);

				if (ppArray[i]->IsFinish())
				{
					if (m_eRightPanelState == RIGHT_PANEL_STATE_ANIMATING_EXPAND)
					{
						m_eRightPanelState = RIGHT_PANEL_STATE_EXPAND;
					}
					else
					{
						m_eRightPanelState = RIGHT_PANEL_STATE_COLLAPSE;

                        // 修改窗口大小，否则在拖拽窗口时预览大小不正确
//                         if (!::IsZoomed(GetHWND()))
//                         {
//                             if (m_pLeftPanel)
//                             {
//                                 m_pLeftPanel->SetConfigRight(0);
//                             }
// 
//                             CRect rcWindow;
//                             ::GetWindowRect(GetHWND(), &rcWindow);
//                             int nWidth = rcWindow.Width() - RIGHT_PANEL_WIDTH;
//                             ::SetWindowPos(GetHWND(), 0, 0, 0, nWidth, rcWindow.Height(), SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOREDRAW);
//                         }
					}
                    
                    // 动画过程中，动画按钮的位置改变了，但hover样式却没有修改。
                    // 再发一个鼠标消息更新hover对象
                    POINT pt;
                    GetCursorPos(&pt);
                    ::ScreenToClient(GetHWND(), &pt);
                    ::PostMessage(GetHWND(), WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));
				}
			}
			break;
		}
	}
}
void CChatDlg::OnSysCommand(UINT nID, CPoint point)
{
	SetMsgHandled(FALSE);
	if (nID == SC_RESTORE && IsZoomed(GetHWND()))
	{
		if (m_pLeftPanel)
		{
			m_pLeftPanel->SetConfigRight(m_nRightPanelConfigWidth-FRAME_SHADOW_SIZE);
		}
	}
	else if (nID == SC_MAXIMIZE)
	{
		if (m_eRightPanelState == RIGHT_PANEL_STATE_COLLAPSE)
		{
			m_pLeftPanel->SetConfigRight(0);
		}
	}
}
void  CChatDlg::OnClose()
{
    SetMsgHandled(TRUE);
    m_pPrivateChatUI->DelayDestroyDlg(this);
}


void  CChatDlg::OnBtnFont()
{
    if (!m_pPanelFontToolbar)
        return;

    m_pPanelFontToolbar->SetVisible(!m_pPanelFontToolbar->IsMySelfVisible(), true, true);
}
void  CChatDlg::OnBtnFontBold()
{
    if (!m_pRichEditInput)
        return;

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_BOLD;

    if (m_pBtnFontBold->IsChecked())
        cf.dwEffects |= CFE_BOLD;
    else
        cf.dwEffects = 0;

    m_pRichEditInput->SetCharFormat(&cf);
}
void  CChatDlg::OnBtnFontItalic()
{
    if (!m_pRichEditInput)
        return;

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_ITALIC;

    if (m_pBtnFontItalic->IsChecked())
        cf.dwEffects |= CFE_ITALIC;
    else
        cf.dwEffects = 0;

    m_pRichEditInput->SetCharFormat(&cf);
}
void  CChatDlg::OnBtnFontUnderline()
{
    if (!m_pRichEditInput)
        return;

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_UNDERLINE;

    if (m_pBtnFontUnderline->IsChecked())
        cf.dwEffects |= CFE_UNDERLINE;
    else
        cf.dwEffects = 0;

    m_pRichEditInput->SetCharFormat(&cf);
}

void  CChatDlg::OnBtnFontColor()
{
    if (!m_pRichEditInput)
        return;

    static COLORREF acrCustClr[16] = {0x00FFFFFF}; // array of custom colors
    static DWORD rgbCurrent = 0;        // initial color selection

    CHOOSECOLOR cc = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = GetHWND();
    cc.lpCustColors = (LPDWORD) acrCustClr;
    cc.rgbResult = rgbCurrent;
    cc.rgbResult = 0;
    cc.Flags = CC_FULLOPEN;

    if (!ChooseColor(&cc)) 
       return;

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = cc.rgbResult;

    m_pRichEditInput->SetCharFormat(&cf);
}

void  CChatDlg::OnComboSizeSelChanged(UI::IListItemBase* pOldItem, UI::IListItemBase* pNewItem)
{
    if (!pNewItem)
        return;

    int nSize = (int)pNewItem->GetData();   
    if (!m_pRichEditInput)
        return;

    CHARFORMAT2 cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE;

    HWND hWnd = GetDesktopWindow();
    HDC  hDC = GetDC(hWnd);
    LONG yPixPerInch = GetDeviceCaps(hDC, LOGPIXELSY);
    cf.yHeight = abs(UI::Util::FontSize2Height(nSize) * 1440 / yPixPerInch);
    ReleaseDC(hWnd, hDC);

    m_pRichEditInput->SetCharFormat(&cf);
}
void  CChatDlg::OnComboFontFaceSelChanged(UI::IListItemBase* pOldItem, UI::IListItemBase* pNewItem)
{

}

void  CChatDlg::OnBtnEmotion()
{
	return;  // richedit的gif线程还有问题，先屏蔽
    if (m_pBtnEmotion->IsForcePress())
        return;

    POINT pt = m_pBtnEmotion->GetRealPosInWindow();
    ::ClientToScreen(GetHWND(), &pt);
    pt.x -= 4;
    m_pPrivateChatUI->ShowEmotionDlg(GetHWND(), pt);
    m_pBtnEmotion->SetForcePress(true, false);
}
LRESULT  CChatDlg::OnEmotionDlgHide(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    m_pBtnEmotion->SetForcePress(false, false);
    m_pBtnEmotion->UpdateObject();
    return 0;
}
LRESULT  CChatDlg::OnInsertEmotion(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!m_pRichEditInput)
        return 0;

    if (1==wParam)
    {
        m_pRichEditInput->InsertSkinGif(_T("chat.emotion.1"));
    }
    else if (2==wParam)
    {
        m_pRichEditInput->InsertSkinGif(_T("chat.emotion.2"));
    }
    else
    {
    //    m_pRichEditInput->InsertGif(L"C:\\aaa.gif");
    }
    return 0;
}
void  CChatDlg::OnBtnClose()
{
    PostMessage(GetHWND(), WM_SYSCOMMAND, SC_CLOSE, 0);
}
void  CChatDlg::OnBtnSend()
{
    if (!m_pRichEditInput || !m_pRichEditOutput)
        return;

    UI::IBuffer* pBuffer = NULL;
    m_pRichEditInput->GetEncodeTextW(&pBuffer);
    if (pBuffer)
    {
        m_pRichEditOutput->AppendEncodeTextW((const TCHAR*)pBuffer->GetBuffer(), pBuffer->GetSize());
        SAFE_RELEASE(pBuffer);
        m_pRichEditInput->SetText(_T(""));
    }

    return;
}
void  CChatDlg::OnBtnSendMode()
{

}