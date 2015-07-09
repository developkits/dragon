#include "stdafx.h"
#include "combobox.h"
#include "UISDK\Control\Inc\Interface\ibutton.h"
#include "UISDK\Control\Src\Renderbase\renderbasedefine.h"
#include "UISDK\Kernel\Inc\Interface\iwindow.h"
#include "UISDK\Kernel\Inc\Interface\icolorrender.h"
#include "UISDK\Control\Src\Control\ListBase\PopupControlWindow\popupcontrolwindow.h"
#include "UISDK\Control\Src\Control\ListBox\listbox.h"
#include "UISDK\Kernel\Inc\Interface\iattribute.h"
#include "UISDK\Control\Src\Control\Edit\edit.h"

#define DEFAULT_COMBOBOX_DROPWND_ID  TEXT("DefaultComboBoxDropWnd")
using namespace UI;

ComboBoxBase::ComboBoxBase()
{
    m_pIComboBoxBase = NULL;
	m_pEdit = NULL;
	m_pButton = NULL;
	
    m_pPopupWnd = NULL;
	m_bEditDropList = true;
	m_strDropWndId = DEFAULT_COMBOBOX_DROPWND_ID;
}

ComboBoxBase::~ComboBoxBase()
{
	m_pEdit = NULL;     // 由父Obj释放
	m_pButton = NULL;   // 由父Obj释放

    destroyDropWnd();
}

