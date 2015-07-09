#pragma once
#include "UISDK\Control\Inc\Interface\imenu.h"

//
// 2015.2.22 ��д�˵��봰�ڵ��߼����ɴ����������˵��������ǲ˵��д�������
//
// �����˵���Ƥ�����������ڵ�Ƥ��Ҳһ����
/* Ĭ��Ƥ����Ҫ��xml���������ã�
     <CustomWindow id="DefaultMenu">
 	    <Menu/>
     </CustomWindow>
*/
namespace UI
{
class PopupMenuWindow;

class Menu : public MessageProxy
{
public:
    Menu();
    ~Menu();

    UI_DECLARE_OBJECT(Menu, OBJ_CONTROL|CONTROL_MENU)

    UI_BEGIN_MSG_MAP
        UIMSG_WM_NOTIFY(UI_LCN_HOVERITEMCHANGED, OnHoverItemChanged)
        UIMSG_WM_KEYDOWN(OnKeyDown)
		UIMSG_WM_KEYUP(OnKeyUp)
        UIMSG_WM_TIMER(OnTimer)
        UIMSG_WM_LBUTTONUP(OnLButtonUp)
        UIMSG_WM_MOUSEMOVE(OnMouseMove)
        UIMSG_WM_MOUSELEAVE(OnMouseLeave)
        UIMSG_WM_LBUTTONDOWN(OnLButtonDown)

        UIMESSAGE_HANDLER_EX(UI_WM_MENU_GETICONGUTTERWIDTH, OnGetIconGutterWidth);
        UIMESSAGE_HANDLER_EX(UI_WM_MENU_GETPOPUPTRIANGLEWIDTH, OnGetPopupTriangleWidth)
        UIMSG_WM_INITPOPUPCONTROLWINDOW(OnInitPopupControlWindow)
        UIMSG_WM_UNINITPOPUPCONTROLWINDOW(OnUnInitPopupControlWindow)
        UIMSG_WM_THEMECHANGED(OnThemeChanged)
        UIMSG_WM_GETOBJECTINFO(OnGetObjectInfo)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_RESETATTRIBUTE(ResetAttribute)
        UIMSG_WM_SETATTRIBUTE(SetAttribute)
        UIMSG_WM_FINALCONSTRUCT(FinalConstruct)
        UIMSG_WM_FINALRELEASE(FinalRelease)

    UI_END_MSG_MAP_CHAIN_PARENT_Ixxx(Menu, IListCtrlBase)

public:
    static PARSE_CONTROL_RETURN UIParseMenuTag(IUIElement*, IUIApplication*  pUIApp, IObject* pParent, IObject** ppOut);
    void  OnNewChildElement(IUIElement* pUIElement);

public:
    void  SetIMenu(IMenu* p) { m_pIMenu = p; SetIMessageProxy(static_cast<IMessage*>(p)); }
    IMenu*  GetIMenu() { return m_pIMenu; }

    IWindow*  CreateMenuWindow();
    int    TrackPopupMenu(UINT nFlag, int x, int y, IMessage* pNotifyObj, HWND hWndClickFrom = NULL, RECT* prcClickFrom = NULL);
    IMenuStringItem*  AppendString(LPCTSTR szText, UINT nId);
    IMenuSeparatorItem*  AppendSeparator(UINT nId);
    IMenuPopupItem*  AppendPopup(LPCTSTR szText, UINT nId, IMenu* pSubMenu);

    IMenu*  GetSubMenuByPos(int nPos);
    IMenu*  GetSubMenuById(long lId);
    IListItemBase*  GetMenuItemBySubMenu(IMenu* pSubMenu);

