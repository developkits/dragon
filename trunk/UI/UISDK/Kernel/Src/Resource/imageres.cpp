#include "stdafx.h"
#include "imageres.h"
#include "UISDK\Kernel\Src\Renderlibrary\gdi\gdibitmap.h"
#include "UISDK\Kernel\Src\Renderlibrary\gdiplus\gdiplusbitmap.h"
#include "UISDK\Kernel\Src\Resource\skinres.h"
#include "UISDK\Kernel\Inc\Util\iimage.h"

namespace UI
{

//////////////////////////////////////////////////////////////////////////
ImageResItem::ImageResItem()
{
	m_pMapAttrib = NULL;
	m_pGdiBitmap = NULL;
	m_pGdiplusBitmap = NULL;
	// 	m_pDirect2DBitmap = NULL;
	m_pOriginImageData = NULL;
	m_pIImageResItem = NULL;

	m_bUseSkinHLS = true;
	m_bNeedAntiAliasing = false;
	//	m_bHasAlphaChannel = false;
	m_bMustHasAlphaChannel = false;
	m_eType = IMAGE_ITEM_TYPE_IMAGE;
}
ImageResItem::~ImageResItem()
{
    // 不直接删除，还有控件在引用这个图片（如在UIEditor中删除一张图片），由引用者负责释放其引用的图片指针
    if (m_pGdiBitmap)
    {
        m_pGdiBitmap->Destroy();
		m_pGdiBitmap->SetOutRef(NULL); // 注：nnd，搞了我一天才发现没的置空导致的莫名其妙的崩溃，自己都析构了，还通知个毛，不崩溃才怪
    }
    if (m_pGdiplusBitmap)
    {
        m_pGdiplusBitmap->Destroy();
		m_pGdiplusBitmap->SetOutRef(NULL);
    }

// 	SAFE_DELETE(m_pGdiBitmap);
// 	SAFE_DELETE(m_pGdiplusBitmap);
	// 	SAFE_DELETE(m_pDirect2DBitmap);
	SAFE_DELETE(m_pOriginImageData);
	SAFE_RELEASE(m_pMapAttrib);
	SAFE_DELETE(m_pIImageResItem);
}

IImageResItem*  ImageResItem::GetIImageResItem()
{
	if (NULL == m_pIImageResItem)
		m_pIImageResItem = new IImageResItem(this);

	return m_pIImageResItem;
}

IMAGE_ITEM_TYPE  ImageResItem::GetImageItemType()
{
    return m_eType;
}
void  ImageResItem::SetImageItemType(IMAGE_ITEM_TYPE e)
{
    if (e == IMAGE_ITEM_TYPE_IMAGE ||
        e == IMAGE_ITEM_TYPE_ICON || 
        e == IMAGE_ITEM_TYPE_IMAGE_LIST)
        m_eType = e;
}

IRenderBitmap* ImageResItem::GetImage(SkinRes* pSkinRes, GRAPHICS_RENDER_LIBRARY_TYPE eRenderType, bool* pbFirstTimeCreate)
{
	if (NULL == pSkinRes)
		return NULL;

	switch (eRenderType)
	{
	case GRAPHICS_RENDER_LIBRARY_TYPE_GDI:
		 return GetImage_gdi(pSkinRes, pbFirstTimeCreate);	

	case GRAPHICS_RENDER_LIBRARY_TYPE_GDIPLUS:
        return GetImage_gdiplus(pSkinRes, pbFirstTimeCreate);	
	}

	return NULL;
}

IRenderBitmap*  ImageResItem::GetImage_gdi(SkinRes* pSkinRes, bool* pbFirstTimeCreate)
{
    ISkinDataSource* pDataSource = pSkinRes->GetDataSource();
    if (NULL == pDataSource)
        return NULL;

    if (m_bNeedAntiAliasing)
        return GetImage_gdiplus(pSkinRes, pbFirstTimeCreate);

    if (m_pGdiBitmap )
    {
        m_pGdiBitmap->AddRef();
        return (IRenderBitmap*)m_pGdiBitmap;
    }    

    RenderBitmapFactory::CreateInstance(
        GRAPHICS_RENDER_LIBRARY_TYPE_GDI,
        m_eType, 
        (IRenderBitmap**)&m_pGdiBitmap);

    if (!m_pGdiBitmap)
        return NULL;
    
    m_pGdiBitmap->SetOutRef((IRenderResource **)&m_pGdiBitmap);
    SetRenderBitmapAttribute(m_pGdiBitmap);

    /*if (false == m_pGdiBitmap->LoadFromFile(m_strPath.c_str(), m_bMustHasAlphaChannel))*/
    if (false == pDataSource->Load_RenderBitmap(m_pGdiBitmap, m_strPath.c_str(), m_bMustHasAlphaChannel))
    {
        SAFE_RELEASE(m_pGdiBitmap);
        UI_LOG_ERROR(_T("%s Load gdi bitmap from file failed. path=%s"), FUNC_NAME, m_strPath.c_str());
        return NULL;
    }
//  UI_LOG_DEBUG(_T("%s gdi bitmap create: %s \tPtr=0x%08X"), FUNC_NAME, m_strId.c_str(), m_pGdiBitmap);

    if (pbFirstTimeCreate)
        *pbFirstTimeCreate = true;

    return (IRenderBitmap*)m_pGdiBitmap;
}

IRenderBitmap*  ImageResItem::GetImage_gdiplus(SkinRes* pSkinRes, bool* pbFirstTimeCreate)
{
    ISkinDataSource* pDataSource = pSkinRes->GetDataSource();
    if (!pDataSource)
        return NULL;

    m_bMustHasAlphaChannel = true;  // 只要有尝试创建GRAPHICS_RENDER_TYPE_GDIPLUS类型的图片，就将m_bMustHasAlphaChannel设置为true（这也许不太准确，但也是个办法）

    if (m_bNeedAntiAliasing)  
    {
        if (m_pGdiplusBitmap)
        {
            m_pGdiplusBitmap->AddRef();
            return (IRenderBitmap*)m_pGdiplusBitmap;
        }

        RenderBitmapFactory::CreateInstance(GRAPHICS_RENDER_LIBRARY_TYPE_GDIPLUS, m_eType, (IRenderBitmap**)&m_pGdiplusBitmap);
        if (!m_pGdiplusBitmap )
            return NULL;

        m_pGdiplusBitmap->SetOutRef((IRenderResource **)&m_pGdiplusBitmap);
        SetRenderBitmapAttribute(m_pGdiplusBitmap);

        /*if (false == m_pGdiplusBitmap->LoadFromFile(m_strPath.c_str(), true))*/
        if (false == pDataSource->Load_RenderBitmap(m_pGdiplusBitmap, m_strPath.c_str(), true))
        {
            SAFE_RELEASE(m_pGdiplusBitmap);
            UI_LOG_ERROR(_T("%s Load gdiplus bitmap from file failed. path=%s"), FUNC_NAME, m_strPath.c_str());
            return NULL;
        }
//      UI_LOG_DEBUG(_T("%s gdiplus bitmap create: %s \tPtr=0x%08X"), FUNC_NAME, m_strId.c_str(), m_pGdiplusBitmap );

        if (pbFirstTimeCreate)
            *pbFirstTimeCreate = true;
        return (IRenderBitmap*)m_pGdiplusBitmap;

    }
    else     // 如果不需要支持抗锯齿功能，则直接使用GDI绘图，提高效率
    {
        if (!m_pGdiBitmap)
            return GetImage_gdi(pSkinRes, pbFirstTimeCreate);

        if (!m_pGdiBitmap->GetBitmap()->HasAlphaChannel())  // 重新创建一张带alpha channel的图片
        {
            // 强行让image类通过gdiplus创建一个带alpha通道的图片，即使这张图片可能没有alphachannel，image会让gdiplus帮我们转换
            m_pGdiBitmap->GetBitmap()->Destroy();
            SetRenderBitmapAttribute(m_pGdiBitmap);

            /*if (false == m_pGdiBitmap->LoadFromFile(m_strPath.c_str(), m_bMustHasAlphaChannel))*/
            if (false == pDataSource->Load_RenderBitmap(m_pGdiBitmap, m_strPath.c_str(), m_bMustHasAlphaChannel))
            {
                SAFE_RELEASE(m_pGdiBitmap);
                UI_LOG_ERROR(_T("%s Load gdi bitmap from file failed. path=%s"), FUNC_NAME, m_strPath.c_str());
                return NULL;
            }
        }

        m_pGdiBitmap->AddRef();
        return (IRenderBitmap*)m_pGdiBitmap;
    }

    return NULL;
}

bool ImageResItem::ModifyHLS(short h, short l, short s, int nFlag)
{
	if (false == m_bUseSkinHLS)
		return true;

	ModifyHLS(m_pGdiBitmap, h,l,s,nFlag);
	ModifyHLS(m_pGdiplusBitmap, h,l,s,nFlag);

	// 	ModifyHLS(m_pDirect2DBitmap, h,l,s,nFlag);

	// 	if (m_pGdiBitmap)
	// 		m_pGdiBitmap->GetBitmap()->Save(_T("c:\\aaa.png"),Gdiplus::ImageFormatPNG);
	return true;
}
bool ImageResItem::ModifyHLS(IRenderBitmap* pBitmap, short h, short l, short s, int nFlag)
{
	if (false == m_bUseSkinHLS)
		return true;

	if (pBitmap)
	{
		if (NULL == m_pOriginImageData)
		{
			m_pOriginImageData = new ImageData;
			if (false == pBitmap->SaveBits(m_pOriginImageData))
			{
				UI_LOG_WARN(_T("%s not support this image to change hue. id=%s"), FUNC_NAME, m_strId.c_str() );
				m_bUseSkinHLS = false;
				SAFE_DELETE(m_pOriginImageData);
			}
		}
		return pBitmap->ChangeHLS(m_pOriginImageData,  h,l,s,nFlag);
	}
	return false;
}

bool ImageResItem::ModifyAlpha(int nAlphaPercent)
{
	if (m_pGdiBitmap)
	{
		if (NULL == m_pOriginImageData)
		{
			m_pOriginImageData = new ImageData;
			if (false == m_pGdiBitmap->SaveBits(m_pOriginImageData))
			{
				UI_LOG_WARN(_T("%s not support this image to change alpha. id=%s"), FUNC_NAME, m_strId.c_str() );
				SAFE_DELETE(m_pOriginImageData);
			}
		}
		return m_pGdiBitmap->GetBitmap()->ModifyAlpha(m_pOriginImageData, nAlphaPercent);
	}
	return false;
}

//
// 修改图片
//
// 注意：1. 外界根本就不会知道image已经被重新创建的事实，因为没有指针被销毁
//       2. 但是有一个问题，如果第一次加载的是32位带alpha的图片，返回出去的将是GdiBitmap，
//          这时候如果modify为一个非32位的位图，将导致GdiBitmap没有alpha通道，在分层窗口上
//          显示失败
//
bool ImageResItem::ModifyImage(const TCHAR* szPath)
{
	if (NULL == szPath)
		return false;

	m_strPath = szPath;

	SAFE_DELETE(m_pOriginImageData);

	if (m_pGdiBitmap)
	{
		if (!m_pGdiBitmap->LoadFromFile(m_strPath.c_str(), m_bMustHasAlphaChannel))
		{
			UI_LOG_ERROR(_T("%s load from file failed 1:%s"), FUNC_NAME, m_strPath.c_str());
		}
	}
	if (m_pGdiplusBitmap)
	{
		if (!m_pGdiplusBitmap->LoadFromFile(m_strPath.c_str(), true))
		{
			UI_LOG_ERROR(_T("%s load from file failed 2:%s"), FUNC_NAME, m_strPath.c_str());
		}
	}
	// 	if (m_pDirect2DBitmap)
	// 	{
	// 		m_pDirect2DBitmap->Modify(strPath);
	// 	}

	return true;
}

void ImageResItem::SetAttribute(IMapAttribute* pMapAttrib) 
{ 
	m_pMapAttrib = pMapAttrib;
	m_pMapAttrib->AddRef();

	bool   bUseSkinHLS = false;
	bool   bNeedAntiAliasing = false;

	pMapAttrib->GetAttr_bool(XML_IMAGE_USESKINHLS, true, &bUseSkinHLS);
	this->SetUseSkinHLS(bUseSkinHLS);

	pMapAttrib->GetAttr_bool(XML_IMAGE_NEED_ANTIALIASING, true, &bNeedAntiAliasing);
	this->SetNeedAntiAliasing(bNeedAntiAliasing);
}

// TODO: 该函数以后将被干掉，还差一个icon没有重构
void  ImageResItem::SetRenderBitmapAttribute(IRenderBitmap* pRenderBitmap)
{
}
//////////////////////////////////////////////////////////////////////////
ImageListResItem::ImageListResItem()
{
    m_eType = IMAGE_ITEM_TYPE_IMAGE_LIST;
    m_eLayoutType = IMAGELIST_LAYOUT_TYPE_H;
    m_nCount = 1;
}

IImageResItem*  ImageListResItem::GetIImageResItem()
{
    if (NULL == m_pIImageResItem)
        m_pIImageResItem = new IImageListResItem(this);

    return m_pIImageResItem;   
}
IMAGELIST_LAYOUT_TYPE  ImageListResItem::GetLayoutType()
{
    return m_eLayoutType;
}

int  ImageListResItem::GetItemCount()
{
    return m_nCount;
}
void  ImageListResItem::SetLayoutType(IMAGELIST_LAYOUT_TYPE e)
{
    m_eLayoutType = e;
}
void  ImageListResItem::SetItemCount(int n)
{   
    m_nCount = n;
}
void  ImageListResItem::SetAttribute(IMapAttribute* pMapAttrib)
{
    __super::SetAttribute(pMapAttrib);

    const TCHAR* szText = pMapAttrib->GetAttr(XML_IMAGE_IMAGELIST_LAYOUT, true);
    if (szText)
    {
        if (0 == _tcscmp(szText, XML_IMAGE_IMAGELIST_LAYOUT_V))
            m_eLayoutType = IMAGELIST_LAYOUT_TYPE_V;
        else
            m_eLayoutType = IMAGELIST_LAYOUT_TYPE_H;
    }

    pMapAttrib->GetAttr_int(XML_IMAGE_IMAGELIST_COUNT, true, &m_nCount);
}

void  ImageListResItem::SetRenderBitmapAttribute(IRenderBitmap* pRenderBitmap)
{
    if (!pRenderBitmap)
        return;

    if (pRenderBitmap->GetImageType() != IMAGE_ITEM_TYPE_IMAGE_LIST)
        return;

    IImageListRenderBitmap* pImageListBmp = static_cast<IImageListRenderBitmap*>(pRenderBitmap);
    pImageListBmp->SetItemCount(m_nCount);
    pImageListBmp->SetLayoutType(m_eLayoutType);
}


//////////////////////////////////////////////////////////////////////////

ImageIconResItem::ImageIconResItem()
{
    m_eType = IMAGE_ITEM_TYPE_ICON;
    m_sizeDraw.cx = 16;
    m_sizeDraw.cx = 16;
}

IImageResItem*  ImageIconResItem::GetIImageResItem()
{
    if (NULL == m_pIImageResItem)
        m_pIImageResItem = new IImageIconResItem(this);

    return m_pIImageResItem;   
}
void  ImageIconResItem::SetAttribute(IMapAttribute* pMapAttrib)
{
    __super::SetAttribute(pMapAttrib);

    pMapAttrib->GetAttr_int(XML_IMAGE_ICON_WIDTH,  true, (int*)&m_sizeDraw.cx);
    pMapAttrib->GetAttr_int(XML_IMAGE_ICON_HEIGHT, true, (int*)&m_sizeDraw.cy);
}

void  ImageIconResItem::SetRenderBitmapAttribute(IRenderBitmap* pRenderBitmap)
{
    if (!pRenderBitmap)
        return;

    if (pRenderBitmap->GetImageType() != IMAGE_ITEM_TYPE_ICON)
        return;

    IImageIconRenderBitmap* pIcon = static_cast<IImageIconRenderBitmap*>(pRenderBitmap);
    pIcon->SetDrawSize(&m_sizeDraw);
}

//////////////////////////////////////////////////////////////////////////
//
//	ImageRes
//

ImageRes::ImageRes(SkinRes*  pSkinRes)
{
	m_pSkinRes = pSkinRes; 
	m_pIImageRes = NULL;
}

ImageRes::~ImageRes()
{	
	this->Clear(); 
	SAFE_DELETE(m_pIImageRes);
}

IImageRes*  ImageRes::GetIImageRes()
{
	if (NULL == m_pIImageRes)
		m_pIImageRes = new IImageRes(this);

	return m_pIImageRes;
}

//
// 从文件中加载一项(由CXmlImageParse::load_from_file中调用)
//
ImageResItem* ImageRes::LoadItem(const TCHAR* szType, IMapAttribute* pMapAttrib, const TCHAR* szFullPath)
{
    if (!szType || !pMapAttrib || !szFullPath)
        return NULL;

    const TCHAR* szId = pMapAttrib->GetAttr(XML_ID, true);
    if (!szId)
        return NULL;

    IMAGE_ITEM_TYPE eType = IMAGE_ITEM_TYPE_IMAGE;
    if (0 == _tcscmp(szType, XML_IMAGE_ITEM_TYPE_ICON))
    {
        eType = IMAGE_ITEM_TYPE_ICON;
    }
    else if (0 == _tcscmp(szType, XML_IMAGE_ITEM_TYPE_IMAGELIST))
    {
        eType = IMAGE_ITEM_TYPE_IMAGE_LIST;
    }
    else 
    {
        int lLength = _tcslen(szFullPath);
        if (lLength > 4 && 0 == _tcsicmp((szFullPath+lLength-4), _T(".ico")))
        {
            eType = IMAGE_ITEM_TYPE_ICON;
        }
        else
        {
            eType = IMAGE_ITEM_TYPE_IMAGE;
        }
    }

	ImageResItem* pItem = this->InsertImage(eType, szId, szFullPath);
	if (!pItem)
	{
        UI_LOG_WARN( _T("%s insert image m_strID=%s, path=%s failed."), FUNC_NAME, szId, szFullPath);
        return NULL;
	}
	
    pItem->SetAttribute(pMapAttrib);
    return pItem;
}

long ImageRes::GetImageCount() 
{
	return (long)m_vImages.size();
}

IImageResItem* ImageRes::GetImageResItem(long lIndex)
{
	ImageResItem* pItem = this->GetImageItem2(lIndex);
	if (NULL == pItem)
		return NULL;

	return pItem->GetIImageResItem();
}
IImageResItem*  ImageRes::GetImageResItem(const TCHAR* szID)
{
	if (NULL == szID)
		return NULL;

	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	for (; iter != m_vImages.end(); iter++)
	{
		ImageResItem* p = *iter;
		if (0 == _tcscmp(szID, p->GetIdRef().c_str()))
			return p->GetIImageResItem();
	}
	return NULL;
}

ImageResItem* ImageRes::GetImageItem2(int nIndex)
{
	if (nIndex < 0)
		return NULL;
	if (nIndex >= (int)m_vImages.size())
		return NULL;

	return m_vImages[nIndex];
}

ImageResItem* ImageRes::GetImageItem2(const TCHAR* szId)
{
	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	for (; iter != m_vImages.end(); iter++)
	{
		ImageResItem* p = *iter;
		if (0 == _tcscmp(p->GetId(), szId))
			return p;
	}
	return NULL;
}

ImageResItem*  ImageRes::InsertImage(IMAGE_ITEM_TYPE eType, const TCHAR* szId, const TCHAR* szPath)
{
    if (!szId || !szPath)
        return NULL;

    ImageResItem* pItem = this->GetImageItem2(szId);
    if (pItem)
    {
        UI_LOG_WARN(_T("%s failed, insert item=%s, path=%s"), FUNC_NAME, szId, szPath);
        return NULL;
    }

    ImageResItem*  pImageItem = NULL;
    switch (eType)
    {
    case IMAGE_ITEM_TYPE_IMAGE_LIST:
        pImageItem = new ImageListResItem;
        break;

    case IMAGE_ITEM_TYPE_ICON:
        pImageItem = new ImageIconResItem;
        break;

    default:
        pImageItem = new ImageResItem;
        break;
    }

    if (!pImageItem)
        return NULL;
	
	pImageItem->SetId(szId);
	pImageItem->SetPath(szPath);

	this->m_vImages.push_back(pImageItem);  
	return pImageItem;
}

bool ImageRes::ModifyImage(const TCHAR* szId, const TCHAR* szPath)
{
	if (NULL == szId || NULL == szPath)
		return false;

	ImageResItem* p = this->GetImageItem2(szId);
	if (p)
	{
		bool bRet = p->ModifyImage(szPath);
		if (p->GetUseSkinHLS())
		{
			// 检查当前皮肤的HLS
			if (m_pSkinRes && m_pSkinRes->GetHLSInfo())
			{
				SKIN_HLS_INFO* pHLSInfo = m_pSkinRes->GetHLSInfo();
				p->ModifyHLS(pHLSInfo->h, pHLSInfo->l, pHLSInfo->s, pHLSInfo->nFlag);
			}
		}

		return bRet;
	}

	UI_LOG_WARN(_T("%s failed. modify image id=%s, path=%s"), FUNC_NAME, szId, szPath);
	return false;
}
bool ImageRes::RemoveImage(const TCHAR* szId)
{
    if (!szId)
        return false;

	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	for (; iter != m_vImages.end(); iter++)
	{
		ImageResItem* p = *iter;
		if (0 == _tcscmp(p->GetId(), szId))
		{
			delete p; 
			p = NULL;
			m_vImages.erase(iter);
			return true;
		}
	}
	return false;
}
bool ImageRes::RemoveImage(IImageResItem* pItem)
{
    if (!pItem)
        return false;

    ImageResItem* pItemImpl = pItem->GetImpl();
    
    vector<ImageResItem*>::iterator iter = m_vImages.begin();
    for (; iter != m_vImages.end(); iter++)
    {
        if (*iter == pItemImpl)
        {
            delete pItemImpl; 
            m_vImages.erase(iter);
            return true;
        }
    }
//     vector<ImageResItem*>::iterator iter = std::find(m_vImages.begin(), m_vImages.end(), pItemImpl);
//     if (iter == m_vImages.end())
//         return false;

    return false;
}
bool ImageRes::Clear()
{
	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	vector<ImageResItem*>::iterator  iterEnd = m_vImages.end();

	for (; iter != iterEnd; iter++)
	{
		ImageResItem* p = *iter;
		delete p;
		p = NULL;
	}

	m_vImages.clear();
	return true;
}

bool ImageRes::ChangeSkinHLS(short h, short l, short s, int nFlag)
{
	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	vector<ImageResItem*>::iterator  iterEnd = m_vImages.end();

	for (; iter != iterEnd; iter++)
	{
		ImageResItem* p = *iter;
		p->ModifyHLS(h,l,s,nFlag);
	}

	return true;
}

bool ImageRes::ModifyImageItemAlpha(const String& strID, int nAlphaPercent)
{
	vector<ImageResItem*>::iterator  iter = m_vImages.begin();
	vector<ImageResItem*>::iterator  iterEnd = m_vImages.end();

	for (; iter != iterEnd; iter++)
	{
		ImageResItem* p = *iter;
		if (p->GetIdRef() == strID)
		{
			return p->ModifyAlpha(nAlphaPercent);
		}
	}

	return false;
}

HBITMAP  ImageRes::LoadBitmap(const TCHAR* szId)
{
	if (NULL == szId)
		return NULL;

	ImageResItem* pItem = this->GetImageItem2(szId);
	if (NULL == pItem)
	{
		UI_LOG_ERROR( _T("%s，GetImageItem：%s failed .1"), FUNC_NAME, szId);
		return NULL;
	}

	ISkinDataSource* pDataSource = m_pSkinRes->GetDataSource();
	if (NULL == pDataSource)
		return NULL;

	IImage image;
	pDataSource->Load_Image(&image, pItem->GetPath());    

	if (image.IsNull())
		return NULL;

	return image.Detach();
}

HRESULT  ImageRes::GetBitmap(const TCHAR* szImageID, GRAPHICS_RENDER_LIBRARY_TYPE eRenderType, __out IRenderBitmap** pRenderBitmap)
{
	if (NULL == szImageID || NULL == pRenderBitmap)
		return E_INVALIDARG;

	ImageResItem* pItem = this->GetImageItem2(szImageID);
	if (NULL == pItem)
	{
		UI_LOG_ERROR( _T("%s，GetImageItem：%s failed .1"), FUNC_NAME, szImageID);
		return E_FAIL;
	}

	bool bFirstTimeCreate = false;
	IRenderBitmap* pBitmap = pItem->GetImage(m_pSkinRes, eRenderType, &bFirstTimeCreate);
	if (NULL == pBitmap)
	{
		UI_LOG_ERROR( _T("%s，GetImage：%s failed .2"), FUNC_NAME, szImageID);
		return E_FAIL;
	}

	if (bFirstTimeCreate && pItem->GetUseSkinHLS())
	{
		// 检查当前皮肤的HLS
		if (m_pSkinRes && m_pSkinRes->GetHLSInfo())
		{
			SKIN_HLS_INFO* pHLSInfo = m_pSkinRes->GetHLSInfo();
			pItem->ModifyHLS(pBitmap, pHLSInfo->h, pHLSInfo->l, pHLSInfo->s, pHLSInfo->nFlag);
		}
	}

	*pRenderBitmap = pBitmap;
	return S_OK;
}


}