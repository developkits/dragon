#pragma once
#include "chatdlgbase.h"
#include "UISDK\Control\Inc\Interface\isplitterbar.h"

namespace UI
{
    interface  IStoryboard;
    interface  IButton;
    interface  ISliderCtrl;
    interface  IPanel;
    interface  IRichEdit;
    interface  ICheckButton;
    interface  IComboBox;
	interface  IIEWrap;
}
class CPrivateChatUI;

class CChatDlg2 : public CChatDlgBase
{
public:
	CChatDlg2(void);
	~CChatDlg2(void);

	UI_BEGIN_MSG_MAP_Ixxx(CChatDlg2)
        UIMSG_BN_CLICKED2(m_pBtnFont, OnBtnFont)
        UIMSG_BN_CLICKED2(m_pBtnEmotion, OnBtnEmotion)
        UIMSG_BN_CLICKED2(m_pBtnFontBold, OnBtnFontBold)
        UIMSG_BN_CLICKED2(m_pBtnFontItalic, OnBtnFontItalic)
        UIMSG_BN_CLICKED2(m_pBtnFontUnderline, OnBtnFontUnderline)
        UIMSG_BN_CLICKED2(m_pBtnFontColor, OnBtnFontColor)
        UIMSG_CBN_SELCHANGED(m_pComboFontSize, OnComboSizeSelChanged)
        UIMSG_CBN_SELCHANGED(m_pComboFontFace, OnComboFontFaceSelChanged)
        UIMSG_BN_CLICKED2(m_pBtnClose, OnBtnClose)
        UIMSG_BN_CLICKED2(m_pBtnSend, OnBtnSend)
        UIMSG_BN_CLICKED2(m_pBtnSendMode, OnBtnSendMode)
		UIMESSAGE_HANDLER_EX(UI_WM_SPLITTERBAR_POSCHANGED, OnSplitterbarPosChanged)
		UIMSG_WM_SIZE(OnSize)

        UIMESSAGE_HANDLER_EX(WM_EMOTION_DLG_HIDE, OnEmotionDlgHide)
        UIMESSAGE_HANDLER_EX(WM_EMOTION_INSERT, OnInsertEmotion)
		UIMSG_WM_SYSCOMMAND(OnSysCommand)
        UIMSG_WM_CLOSE(OnClose)
        UIMSG_WM_INITIALIZE(OnInitWindow)
    UI_END_MSG_MAP_CHAIN_PARENT(CChatDlgBase)

public:
	void  OnInitWindow();
	void  OnSize(UINT nType, int cx, int cy);
	LRESULT  OnSplitterbarPosChanged(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void  OnTextureAlphaChanged(int nNewAlpha);
    void  OnBtnFont();
    void  OnBtnEmotion();
	void  OnSysCommand(UINT nID, CPoint point);
    void  OnClose();
    void  OnBtnFontBold();
    void  OnBtnFontItalic();
    void  OnBtnFontColor();
    void  OnBtnFontUnderline();
    void  OnBtnClose();
    void  OnBtnSend();
    void  OnBtnSendMode();
    void  OnComboSizeSelChanged(UI::IListItemBase* pOldItem, UI::IListItemBase* pNewItem);
    void  OnComboFontFaceSelChanged(UI::IListItemBase* pOldItem, UI::IListItemBase* pNewItem);
    
    LRESULT  OnEmotionDlgHide(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT  OnInsertEmotion(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void  SetSkin(UI::IRenderBase* p);

	void  AdjustIETransMargins();
private:
    UI::IRichEdit*  m_pRichEditInput;
    UI::IRichEdit*  m_pRichEditOutput;
	UI::IIEWrap*    m_pIEOutput;
    UI::ISplitterBar*  m_pSplitterBar;
    UI::ICheckButton*  m_pBtnFont;
    UI::IButton*   m_pBtnEmotion;

    UI::IButton*   m_pBtnClose;
    UI::IButton*   m_pBtnSend;
    UI::IButton*   m_pBtnSendMode;

    // ����
    UI::IPanel*    m_pPanelFontToolbar;
    UI::ICheckButton*  m_pBtnFontBold;
    UI::ICheckButton*  m_pBtnFontItalic;
    UI::ICheckButton*  m_pBtnFontUnderline;
    UI::IComboBox*     m_pComboFontFace;
    UI::IComboBox*     m_pComboFontSize;
    UI::IButton*       m_pBtnFontColor;
};

