#pragma once
#include "UISDK\Kernel\Inc\Interface\ilayout.h"
#include "UISDK\Kernel\Src\Helper\layout\layout.h"

namespace UI{

class Object;

class CanvasLayoutParam : public LayoutParamImpl<ICanvasLayoutParam>
{
public:
    CanvasLayoutParam();
    ~CanvasLayoutParam();

    virtual void  UpdateByRect();
    virtual void  Serialize(SERIALIZEDATA* pData);
    virtual LAYOUTTYPE  GetLayoutType() { return LAYOUT_TYPE_CANVAS; }
    virtual SIZE  CalcDesiredSize();
    virtual bool  IsSizedByContent();

    static int  ParseAlignAttr(LPCTSTR);
public:
    DECLARE_INT_SETGET(ConfigWidth)
    DECLARE_INT_SETGET(ConfigHeight)
    DECLARE_INT_SETGET(ConfigLayoutFlags)
    DECLARE_INT_SETGET(ConfigLeft)
    DECLARE_INT_SETGET(ConfigRight)
    DECLARE_INT_SETGET(ConfigTop)
    DECLARE_INT_SETGET(ConfigBottom)

private:
    long  m_nConfigWidth;   // 对象的宽度，可取值： 数值 | "auto" . （对于window对象，width 是指client区域的大小，不是整个窗口的大小；width包括padding，但不包括margin）
    long  m_nConfigHeight;  // 对象的高度，可取值： 数值 | "auto" . （对于window对象，height是指client区域的大小，不是整个窗口的大小；height包括padding，但不包括margin）
    // 在这里需要说明，对象的最终占用的宽度= margin.left + width + margin.right
    // 也就是说这里的width = padding.left + padding.right + content.width
    long  m_nConfigLeft;
    long  m_nConfigRight;
    long  m_nConfigTop;
    long  m_nConfigBottom;
    long  m_nConfigLayoutFlags;
};


//
//	指定布局离边缘的位置
//
class CanvasLayout : public LayoutImpl<CanvasLayout, ICanvasLayout, CanvasLayoutParam, LAYOUT_TYPE_CANVAS>
{
public:
    virtual void  Serialize(SERIALIZEDATA*) override{};
    virtual SIZE  Measure() override;
    virtual void  Arrange(IObject* pObjToArrage=NULL, bool bUpdate=false) override;

public:
    static void  ArrangeObject(Object*  pChild, const int& nWidth, const int& nHeight);
};

}
