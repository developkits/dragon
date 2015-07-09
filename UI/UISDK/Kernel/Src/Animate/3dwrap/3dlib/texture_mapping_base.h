#pragma once
#include "UISDK\Kernel\Src\Atl\image.h"
#include "UISDK\Kernel\Src\Animate\3dwrap\3dlib\3dlib.h"

namespace UI
{

enum TRIANGLE_TYPE
{
	TRIANGLE_GENERAL,    
	TRIANGLE_FLAT_TOP,    // 平顶
	TRIANGLE_FLAT_BOTTOM, // 平底
};


// TextMapping Mode
#define  TextMapping_Bilinear     0x0001
#define  TextMapping_AlphaBlend   0x0010

#define  BILINEAR_EDGE_PADDING 1

class TextureMappingBase
{
public:
    TextureMappingBase();
    virtual ~TextureMappingBase(){}

    virtual void  Do(POINT*  ptOffset, RECT* prcClip);
	void  DoTriangle(TexturePoint* V0, TexturePoint* V1, TexturePoint* V2);
	virtual void  DoFlatTopTriangle(TexturePoint* A, TexturePoint* B, TexturePoint* C){}
	virtual void  DoFlatBottomTriangle(TexturePoint* A, TexturePoint* B, TexturePoint* C){}
	virtual void  DoGeneralTriangle(TexturePoint* A, TexturePoint* B, TexturePoint* C){}

    virtual void  SetQuad(QUAD* pQuad, RECT* prc);
    virtual void  MousePoint2TexturePoint(POINT pt, float* pfx, float* pfy){UIASSERT(0);};

    void  SetSrcBuffer(ImageData* pBuffer);
    void  SetDstBuffer(ImageData* pBuffer);
    void  SetTexturePointA(int u, int v);
    void  SetTexturePointB(int u, int v);
    void  SetTexturePointC(int u, int v);
    void  SetTexturePointD(int u, int v);
	void  SetTextureRect(RECT* prc);
	void  GetTexturePadding(RECT* prc);

private:
    void SetTexturePoint(int nIndex, int u, int v);

public:
    ImageData  m_destBuffer;
    ImageData  m_texture;
	Image  m_imageTexture; // 如果需要作边界哨兵处理时，重新生成一个带哨兵边界的纹理 

    Quad   m_quad;             // 3d映射后的坐标
    TexturePoint  m_texturePoint[4];  // 纹理坐标

	long  m_lTextMappingMode;

	//////////////////////////////////////////////////////////////////////////
	POINT  m_ptOffset;
	CRect  m_rcClip;
};

}