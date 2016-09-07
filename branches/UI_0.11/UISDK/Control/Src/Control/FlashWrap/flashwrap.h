#pragma once
#include "flash.tlh"
#include "UISDK\Control\Inc\Interface\iflashwrap.h"

namespace UI
{
	// 		ScaleMode����ȡ�� 
	// ShowAll:�ڿؼ�����ʾȫ��ӰƬ����,����ӰƬ������������,ӰƬ�Ĵ�С�����ڿؼ�������н�С��һ��.
	// NoBorder:�ڿؼ�����ʾ����ӰƬ����,����ӰƬ������������,ӰƬ�Ĵ�С�����ڿؼ�������нϴ��һ��.
	// ExactFit:�ڿؼ�����ʾȫ��ӰƬ����,������ӰƬ�ĳ�������,ǿ�ƽ�ӰƬ�ĳ������ڿؼ��ĳ���. 
	// 			0:�൱��Scaleȡ"ShowAll" 
	// 			1:�൱��Scaleȡ"NoBorder" 
	// 			2:�൱��Scaleȡ"ExactFit" 
	//		m_pFlash->put_ScaleMode(2);


// 	1. site վ�������ʲô?
// 		ÿһ��Ƕ����󶼻��Ӧ����(Container)�е�һ��site���󣬸�site����ΪǶ������ṩλ�õ���Ϣ
// 		������ά����site�б���
// 
// 	2. swfһ���ǲ�����ͣ��
//
//  3. ����flashλ�õ�����
//     . ����SetFlashPos����
//     . Ҫ���Ǽ�������, (1)-�����λ�� (2)-��Ч����λ�� (3)�ؼ�ˢ��
//       �������Ϊ�������꣬����Ч����Ҳ�ǻ��ڴ��ڵģ����´���OnPaintʱ�����л�Ϊ�ؼ�����
//       ��ֱ�ӽ�flashλ������Ϊ(0, 0, width, height)�������ʱ��һ��ת��
//

	class FlashWrap;
	class FlashEmbeddingSite : 
					public IOleClientSite,
					public IOleControlSite,
					public IAdviseSink,       // �ؼ�ͨ���ýӿڵ���OnViewChange��ˢ���Լ�
					public IOleInPlaceSiteWindowless
		
	{
	public:
		FlashEmbeddingSite(FlashWrap* p);
		~FlashEmbeddingSite();

#pragma region // IUnknown
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void  **ppvObject);
		virtual ULONG   STDMETHODCALLTYPE AddRef(void);
		virtual ULONG   STDMETHODCALLTYPE Release(void);
#pragma endregion

#pragma region // IAdviseSink
		virtual void STDMETHODCALLTYPE OnDataChange(FORMATETC *pFormatetc, STGMEDIUM *pStgmed)
			{ }
		virtual  void STDMETHODCALLTYPE OnViewChange(DWORD dwAspect, LONG lindex)
			{ }
		virtual void STDMETHODCALLTYPE OnRename(IMoniker *pmk)
			{ }
		virtual void STDMETHODCALLTYPE OnSave(void) 
			{ }
		virtual void STDMETHODCALLTYPE OnClose(void)
			{ }
#pragma endregion

#pragma region // IOleClientSite
		virtual HRESULT STDMETHODCALLTYPE SaveObject(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,  IMoniker **ppmk)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer **ppContainer)
			{ *ppContainer = NULL; return E_NOTIMPL; } 
		virtual HRESULT STDMETHODCALLTYPE ShowObject(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL fShow)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void)
			{ return S_OK; }
#pragma endregion

#pragma region // IOleControlSite
		virtual HRESULT STDMETHODCALLTYPE OnControlInfoChanged(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE LockInPlaceActive(BOOL fLock)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetExtendedControl(IDispatch **ppDisp)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE TransformCoords(POINTL *pPtlHimetric, POINTF *pPtfContainer, DWORD dwFlags)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG *pMsg, DWORD grfModifiers)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnFocus(BOOL fGotFocus)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE ShowPropertyFrame(void)
			{ return S_OK; }
#pragma endregion

#pragma region  // IOleInPlaceSiteWindowless
		virtual HRESULT STDMETHODCALLTYPE CanWindowlessActivate(void) 
			{ return S_OK; } // support Windowless activation
		virtual HRESULT STDMETHODCALLTYPE GetCapture(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE SetCapture(BOOL fCapture)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetFocus(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE SetFocus(BOOL fFocus)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetDC(LPCRECT pRect, DWORD grfFlags, HDC *phDC)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE InvalidateRect(LPCRECT pRect, BOOL fErase);
		virtual HRESULT STDMETHODCALLTYPE InvalidateRgn(HRGN hRGN, BOOL fErase)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE ScrollRect(INT dx, INT dy, LPCRECT pRectScroll, LPCRECT pRectClip)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE AdjustRect(LPRECT prc)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnDefWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult)
			{ return S_OK; }
#pragma endregion

#pragma region  // IOleInPlaceSiteEx
		virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivateEx(BOOL *pfNoRedraw, DWORD dwFlags)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivateEx(BOOL fNoRedraw)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE RequestUIActivate(void)
			{ return S_OK; }
#pragma endregion

#pragma region // IOleInPlaceSite
		virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnUIActivate(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo);
		virtual HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtant)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL fUndoable)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE DiscardUndoState(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void)
			{ return S_OK; }
		virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT lprcPosRect)
			{ return S_OK; }
