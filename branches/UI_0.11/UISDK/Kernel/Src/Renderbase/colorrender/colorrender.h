#pragma once
#include "UISDK\Kernel\Src\Renderbase\renderbase\renderbase.h"
#include "UISDK\Kernel\Inc\Interface\icolorrender.h"

namespace UI
{


//
// ������ɫ+�߿���ɫ�����߶��ǿ�ѡ�ģ�
//
class ColorRender : public RenderBase
{
public:
    ColorRender();
    ~ColorRender();

    UI_DECLARE_RENDERBASE(ColorRender, XML_RENDER_TYPE_COLOR, RENDER_TYPE_COLOR)

    UI_BEGIN_MSG_MAP
        UIMSG_WM_RENDERBASE_DRAWSTATE(DrawState)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_SERIALIZE(OnSerialize)
    UI_END_MSG_MAP_CHAIN_PARENT(RenderBase)

    void  SetIColorRender(IColorRender* p) { m_pIColorRender = p; }

	void  OnSerialize(SERIALIZEDATA* pData);
	void  DrawState(RENDERBASE_DRAWSTATE* pDrawStruct);

public:
	void  SetBkColor(COLORREF col);
    void  SetBorderColor(COLORREF col);
    void  SetBorderRegion(const CRegion4* prc);

	void  LoadBorderColor(LPCTSTR sz)
	{
		_LoadColor(sz, m_pBorderColor);
	}
	LPCTSTR  GetBorderColorId()
	{
		return _GetColorId(m_pBorderColor);
	}
	void  LoadBkColor(LPCTSTR sz)
	{
		_LoadColor(sz, m_pBkColor);
	}
	LPCTSTR  GetBkColorId()
	{
		return _GetColorId(m_pBkColor);
	}
public:
    IColorRender*  m_pIColorRender;

	Color*  m_pBkColor;
    Color*  m_pBorderColor;
    CRegion4  m_rcBorder;
};

// ������ĳЩ�ؼ���Ҫʹ��ϵͳ������ɫ��������ϵͳ����ı�ʱ����Ӧ����ɫҲ��Ҫ�ı� 
class SysColorRender : public /*Theme*/RenderBase
{
public:
    SysColorRender();
    ~SysColorRender();

    UI_DECLARE_RENDERBASE(SysColorRender, XML_RENDER_TYPE_SYSCOLOR, RENDER_TYPE_THEME_SYSCOLOR)

    UI_BEGIN_MSG_MAP
        UIMSG_WM_RENDERBASE_DRAWSTATE(DrawState)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_SERIALIZE(OnSerialize)
    UI_END_MSG_MAP_CHAIN_PARENT(RenderBase)

    void  SetISysColorRender(ISysColorRender* p) { m_pISysColorRender = p; }

	void  OnSerialize(SERIALIZEDATA* pData);
	void  DrawState(RENDERBASE_DRAWSTATE* pDrawStruct);

    void  SetBkColor(int nColorIndex);
    void  SetBorderColor(int nColorIndex);

    // ���ڸĳ�ÿ�����»�ȡ��ɫֵ����˲���Ҫ��Ӧ����ı����Ϣ
	// virtual const TCHAR* GetThemeName() { return NULL; }
	// virtual void  OnThemeChanged();

public:
    ISysColorRender*  m_pISysColorRender;
	long  m_nBkColorIndex;
	long  m_nBorderColorIndex;
    REGION4  m_rcBorder;

	// COLORREF    m_bkColor;     // ������WM_THEMECHANGED��Ϣ�У�ֱ�ӵ���GetSysColor��ȡ����ֵ��Ȼ�п����Ǿɵ�
	// COLORREF    m_borderColor; // ����������������ɫֵ������ÿ�λ���ʱ������GetSysColor
};

// ���򽥱���ɫ����
class GradientRender : public RenderBase
{
public:
    GradientRender();
    ~GradientRender();

    UI_DECLARE_RENDERBASE(GradientRender, XML_RENDER_TYPE_GRADIENT, RENDER_TYPE_GRADIENT)

    UI_BEGIN_MSG_MAP
        UIMSG_WM_RENDERBASE_DRAWSTATE(DrawState)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_SERIALIZE(OnSerialize)
    UI_END_MSG_MAP_CHAIN_PARENT(RenderBase)

    void  SetIGradientRender(IGradientRender* p) { m_pIGradientRender = p; }

	void  OnSerialize(SERIALIZEDATA* pData);
	void  DrawState(RENDERBASE_DRAWSTATE* pDrawStruct);

    void  LoadFromColor(LPCTSTR sz){
        _LoadColor(sz, m_pColorFrom);
    }
    void  LoadToColor(LPCTSTR sz){
        _LoadColor(sz, m_pColorTo);
    }
    void  LoadBorderColor(LPCTSTR sz){
        _LoadColor(sz, m_pBorderColor);
    }
    LPCTSTR  SaveFromColor(){
        return _GetColorId(m_pColorFrom);
    }
    LPCTSTR  SaveToColor(){
        return _GetColorId(m_pColorTo);
    }
    LPCTSTR  SaveBorderColor(){
        return _GetColorId(m_pBorderColor);
    }
public:
    IGradientRender*  m_pIGradientRender;
	Color*  m_pColorFrom;
	Color*  m_pColorTo;
    Color*  m_pBorderColor;
    REGION4  m_rcBorder;

    long  m_nGradientDir;
};



//
//	ֱ��ʹ����ɫֵ��Ϊ����
//
class ColorListRender : public RenderBase
{
public:
    ColorListRender();
    ~ColorListRender();

    UI_DECLARE_RENDERBASE(ColorListRender, XML_RENDER_TYPE_COLORLIST, RENDER_TYPE_COLORLIST)

    UI_BEGIN_MSG_MAP
        UIMSG_WM_RENDERBASE_DRAWSTATE(DrawState)
        UIMSG_WM_QUERYINTERFACE(QueryInterface)
        UIMSG_WM_SERIALIZE(OnSerialize)
    UI_END_MSG_MAP_CHAIN_PARENT(RenderBase)
	
    void  SetIColorListRender(IColorListRender* p) { m_pIColorListRender = p; }
	void  Clear();

    void  OnSerialize(SERIALIZEDATA* pData);
    void  DrawState(RENDERBASE_DRAWSTATE* pDrawStruct);

	void  SetStateColor(int nState, COLORREF colorBk, bool bSetBk, COLORREF colBorder, bool bSetBorder);
	
    void  SetCount(long n);
    long  GetCount();
    void  LoadBkColor(LPCTSTR szColor);
    LPCTSTR  SaveBkColor();
    void  LoadBorderColor(LPCTSTR szColor);
    LPCTSTR  SaveBorderColor();

private:
    IColorListRender*  m_pIColorListRender;
	vector<Color*>  m_vBkColor;
	vector<Color*>  m_vBorderColor;
	int   m_nCount; 

};
}