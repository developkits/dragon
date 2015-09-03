#pragma once
#include "UISDK\Project\UICompositor\Inc\ihardwarecompositor.h"

//
// 如何开启DX10的抗锯齿？
//
// 1. CreateSwapChain时，指定SampleDesc的Count大于1
// 2. 在hlsl中指定SetRasterizerState, 将MultisampleEnable = TRUE;
//
// 如何实现剪裁？
// 1. 通过RSSetViewport来限制绘制区域是不对的。这种方式是将整个内容缩放绘制到新的
//    viewport当中，而不是我们想要的仅剪裁的效果。
//
// 2. RSSetScissorRects
//    The scissor rectangles will only be used if ScissorEnable is set to true 
//    in the rasterizer state
//

namespace UI
{
class GpuLayerTexture;
class HardwareComposition
{
public:
	HardwareComposition(HWND hWnd);
	~HardwareComposition();

	GpuLayerTexture*  CreateLayerTexture();
    bool  BeginCommit();
    void  EndCommit();
    void  Resize(UINT nWidth, UINT nHeight);
    void  SetRootLayerTexture(GpuLayerTexture* p);
    GpuLayerTexture*  GetRootLayerTexture();

protected:
    void  CreateSwapChain();
    void  ReCreateRenderTargetView();

public:
    IHardwareComposition  m_IHardwareComposition;

private:
	GpuLayerTexture*  m_pRootTexture;
	HWND  m_hWnd;

    IDXGISwapChain*  m_pSwapChain;
    ID3D10RenderTargetView*  m_pRenderTargetView;
    SIZE   m_sizeBackBuffer;
};
}