    Menu*  GetPrevMenu() { return m_pPrevMenu; }
    Menu*  GetNextMenu() { return m_pNextMenu; }
    Menu*  GetRootMenu();
    Menu*  GetLastMenu();
    Menu*  GetMenuByHWND(HWND hWnd);
    Menu*  GetMenuByPos(const POINT& ptScreen);
    HWND   GetPopupWindowHandle();
	IWindow*  GetPopupWindow();
	void   SetPopupWindow(PopupMenuWindow* pWindow);
    int    GetIconGutterWidth();

protected:
    void  SetAttribute(IMapAttribute* pMapAttrib, bool bReload);
    void  ResetAttribute();
    HRESULT  FinalConstruct(IUIApplication* p);
    void  FinalRelease();
    void  OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	void  OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
    LRESULT  OnHoverItemChanged(WPARAM w, LPARAM l);
    void  OnLButtonDown(UINT nFlags, POINT point);
    void  OnLButtonUp(UINT nFlags, POINT point);
    void  OnTimer(UINT_PTR nIDEvent, LPARAM lParam);
    void  OnMouseMove(UINT nFlags, CPoint point);
    void  OnMouseLeave();
    void  OnSubMenuMouseMove(IMenu* pSubMenu);
    void  OnThemeChanged();

    void  ShowPopupSubMenu(IListItemBase* pItem);
    void  HidePopupSubMenu();
    int   PopupSubMenu(IListItemBase* pSubMenu);
    int   PopupAsSubMenu(UINT nFlags, Menu* pParentMenu, IListItemBase* pItem);

    void  OnInitPopupControlWindow();
    void  OnUnInitPopupControlWindow();
    LRESULT  OnGetIconGutterWidth(UINT, WPARAM, LPARAM) { return (LRESULT) m_nIconGutterWidth; }
    LRESULT  OnGetPopupTriangleWidth(UINT, WPARAM, LPARAM) { return (LRESULT) m_nPopupTriangleWidth; }

protected:
    IListItemBase*  LoadMenuItem(IUIElement* pUIElement, LPCTSTR szTagName, IMapAttribute* pMapAttrib);

    void  SetReturnCmd(UINT n);
    bool  IsMyChildMenu(Menu* pMenu);
    void  OnClick(IListItemBase* pItem);

    IListItemBase*  GetNextSelectableItem(IListItemBase* pItem);
    IListItemBase*  GetPrevSelectableItem(IListItemBase* pItem);
    IListItemBase*  _GetNextSelectableItem(IListItemBase* pItem);
    IListItemBase*  _GetPrevSelectableItem(IListItemBase* pItem);

    void  destroyPopupWindow();
    void  _HidePopupWindow();
    void  destroyAllSubMenu();
private:
    IMenu*  m_pIMenu;
    PopupMenuWindow*  m_pPopupWrapWnd;    // ���ڰ�װ�˵��Ĵ���

    Menu*  m_pNextMenu;                 // ��ǰ�����Ĳ˵��б��У��Լ����Ӳ˵�
    Menu*  m_pPrevMenu;                 // ��ǰ�����Ĳ˵��б��У��Լ��ĸ��˵�
    UINT   m_nTimerIDShowPopupSubMenu;  // ��ʱ��ID�����ڵ����Ӵ���
    UINT   m_nTimerIDHidePopupSubMenu;  // ��ʱ��ID�����ڹر��Ӵ���

    REGION4  m_rWindowShadowSpace;      // ���ڵ�������Ӱƫ����������ָ��һ��POINTʱ������˵�ʵ��λ�þ��ڸô��������ⲿ�ټ�����Ӱƫ����

    UINT  m_nTrackPopupMenuFlag;        // �˵�������ʽ������ָ��TPM_RETURNCMD
    UINT  m_nRetCmd;                    // ��ָ��TPM_RETURNCMD����ʱ�����ص�ID
    bool  m_bPopupAtParentRight;        // ��¼�Լ���Ϊ�Ӳ˵�ʱ�ڸ��˵�����һ�����򵯳����ˣ�����VK_LEFT/VK_RIGHT�ر�

    int   m_nIconGutterWidth;           // �˵����ͼ���� �Ŀ���
    int   m_nPopupTriangleWidth;        // �˵��Ҳ൯���˵����͵�������
    int   m_nSubMenuGap;                // �Ӳ˵��븸�˵�֮��ļ�϶
};
}