HRESULT  ComboBoxBase::FinalConstruct(IUIApplication* p)
{
	HRESULT hr = DO_PARENT_PROCESS3(IComboBoxBase, IControl);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

void  ComboBoxBase::OnIntialize()
{
    DO_PARENT_PROCESS(IComboBoxBase, IControl);

    if (!m_pEdit)
    {
        m_pEdit = (IEdit*)m_pIComboBoxBase->FindChildObject(
            COMBOBOX_EDIT_ID);
    }
    if (!m_pButton)
    {
        m_pButton = (IButton*)m_pIComboBoxBase->FindChildObject(
            COMBOBOX_BUTTON_ID);
    }
    
    IUIApplication* pUIApp = m_pIComboBoxBase->GetUIApplication();
    if (!m_pEdit)
    {
        IEdit::CreateInstance(pUIApp, &m_pEdit);
        m_pEdit->SetId(COMBOBOX_EDIT_ID);
        m_pIComboBoxBase->AddChild(m_pEdit);

        ObjStyle s = {0};
        s.transparent = 1;
        m_pEdit->ModifyObjectStyle(&s, 0);
    }

    if (!m_pButton)
    {
        IButton::CreateInstance(pUIApp, &m_pButton);
        m_pButton->SetId(COMBOBOX_BUTTON_ID);
        m_pIComboBoxBase->AddChild(m_pButton);

        m_pButton->SetDrawFocusType(
            BUTTON_RENDER_DRAW_FOCUS_TYPE_NONE);
        m_pButton->SetAutoSizeType(BUTTON_AUTOSIZE_TYPE_BKIMAGE);
    }


    // 修改一些默认属性
    m_pEdit->GetImpl()->SetEditType(EDIT_STYLE_COMBOBOX);
    {
        ObjStyle s = {0};
        s.transparent = 1;
        m_pEdit->ModifyObjectStyle(&s, 0);
    }

    m_pButton->SetTabstop(false);

    m_pButton->ClearNotify();
    m_pButton->AddHook(m_pIComboBoxBase, 0, 
            COMBOBOX_BUTTON_MSG_HOOK_MSG_ID);

    m_pEdit->ClearNotify();
    m_pEdit->AddHook(m_pIComboBoxBase, 0, 
            COMBOBOX_EDIT_MSG_HOOK_MSG_ID);
    m_pEdit->SetNotify(m_pIComboBoxBase, 
            COMBOBOX_EDIT_MSG_HOOK_MSG_ID);

	OnSize(0, m_pIComboBoxBase->GetWidth(), m_pIComboBoxBase->GetHeight());
}

void  ComboBoxBase::FinalRelease()
{
    SetMsgHandled(FALSE);

    if (m_pEdit)
    {
        m_pEdit->RemoveHook(m_pIComboBoxBase);
    }
    if (m_pButton)
    {
        m_pButton->RemoveHook(m_pIComboBoxBase);
    }
}

void  ComboBoxBase::OnCreateByEditor(CREATEBYEDITORDATA* pData)
{
    SetMsgHandled(FALSE);
    pData->rcInitPos.right = pData->rcInitPos.left + 100;
    pData->rcInitPos.bottom = pData->rcInitPos.top + 25;

    EditorAddObjectResData addResData = {0};
    addResData.pUIApp = pData->pUIApp;
    addResData.pParentObj = m_pIComboBoxBase;
    addResData.pParentXml = pData->pXml;

    addResData.objiid = uiiidof(IEdit);
    addResData.ppCreateObj = (void**)&m_pEdit;
	addResData.szId = COMBOBOX_EDIT_ID;
    pData->pEditor->AddObjectRes(&addResData);

    addResData.objiid = uiiidof(IButton);
    addResData.ppCreateObj = (void**)&m_pButton;
	addResData.szId = COMBOBOX_BUTTON_ID;
    pData->pEditor->AddObjectRes(&addResData);
}

void  ComboBoxBase::OnSerialize(SERIALIZEDATA* pData)
{
//    bool bBorderConfiged = pMapAttrib->HasAttrib(XML_BORDER);
	DO_PARENT_PROCESS(IComboBoxBase, IControl);

    {
    AttributeSerializerWrap as(pData, TEXT("ComboBoxBase"));
    as.AddString(XML_COMBOBOX_DROPWNDID, this,
        memfun_cast<pfnStringSetter>(&ComboBoxBase::SetDropWndId),
        memfun_cast<pfnStringGetter>(&ComboBoxBase::GetDropWndId));
    }

#if 0
    IUIApplication* pUIApplication = m_pIComboBoxBase->GetUIApplication();
    if (m_pEdit)
        m_pEdit->SetAttributeByPrefix(XML_COMBOBOX_EDIT_PRIFIX, pMapAttrib, bReload, true);
    if (m_pButton)
        m_pButton->SetAttributeByPrefix(XML_COMBOBOX_BUTTON_PRIFIX, pMapAttrib, bReload, true);

    ITextRenderBase*  pTextRenderBase = NULL;
    pMapAttrib->GetAttr_TextRenderBase(NULL, XML_TEXTRENDER_TYPE, true, pUIApplication, m_pIComboBoxBase, &pTextRenderBase);
    if (pTextRenderBase)
    {
        m_pIComboBoxBase->SetTextRender(pTextRenderBase);
        SAFE_RELEASE(pTextRenderBase);
    }

    if (NULL == m_pIComboBoxBase->GetTextRender())
    {
        pUIApplication->CreateTextRenderBase(
                TEXTRENDER_TYPE_SIMPLE, 
                m_pIComboBoxBase, 
                &pTextRenderBase);

        if (pTextRenderBase)
        {

            SERIALIZEDATA data = {0};
            data.pUIApplication = pUIApplication;
            data.pMapAttrib = pMapAttrib;
            data.szPrefix = NULL;
            data.nFlags = SERIALIZEFLAG_LOAD|
                    SERIALIZEFLAG_LOAD_ERASEATTR;
            pTextRenderBase->Serialize(&data);

            m_pIComboBoxBase->SetTextRender(pTextRenderBase);
            pTextRenderBase->Release();
        }
    }

	// 背景绘制 
	if (NULL == m_pIComboBoxBase->GetBkRender())
	{
        IRenderBase*  pRenderBase = NULL;
        pUIApplication->CreateRenderBase(
                RENDER_TYPE_THEME_COMBOBOX_BKGND, 
                m_pIComboBoxBase, 
                &pRenderBase);

        if (pRenderBase)
        {
            m_pIComboBoxBase->SetBkgndRender(pRenderBase);
			ObjStyle s = {0};
			s.transparent = 1;
		    m_pIComboBoxBase->ModifyObjectStyle(&s, 0);
            SAFE_RELEASE(pRenderBase);

            if (!bBorderConfiged)
            {
                CRegion4 r (2,0,0,0);  // 为Edit留两个像素
                m_pIComboBoxBase->SetBorderRegion(&r);
            }
        }
	}

	if (NULL == m_pButton->GetBkRender()) 
	{
        IRenderBase*  pRenderBase = NULL;
        pUIApplication->CreateRenderBase(
                RENDER_TYPE_THEME_COMBOBOX_BUTTON, 
                m_pButton, &pRenderBase);
        if (pRenderBase)
        {
            m_pButton->SetBkgndRender(pRenderBase);
			ObjStyle s = {0};
			s.transparent = 1;
            m_pButton->ModifyObjectStyle(&s, 0);
            SAFE_RELEASE(pRenderBase);
        }
	}
#endif
}

void  ComboBoxBase::SetDropWndId(LPCTSTR szId)
{
    if (!szId || 0 == szId[0])
        m_strDropWndId = DEFAULT_COMBOBOX_DROPWND_ID;
    else
        SETSTRING(m_strDropWndId, szId);
}
LPCTSTR  ComboBoxBase::GetDropWndId()
{
    return m_strDropWndId.c_str();
}

void  ComboBoxBase::GetAutoSize(SIZE* pSize)
{
    SIZE s1 = m_pEdit->GetDesiredSize();
	SIZE s2 = m_pButton->GetDesiredSize();

	pSize->cx = s1.cx + s2.cx;
    pSize->cy = max(s1.cy,s2.cy);
}

void ComboBoxBase::OnSize(UINT nType, int cx, int cy)
{
    SetMsgHandled(FALSE);

	CRect  rcClient;
	m_pIComboBoxBase->GetObjectClientRect(&rcClient);

	int nBtnW = 0;
    CRegion4 rcMarginBtn(0, 0, 0, 0);

    if (m_pButton)
    {
        nBtnW = m_pButton->GetWidth();
        if (0 == nBtnW)
        {
            SIZE s = m_pButton->GetDesiredSize();
            nBtnW = s.cx;
        }

        m_pButton->GetMarginRegion(&rcMarginBtn);
        m_pButton->SetObjectPos(
            rcClient.right - nBtnW - rcMarginBtn.right,
            rcClient.top + rcMarginBtn.top, 
            nBtnW,
            rcClient.Height() - rcMarginBtn.Height(),
            SWP_NOREDRAW);
    }

    if (m_pEdit)
    {
	    CRegion4 rcMarginEdit;
	    m_pEdit->GetMarginRegion(&rcMarginEdit);
	    m_pEdit->SetObjectPos(
		    rcClient.left + rcMarginEdit.left,
		    rcClient.top + rcMarginEdit.top, 
		    rcClient.Width()
                - nBtnW 
                - rcMarginEdit.left 
                - rcMarginEdit.right 
                - rcMarginBtn.left 
                - rcMarginBtn.right,
		    rcClient.Height() 
                - rcMarginEdit.top 
                - rcMarginEdit.bottom, 
		    SWP_NOREDRAW);
    }
}

bool  ComboBoxBase::IsDropDown()
{
	if (!m_pPopupWnd)
		return false;

	return m_pPopupWnd->IsVisible() ? true:false;
}

void ComboBoxBase::OnEraseBkgnd(IRenderTarget* pRenderTarget)
{
    IRenderBase* pRenderBase = m_pIComboBoxBase->GetBkRender();
    bool  bReadOnly = !m_bEditDropList;

	if (pRenderBase)
	{
		CRect rc(0,0, m_pIComboBoxBase->GetWidth(), m_pIComboBoxBase->GetHeight());

		if (!m_pIComboBoxBase->IsEnable())
		{
			pRenderBase->DrawState(pRenderTarget, &rc, bReadOnly?COMBOBOX_BKGND_RENDER_STATE_READONLY_DISABLE:COMBOBOX_BKGND_RENDER_STATE_DISABLE);
		}
		else if (m_pIComboBoxBase->IsPress() || IsDropDown() /*|| m_pButton->IsPress() || m_pEdit->IsPress()*/)
		{
			pRenderBase->DrawState(pRenderTarget, &rc, bReadOnly?COMBOBOX_BKGND_RENDER_STATE_READONLY_PRESS:COMBOBOX_BKGND_RENDER_STATE_PRESS);
		}
		else if (m_pIComboBoxBase->IsHover() || m_pEdit->IsFocus() /*|| m_pButton->IsHover() || m_pEdit->IsHover()*/)
		{
			pRenderBase->DrawState(pRenderTarget, &rc, bReadOnly?COMBOBOX_BKGND_RENDER_STATE_READONLY_HOVER:COMBOBOX_BKGND_RENDER_STATE_HOVER);
		}
		else
		{
			pRenderBase->DrawState(pRenderTarget, &rc, bReadOnly?COMBOBOX_BKGND_RENDER_STATE_READONLY_NORMAL:COMBOBOX_BKGND_RENDER_STATE_NORMAL);
		}
	}
}

// 避免鼠标从COMBOBOX移动到EDIT/BUTTON上时刷新为normal状态
void ComboBoxBase::OnStateChanged(UINT nMask)
{
//	this->UpdateObject();

	UIMSG msg;
	msg.message = UI_WM_REDRAWOBJECT;
	msg.pMsgTo = m_pIComboBoxBase;
	UIPostMessage(m_pIComboBoxBase->GetUIApplication(), &msg);
}

// 使用
void ComboBoxBase::OnChildObjStateChanged(UINT nMask)
{
	SetMsgHandled(FALSE);  // 设置为TRUE会导致子控件刷新不正常，如EDIT不显示光标
	
    IObject* pObj = static_cast<IObject*>(GetCurMsg()->pMsgTo);
    if (nMask & (OSB_HOVER | OSB_FORCEHOVER | OSB_PRESS | OSB_FORCEPRESS | OSB_FOCUS))
    {
        m_pIComboBoxBase->SetForceHover(pObj->IsHover()||pObj->IsPress() || pObj->IsFocus(), false);

        // 因为子控件现在被HOOK掉没刷新了，由Combobox来刷新
        UIMSG msg;
        msg.pMsgFrom = m_pIComboBoxBase;
        msg.pMsgTo = m_pIComboBoxBase;
        msg.message = UI_WM_STATECHANGED2;
        msg.wParam = OSB_FORCEHOVER;
        UIPostMessage(m_pIComboBoxBase->GetUIApplication(), &msg);  // 有可能是从子对象上移到COMBOBOX，因此要延迟刷新

        SetMsgHandled(TRUE);
    }
}

LRESULT  ComboBoxBase::OnEditKeyDown(WPARAM wParam, LPARAM)
{
	if (!m_bEditDropList)
		return UI::NOT_HANDLED;

	UIMSG  msg;
	msg.message = UI_WM_NOTIFY;
	msg.nCode = UI_EN_KEYDOWN;
	msg.wParam = wParam;
	msg.pMsgFrom = m_pIComboBoxBase;
	return m_pIComboBoxBase->DoNotify(&msg);
}


LPCTSTR  ComboBoxBase::GetText()
{
    if (m_pEdit)
	    return m_pEdit->GetText();

	return NULL;
}

// 外部主动修改显示文本时，也需要同步当前列表选项
void  ComboBoxBase::SetText(LPCTSTR szText, bool bUpdate)
{
	if (m_pEdit)
		m_pEdit->SetTextNoFilter(szText, false);

	// 同步当前选中项
	UISendMessage(m_pIComboBoxBase, COMBOBOX_MSG_SETTEXT, (WPARAM)szText);

    if (bUpdate)
    {
        m_pIComboBoxBase->UpdateObject();
    }
}

void  ComboBoxBase::SetReadOnly(bool b)
{
    m_bEditDropList = !b;

    if (m_pEdit)
    {
        m_pEdit->SetVisible(m_bEditDropList,false);
        m_pEdit->SetReadOnly(b, false);
    }
}

bool  ComboBoxBase::IsReadOnly()
{
    return !m_bEditDropList;
}

void ComboBoxBase::OnRedrawObject()
{
	m_pIComboBoxBase->UpdateObject();
}

void ComboBoxBase::OnLButtonDown(UINT nFlags, POINT point)
{
	if (!m_bEditDropList)
	{
		if (m_pIComboBoxBase->IsForcePress())
			return;

        DropDown();
	}
}
void ComboBoxBase::OnBtnLButtonDown(UINT nFlags, POINT point)
{
	if (m_pButton->IsForcePress())
		return;
    
	DropDown();
}

void ComboBoxBase::OnStyleChanged(int nStyleType, LPSTYLESTRUCT lpStyleStruct)
{
	SetMsgHandled(FALSE);
}

// PopupListBoxWindow显示/销毁时，发送过来的消息
void ComboBoxBase::OnInitPopupControlWindow()
{
	SetMsgHandled(FALSE);
}
void ComboBoxBase::OnUnInitPopupControlWindow()
{
    SetMsgHandled(FALSE);

	if (m_bEditDropList)
	{
        m_pButton->SetForcePress(false);  		
	}
	else
	{
        m_pIComboBoxBase->SetForcePress(false);   
        m_pIComboBoxBase->UpdateObject();
	}
}


PopupListBoxWindow*  ComboBoxBase::GetDropWnd()
{
	if (m_pPopupWnd)
		return m_pPopupWnd;

	if (m_strDropWndId.empty())
		return NULL;

	do 
	{
		PopupListBoxWindow::CreateInstance(m_pIComboBoxBase->GetUIApplication(), &m_pPopupWnd);

		// 指定一个父窗口，而不是采用置顶的方式。避免在EDIT输入中文时，listbox遮挡了输入法窗口
		if (!m_pPopupWnd->Create(m_strDropWndId.c_str(), m_pIComboBoxBase->GetHWND()))
			break;

		m_pPopupWnd->SetBindObject(m_pIComboBoxBase);
		m_pPopupWnd->SetContentObject(NULL);

		return m_pPopupWnd;
	} while (0);

	destroyDropWnd();
	return NULL;
}

void  ComboBoxBase::destroyDropWnd()
{
	if (m_pPopupWnd)
	{
		m_pPopupWnd->Destroy();
		SAFE_DELETE_Ixxx(m_pPopupWnd);
	}	
}

IObject*  ComboBoxBase::GetContentObject(LPCTSTR szText)
{
	if (!szText)
		return NULL;

	PopupListBoxWindow* pDropWnd = GetDropWnd();
	if (!pDropWnd)
		return NULL;

	IObject* pObj = pDropWnd->FindChildObject(szText);
	pDropWnd->SetContentObject(pObj);

	return pObj;
}

void ComboBoxBase::CloseUp()
{
    UIMSG  msg;
    msg.message = UI_WM_NOTIFY;
    msg.nCode = UI_CBN_CLOSEUP;
    msg.pMsgTo = m_pIComboBoxBase;
    msg.pMsgFrom = m_pIComboBoxBase;

    BOOL bHandle = 0;
    bHandle = UISendMessage(&msg);

	if (!bHandle)
	{
		UIMSG  msg;
		msg.message = UI_WM_NOTIFY;
		msg.nCode = UI_CBN_CLOSEUP;
		msg.pMsgFrom = m_pIComboBoxBase;
		bHandle = m_pIComboBoxBase->DoNotify(&msg);
	}

	if (!bHandle)
	{
		PopupListBoxWindow* pDropWnd = GetDropWnd();
		if (pDropWnd)
			pDropWnd->Hide();
	}
}


void  ComboBoxBase::DropDown()
{
	UIMSG  msg;
	msg.message = UI_WM_NOTIFY;
	msg.nCode = UI_CBN_DROPDOWN;
	msg.pMsgTo = m_pIComboBoxBase;
	msg.pMsgFrom = m_pIComboBoxBase;

	BOOL bHandle = 0;
	bHandle = (BOOL)UISendMessage(&msg);

	// 派生类没有处理，则交给外部处理
	if (!bHandle)
	{
		msg.pMsgTo = NULL;
		msg.bHandled = FALSE;
		bHandle = (BOOL)m_pIComboBoxBase->DoNotify(&msg);
	}
	if (!bHandle)
	{
		bHandle = defaultDropDown()?TRUE:FALSE;
	}

	if (bHandle)
	{
		if (m_bEditDropList)
		{
			m_pButton->SetForcePress(true);
		}
		if (m_bEditDropList && m_pEdit) // 可编辑，将焦点设置给编辑框
		{
			m_pEdit->SetFocusInWindow();
		}
	}
}

bool ComboBoxBase::defaultDropDown()
{
	PopupListBoxWindow*  pDropWnd = GetDropWnd();
	if (!pDropWnd)
		return false;

	HWND hWnd = m_pIComboBoxBase->GetHWND();
	CRect rcWindow;
	m_pIComboBoxBase->GetWindowRect(&rcWindow);

	POINT pt = {rcWindow.left, rcWindow.bottom};
	::MapWindowPoints(hWnd, NULL, (LPPOINT)&pt, 1);

	SIZE size = {rcWindow.Width(), 20};
	SIZE sDesired = pDropWnd->GetDesiredSize(); 
	if (sDesired.cy <= 0)
		return false;
	
// 	if (sDesired.cx > size.cx)
// 		size.cx = sDesired.cx;

	size.cy = sDesired.cy;

	// 	if (m_pButton && m_pButton->IsSelfVisible())
	// 	{
	// 		CRect rcClickFrom;
	// 		m_pButton->GetWindowRect(&rcClickFrom);
	// 		m_pPopupWrapWnd->SetPopupFromInfo(hWnd, &rcClickFrom);
	// 	}
	// 	else
	{
		pDropWnd->SetPopupFromInfo(hWnd, &rcWindow);
	}


	pDropWnd->Show(pt, size, FALSE, SW_SHOWNOACTIVATE);  // <-- 这里不要用模态的，否则直接点击窗口关闭按钮时会导致崩溃

	return true;
}
//////////////////////////////////////////////////////////////////////////

ComboBox::ComboBox()
{
    m_pIComboBox = NULL; 
    m_pListCtrl = NULL; 
}

void  ComboBox::FinalRelease()
{
    SetMsgHandled(FALSE);
}

HRESULT  ComboBox::OnDropDown(WPARAM, LPARAM)
{
    if (!m_pListCtrl)
        return HANDLED;

    if (!m_pListCtrl->GetFirstItem())
        return HANDLED;

    SetMsgHandled(FALSE);
    return NOT_HANDLED;
}

LRESULT  ComboBox::OnSetText(UINT, WPARAM wParam, LPARAM)
{
	IListBox*  pListBox = GetListBox();
	if (!pListBox)
		return 0;

	LPCTSTR szText = (LPCTSTR)wParam;
	if (!szText)
	{
		pListBox->ClearSelectItem(true);
		return 0;
	}

	IListItemBase* pMatchItem = pListBox->FindItemByText(szText);
	if (pMatchItem)
	{
		// 这里不再notify，因为这可能是用户主观调用的settext，再notify sel changed
		// 就会出现逻辑重复
		pListBox->SelectItem(pMatchItem, false, false);
	}
	else
	{
		pListBox->ClearSelectItem(true);
	}
	return 0;
}

bool  ComboBox::AddString(LPCTSTR szText, int nAddItemFlags)
{ 
    if (this->AddStringEx(szText, nAddItemFlags))
        return true;

    return false;
}
IListBoxItem* ComboBox::AddStringEx(LPCTSTR szText, int nAddItemFlags)
{
	IListBox* pListBox = GetListBox();
	if (!pListBox)
		return NULL;

    IListBoxItem* p = pListBox->AddString(szText, nAddItemFlags);
    return static_cast<IListBoxItem*>(p);
}

bool  ComboBox::SetCurSel(int nIndex)
{
	IListBox* pListBox = GetListBox();
	if (!pListBox)
		return false;

    return pListBox->SetSel(nIndex);
}

void  ComboBox::SetAttribute(IMapAttribute* pMapAttrib, bool bReload)
{
	DO_PARENT_PROCESS(IComboBox, IComboBoxBase);

    if (!bReload)
    {
        // 这个要放在设置完EDIT/BUTTON后面，否则EDIT在readonly下面又会被显示出来
        bool bReadonly = false;
        if (MAPATTR_RET_NOT_EXIST != pMapAttrib->GetAttr_bool(XML_READONLY, true, &bReadonly))
        {
            SetReadOnly(bReadonly);
        }
    }
}

void  ComboBox::OnPaint(IRenderTarget* pRenderTarget)
{
    if (m_bEditDropList)
		return;

	ITextRenderBase*  pTextRender = m_pIComboBox->GetTextRender();
	if (m_pEdit && pTextRender)
	{
		CRect rc;
		m_pIComboBox->GetObjectClientRect(&rc);
		if (m_pButton && m_pButton->IsSelfVisible())
		{
			RECT rcBtn;
			m_pButton->GetParentRect(&rcBtn);
			rc.right = rcBtn.left;
		}
		pTextRender->DrawState(pRenderTarget, &rc, 0, m_pEdit->GetText());
	}
}


void ComboBox::OnLCNSelChanged(IMessage* pObjMsgFrom, IListItemBase* pOldSelItem, IListItemBase* pSelItem)
{
    if (pSelItem)
    {
		// 忽略edit过滤器。例如edit只能输入数字时，也允许主动调用settext设置其它文本
        m_pEdit->SetTextNoFilter(pSelItem->GetText(), false);
        m_pEdit->SetSel(0,-1);   // 全选
    }

    UIMSG   msg;
    msg.message = UI_WM_NOTIFY;
    msg.nCode    = UI_CBN_SELCHANGED;
    msg.wParam  = (WPARAM)pOldSelItem;
    msg.lParam  = (LPARAM)pSelItem;
    msg.pMsgFrom = m_pIComboBox;
    m_pIComboBox->DoNotify( &msg );
}

LRESULT  ComboBox::OnListCtrlClick(WPARAM, LPARAM)
{
    this->CloseUp();
    return 0;
}

LRESULT  ComboBox::OnListCtrlKeyDown(WPARAM vKey, LPARAM)
{
	IListBox* pListBox = GetListBox();
	if (!pListBox)
		return NOT_HANDLED;

	if (vKey == VK_RETURN)
	{
		IListItemBase* pItemHover = pListBox->GetHoverItem();
		IListItemBase* pItemSelect = pListBox->GetFirstSelectItem();

		if (pItemHover && pItemHover!=pItemSelect)
		{
			pListBox->SelectItem(pItemHover, false);
		}
		
		this->CloseUp();
		return HANDLED;
	}
	return NOT_HANDLED;
}

void  ComboBox::ResetContent()
{
	IListBox* pListBox = GetListBox();
	if (pListBox)
		pListBox->RemoveAllItem(LF_NONE);
}
void ComboBox::OnInitPopupControlWindow()
{
    SetMsgHandled(FALSE);

	IListBox* pListBox = GetListBox();
	if (!pListBox)
		return;

    // 下拉后当前项可见
	pListBox->SetHoverItem(NULL);  // 删除上一次打开时的hover项，直接高亮当前选中项
	IListItemBase* pSel = pListBox->GetFirstSelectItem();
	if (pSel)
	{
		pListBox->MakeItemVisible(pSel, NULL);
	}
}


void ComboBox::OnUnInitPopupControlWindow()
{
	SetMsgHandled(FALSE);
	IListBox* pListBox = GetListBox();
	if (!pListBox)
		return;

	pListBox->SetHoverItem(NULL);  // 清空被选中的item，以防下次再次弹出时，状态未更新，仍然显示上一次的hover item。（弹出窗口向listbox获取,listbox向combobox获取,combobox其实是向它所在的窗口获取）
}

IListBox*  ComboBox::GetListBox() 
{
	if (m_pListCtrl)
		return m_pListCtrl;

#define DefaultListBoxId  TEXT("")
	IObject* pObj = this->GetContentObject(DefaultListBoxId);
	if (!pObj)
		return NULL;

	m_pListCtrl = (IListBox*)pObj->QueryInterface(uiiidof(IListBox));
	if (!m_pListCtrl)
		return NULL;

	ListCtrlStyle s = {0};
	s.popuplistbox = 1;
	m_pListCtrl->ModifyListCtrlStyle(&s, 0);
	m_pListCtrl->SetBindObject(m_pIComboBox);
	m_pListCtrl->AddHook(m_pIComboBox, 0, COMBOBOX_LISTBOX_MSG_HOOK_MSG_ID);
	m_pListCtrl->SetNotify(m_pIComboBox, COMBOBOX_LISTBOX_MSG_HOOK_MSG_ID);

	// 设置一个最大高度
	if (NDEF == m_pListCtrl->GetMaxHeight())
		m_pListCtrl->SetMaxHeight(200); 

	return m_pListCtrl;
}


LRESULT  ComboBox::OnEditChanged(WPARAM wParam, LPARAM)
{
	SetMsgHandled(FALSE);

	if (!m_bEditDropList)
		return 0;

	bool bSetText = wParam?true:false;
	if (bSetText)
		return 0;

	// 用户手动输入内容，将下拉列表的内容清空
	IListBox*  pListBox = GetListBox();
	if (pListBox)
	{
		pListBox->ClearSelectItem(false);
	}
	return 0;
}