#pragma endregion

#pragma region // IOleWindow
		virtual HRESULT STDMETHODCALLTYPE GetWindow(HWND *phwnd);
		virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode) 
			{ return S_OK; }
#pragma endregion

	public:
		bool   GetInvalidateRect(bool bClear, RECT* lprc);
		void   SetFlashInvalidateListener(IFlashInvalidateListener* p){
			m_pListener = p; }

	private:
		long         m_lRef;
		FlashWrap*   m_pFlashWrap;
        IFlashWrap*  m_pIFlashWrap;
		CRect        m_rcInvalidate;
	public:
		IFlashInvalidateListener*  m_pListener;
	};

class FlashWrap : public MessageProxy
{
public:
	FlashWrap();
	~FlashWrap();

	UI_DECLARE_OBJECT(FlashWrap, OBJ_CONTROL|CONTROL_FLASH)

	UI_BEGIN_MSG_MAP
		UIMSG_WM_PAINT(OnPaint)
		UIMSG_WM_SIZE(OnSize)
		UIMESSAGE_HANDLER_EX(WM_MOUSEMOVE, OnMouseMsg)
		UIMESSAGE_HANDLER_EX(WM_LBUTTONDOWN, OnMouseMsg)
		UIMESSAGE_HANDLER_EX(WM_LBUTTONUP, OnMouseMsg)
        UIMSG_WM_GETDESIREDSIZE(GetDesiredSize)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_GETOBJECTINFO(OnGetObjectInfo)
        UIMSG_WM_RESETATTRIBUTE(ResetAttribute)
        UIMSG_WM_SETATTRIBUTE(SetAttribute)
        UIMSG_WM_FINALCONSTRUCT(FinalConstruct)
        UIMSG_WM_FINALRELEASE(FinalRelease)
	UI_END_MSG_MAP_CHAIN_PARENT_Ixxx(FlashWrap, IControl)

    void  SetIFlashWrap(IFlashWrap* p)
	{ 
		m_pIFlashWrap = p;
		SetIMessageProxy(static_cast<IMessage*>(p)); 
	}
    inline IFlashWrap*  GetIFlashWrap()
	{ 
		return m_pIFlashWrap;
	}

public:
    void  SetFlashUri(LPCTSTR szUri);
    HRESULT  CallFlashFunction(LPCTSTR szFuncName, LPCTSTR szArgs, BSTR* pbstrRet);
    void  Pause();
    void  Play();

public:
	HRESULT  FinalConstruct(IUIApplication* p);
    void  FinalRelease();
	void  GetDesiredSize(SIZE* pSize);
	void  ResetAttribute();
	void  SetAttribute(IMapAttribute* pMapAttr, bool bReload);
	LRESULT  OnMouseMsg(UINT nMsg, WPARAM wParam, LPARAM lParam);

	void   SetFlashInvalidateListener(IFlashInvalidateListener* p);
	void   OnPaint(IRenderTarget* pRenderTarget);  // <- ����public�ģ�����render layer��ֱ�ӵ���
	bool   IsAvailable();

protected:
	void   OnSize( UINT nType, int cx, int cy );
	void   SetWMode(FLASH_WMODE e);

protected:
	bool      CreateControl();
	void      DrawFlash(HDC hDC);
	HRESULT   SetFlashPos(LPRECT lprc);
	HRESULT   SetFlashSize(int cx, int cy);

protected:
    IFlashWrap*  m_pIFlashWrap;

	FlashEmbeddingSite*  m_pSite;
	IOleObject*          m_pOleObject;
	IViewObjectEx*       m_pViewObject;
	IOleInPlaceObjectWindowless*  m_pWindowless;  // �����޴���ģʽ����꽻��
public:
	IShockwaveFlash*     m_pFlash;
protected:

	String   m_strFlashUri;
	FLASH_WMODE  m_eWMode;
	int      m_nFlashWidth;
	int      m_nFlashHeight;
};

struct  FlashPropertyUtil
{
	static int  GetFlashWidth(IShockwaveFlash* pFlash);
	static int  GetFlashHeight(IShockwaveFlash* pFlash);
};
}