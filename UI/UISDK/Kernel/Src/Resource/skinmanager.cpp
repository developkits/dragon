#include "StdAfx.h"
#include "skinmanager.h"

#include "UISDK\Kernel\Src\Resource\skinres.h"
#include "UISDK\Kernel\Inc\Interface\iuires.h"
#include "UISDK\Kernel\Src\Helper\timer\timermanager.h"
#include "UISDK\Kernel\Src\Helper\topwindow\topwindowmanager.h"
#include "UISDK\Kernel\Src\Resource\imagemanager.h"
#include "UISDK\Kernel\Src\Resource\colormanager.h"
#include "UISDK\Kernel\Src\Resource\stylemanager.h"
#include "UISDK\Kernel\Src\Resource\layoutmanager.h"
#include "UISDK\Kernel\Src\Resource\fontmanager.h"

using namespace UI;

SkinManager::SkinManager()
{
    m_pISkinManager = NULL;
	m_pCurActiveSkinRes = NULL;
	m_bDirty = false;

	m_pUIApplication = NULL;
}
SkinManager::~SkinManager()
{
    SAFE_DELETE(m_pISkinManager);
}

ISkinManager*  SkinManager::GetISkinManager()
{ 
    if (NULL == m_pISkinManager)
    {
        m_pISkinManager = new ISkinManager(this);
    }
    return m_pISkinManager; 
}

// 用于在UIApplication的析构函数中提前释放
void  SkinManager::Destroy()
{
	//////////////////////////////////////////////////////////////////////////
	// 释放各皮肤数据内存

	vector<SkinRes*>::iterator  iter = m_vSkinRes.begin();
	vector<SkinRes*>::iterator  iterEnd = m_vSkinRes.end();

	for ( ; iter != iterEnd; iter++ )
	{
		SkinRes* p = (*iter);
		delete p; 
	}
	m_vSkinRes.clear();
}

// 遍历该目录下的皮肤列表
void  SkinManager::SetSkinDirection(LPCTSTR szDir)
{
    if (NULL == szDir || 0 == _tcslen(szDir))
        return;

    m_strSkinDir = szDir;

    WIN32_FIND_DATA  finddata;
    if (m_strSkinDir[m_strSkinDir.length()-1] != _T('\\') &&
        m_strSkinDir[m_strSkinDir.length()-1] != _T('/'))
    {
        m_strSkinDir.append(_T("\\"));
    }

    String strFind(m_strSkinDir);
    strFind.append(_T("*.*"));

    HANDLE hFind=::FindFirstFile(strFind.c_str(),&finddata);
    if(INVALID_HANDLE_VALUE == hFind)
        return;

    while (1)
    {
        if (finddata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            if (finddata.cFileName[0] != '.')
            {
                // 判断目录下面是否存在skin.xml文件
                String strPath = m_strSkinDir;
                strPath.append(finddata.cFileName);

                String strFile = strPath;
                strFile.append(_T("\\") XML_SKIN_XML);

                if (PathFileExists(strFile.c_str()))
                {
                    OnFindSkinInSkinDir(SKIN_PACKET_TYPE_DIR, finddata.cFileName, strPath.c_str());
                }
            } 
        }
        else
        {
            // 判断文件后缀是否是*.skn格式
            TCHAR szExt[MAX_PATH] = _T("");
            Util::GetPathFileExt(finddata.cFileName, szExt);
            if (0 == _tcscmp(szExt, XML_SKIN_PACKET_EXT))
            {
				int nLength = _tcslen(finddata.cFileName);
				finddata.cFileName[nLength-4] = L'';
                OnFindSkinInSkinDir(SKIN_PACKET_TYPE_ZIP, finddata.cFileName, m_strSkinDir.c_str());
            }
        }
        if(!FindNextFile(hFind,&finddata))
            break;
    }
    FindClose(hFind);
}


// 在皮肤目录中添加一个新皮肤 
ISkinRes*  SkinManager::AddSkin(LPCTSTR  szName)
{
    if (NULL == szName)
        return NULL;

    String  strFilePath = m_strSkinDir;
    strFilePath.append(_T("\\"));
    strFilePath.append(szName);

    if (!PathFileExists(strFilePath.c_str()))
        return NULL;

    SkinRes* p = OnFindSkinInSkinDir(SKIN_PACKET_TYPE_DIR, szName, strFilePath.c_str());
    if (p)
        return p->GetISkinRes();

    return NULL;
}

