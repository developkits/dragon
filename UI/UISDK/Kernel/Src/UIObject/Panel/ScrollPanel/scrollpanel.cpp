#include "stdafx.h"
#include "scrollpanel.h"
#include "UISDK\Kernel\Inc\Interface\ilayout.h"

namespace UI
{

ScrollPanel::ScrollPanel()
{
    m_pIScrollPanel = NULL;
    m_sizeView.cx = m_sizeView.cy = NDEF;
}
ScrollPanel::~ScrollPanel()
{
}

HRESULT  ScrollPanel::FinalConstruct(IUIApplication* p)
{
    DO_PARENT_PROCESS(IScrollPanel, IPanel);
    if (FAILED(GetCurMsg()->lRet))
        return GetCurMsg()->lRet;

    m_mgrScrollBar.SetBindObject(m_pIScrollPanel);

     // 允许有焦点，这样才能响应mousewheel消息进行滚动
    ObjStyle s = {0};
    s.default_tabstop = 1;
    s.tabstop = 1;
	s.vscroll = 1;
	s.hscroll = 1;
	this->ModifyObjectStyle(&s, 0);

    return S_OK;
}

void  ScrollPanel::OnSerialize(SERIALIZEDATA* pData)
{
	__super::OnSerialize(pData);

	{
		AttributeSerializer as(pData, TEXT("ScrollPanel"));
		as.AddSize(XML_SCROLLPANEL_VIEWSIZE, m_sizeView)->AsData();
	}

    m_mgrScrollBar.Serialize(pData);
}

//
// 1. 指定了viewSize，直接传递viewSize给滚动条作为range
// 2. 没有指定viewSize，则使用DesiredSize作为滚动条range
void  ScrollPanel::virtualOnSize(UINT nType, UINT cx, UINT cy)
{
    Panel::virtualOnSize(nType, cx, cy);

    SIZE sizeContent = {0, 0};
    if (m_sizeView.cx != NDEF && m_sizeView.cy != NDEF)
    {
        sizeContent.cx = m_sizeView.cx;
        sizeContent.cy = m_sizeView.cy;
    }
    else
    {
        ILayout* pLayout = m_pIScrollPanel->GetLayout();
        if (NULL == pLayout)
            return;

        sizeContent = pLayout->Measure();
    }

    CRect rcClient;
    m_pIScrollPanel->GetClientRectInObject(&rcClient);
    SIZE sizePage = { rcClient.Width(), rcClient.Height() };
    m_mgrScrollBar.SetScrollPageAndRange(&sizePage, &sizeContent);
}


BOOL  ScrollPanel::OnMouseWheel(UINT nFlags, short zDelta, POINT pt)
{
    int nhScrollPos = 0, nvScrollPos = 0;
    int nhScrollPos2 = 0, nvScrollPos2 = 0;

    m_mgrScrollBar.GetScrollPos(&nhScrollPos, &nvScrollPos);
    m_mgrScrollBar.nvProcessMessage(GetCurMsg(), 0, false);
    m_mgrScrollBar.GetScrollPos(&nhScrollPos2, &nvScrollPos2);

    if (nvScrollPos != nvScrollPos2 || nhScrollPos != nhScrollPos2)
    {
        this->UpdateObject();
    }

    return 0;
}

}