// 在调用SetSkinDirection后，如果发现一个皮肤文件，则调用该响应函数
SkinRes*  SkinManager::OnFindSkinInSkinDir(SKIN_PACKET_TYPE eType, LPCTSTR szName, LPCTSTR szPath)
{
    SkinRes*  pSkin = new SkinRes(*this);
    pSkin->SetParam(eType, szName, szPath);

    m_vSkinRes.push_back(pSkin);
    return pSkin;
}

void  SkinManager::GetSkinDirection(TCHAR*  szOut)
{
    if (szOut)
    {
        _tcscpy(szOut, m_strSkinDir.c_str());
    }
}

IUIApplication*  SkinManager::GetUIApplication()
{
	return m_pUIApplication;
}

void SkinManager::SetUIApplication(IUIApplication* pUIApp)
{ 
	m_pUIApplication = pUIApp; 
	m_SkinBuilderRes.SetUIApplication(pUIApp);
}

void  ChangeSkinTimerProc(UINT_PTR, TimerItem* pItem)
{
    SkinManager* pThis = (SkinManager*)pItem->wParam;
    pThis->ChangeSkin((ISkinRes*)pItem->lParam, true);
}

// 换肤
// bSync用于解决：点击一个按钮换肤，但这个按钮在换肤中被delete了，导致换肤结束后堆栈崩溃
HRESULT SkinManager::ChangeSkin(ISkinRes* pISkinRes, bool bSync)
{
	if (NULL == pISkinRes)
		return E_INVALIDARG;

    SkinRes* pSkinRes = pISkinRes->GetImpl();
    if (pSkinRes == m_pCurActiveSkinRes)
        return S_FALSE;

    if (!bSync)
    {
        TimerItem  item;
        item.nRepeatCount = 1;
        item.pProc = ChangeSkinTimerProc;
        item.wParam = (WPARAM)this;
        item.lParam = (LPARAM)pISkinRes;
        TimerHelper::GetInstance()->SetTimer(1, &item);
        return E_PENDING;
    }

	SkinRes* pOldSkinRes = m_pCurActiveSkinRes;
	m_pCurActiveSkinRes = NULL;

    pSkinRes->Load();
    m_pCurActiveSkinRes = pSkinRes;
	
    ITopWindowManager* pTopWndMgr = m_pUIApplication->GetTopWindowMgr();
    if (pTopWndMgr)
        pTopWndMgr->GetImpl()->ChangeSkin(pSkinRes);

    pOldSkinRes->Unload();

	m_bDirty = true;
	return true;
}

HRESULT SkinManager::ChangeSkinHLS( short h, short l, short s, int nFlag )
{
	bool bRet = m_pCurActiveSkinRes->ChangeSkinHLS(h,l,s,nFlag);
	if (false == bRet)
		return E_FAIL;

	// 通知窗口刷新
    ITopWindowManager* pTopWndMgr = m_pUIApplication->GetTopWindowMgr();
    if (pTopWndMgr)
    {
        UIMSG  msg;
        msg.message = UI_WM_SKINCHANGED;
        pTopWndMgr->SendMessage2AllWnd(&msg);
        pTopWndMgr->UpdateAllWindow();
    }

	m_bDirty = true;
	return S_OK;
}

//
//	加载皮肤数据
//
bool  SkinManager::LoadSkin(LPCTSTR szName)
{ 
	UI_LOG_INFO( _T("\n\n------------  LoadSkin: %s ----------------\n"), szName);

    SkinRes*  pSkinRes = this->GetSkinResByName(szName);
    if (NULL == pSkinRes)
    {
        UI_LOG_WARN(_T("GetSkinRes Failed. Name=%s"), szName);
        return false;
    }
    bool bRet = pSkinRes->Load();
    if (bRet)
        m_pCurActiveSkinRes = pSkinRes;

    return bRet;
}
//	添加一款皮肤到工程当中，返回该皮肤句柄
//
//	Parameter
//		strSkinName
//			[in]	皮肤名称
//
//		strSkinXmlFullPath
//			[in]	皮肤所对应的ui.xml所在路径
//
SkinRes* SkinManager::AddSkinRes(const String& strSkinName, const String& strSkinXmlFullPath)
{
#if 0
	//////////////////////////////////////////////////////////////////////////
	// 1. 创建ui.xml

	ISkinConfigParse* pSkinInfoParse = IParseClassFactory::CreateSkinInfoParseInstance(m_pUIApplication, strSkinXmlFullPath);
	if (false == pSkinInfoParse->Create() )
	{
		UI_LOG_ERROR(_T("SkinManager::AddSkinRes create skininfo failed."));
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	// 2. 添加到持久性对象属性中

	SkinDescResItem* pSkinItem = new SkinDescResItem;
	pSkinItem->SetSkinName(strSkinName);
	pSkinItem->SetSkinXmlPath(strSkinXmlFullPath);

	m_UISkinDescRes.AddSkinItem(pSkinItem);


	//////////////////////////////////////////////////////////////////////////
	// 3. 初始化皮肤对象

	SkinRes* pSkinRes = new SkinRes(pSkinItem);
	pSkinRes->SetProjectManager(this);
	pSkinRes->GetSkinConfigManager()->SetSkinConfigParse(pSkinInfoParse);


	//////////////////////////////////////////////////////////////////////////
	// 4. 添加到运行时对象属性中
	this->m_vSkinRes.push_back(pSkinRes);
	return pSkinRes;
#endif 
    return 0;
}


//
//	保存到文件中
//
//		NULL - NULL 全部保存
//		NULL -  xx  当前皮肤的指定资源
//		 xx  - NULL 指定皮肤的所有资源
//
bool SkinManager::Save(SkinRes* pSkinRes)
{
	bool bRet = true;

	//////////////////////////////////////////////////////////////////////////
	// 保存各皮肤信息

	int nSkinCount = (int)m_vSkinRes.size();
	for (int i = 0; i < nSkinCount; i++)
	{
		if (pSkinRes)
		{
			if (pSkinRes != m_vSkinRes[i])
				continue;

			pSkinRes->Save();
		}
		else
        {
			m_vSkinRes[i]->Save();
        }
	}

	return bRet;
}

//
//	获取一个HSKIN对应的在m_vSkinRes中的索引
//
//	失败返回-1
//
int SkinManager::GetSkinResIndex(SkinRes* pSkinRes)
{
	if (NULL == pSkinRes)
		return -1;

	int nSize = (int)m_vSkinRes.size();
	if (0 == nSize )
		return -1;

	for (int i = 0; i < nSize; i++)
	{
		if (m_vSkinRes[i] == pSkinRes)
			return i;
	}

	return -1;
}

//
//	枚举当前工程中的皮肤
//
UINT SkinManager::GetSkinCount()
{
	return (UINT)m_vSkinRes.size(); 
}

SkinRes*  SkinManager::GetSkinResByIndex(long lIndex)
{
	int nSize = (int)m_vSkinRes.size();
	if (lIndex < 0 || lIndex >= nSize )
		return NULL;
	
	return m_vSkinRes[lIndex];
}

SkinRes*  SkinManager::GetSkinResByName(LPCTSTR szName)
{
    if (NULL == szName)
        return NULL;

    vector<SkinRes*>::iterator iter = m_vSkinRes.begin();
    for (; iter != m_vSkinRes.end(); iter++)
    {
        SkinRes* p = *iter;
        if (0 == _tcscmp(szName, p->GetName()))
            return p;
    }

    return NULL;
}

//
//	设置当前的活动皮肤（例如皮肤编辑器中正在编辑的皮肤）
//
//	一些GET操作都是默认针对于当前皮肤而言的
//
HRESULT SkinManager::SetActiveSkin(ISkinRes* pSkinRes)
{
	if (NULL == pSkinRes)
	{
		UI_LOG_WARN(_T("SkinManager::SetActiveSkin failed"));
		return E_INVALIDARG;
	}

	SkinRes* pSkinRes2 = pSkinRes->GetImpl(); // 内部仍然保存为SkinRes，便于调用
    m_pCurActiveSkinRes = pSkinRes2;
	return S_OK;
}

SkinRes* SkinManager::GetActiveSkin()
{
	if (NULL == m_pCurActiveSkinRes)
		return NULL;

	return m_pCurActiveSkinRes;
}

