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
/* 过期
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

void  SkinManager::GetSkinDirection(TCHAR*  szOut)
{
    if (szOut)
    {
        _tcscpy(szOut, m_strSkinDir.c_str());
    }
}

// 在皮肤目录中添加一个新皮肤 
ISkinRes*  SkinManager::AddSkin(LPCTSTR  szPath)
{
    if (NULL == szPath)
        return NULL;

    String  strFilePath = m_strSkinDir;
    strFilePath.append(_T("\\"));
    strFilePath.append(szPath);

    if (!PathFileExists(strFilePath.c_str()))
        return NULL;

    SkinRes* p = OnFindSkinInSkinDir(SKIN_PACKET_TYPE_DIR, szPath, strFilePath.c_str());
    if (p)
        return p->GetISkinRes();

    return NULL;
}


// 在调用SetSkinDirection后，如果发现一个皮肤文件，则调用该响应函数
SkinRes*  SkinManager::OnFindSkinInSkinDir(SKIN_PACKET_TYPE eType, LPCTSTR szPath, LPCTSTR szPath)
{
    SkinRes*  pSkin = new SkinRes(*this);
    pSkin->SetParam(eType, szPath, szPath);

    m_vSkinRes.push_back(pSkin);
    return pSkin;
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

SkinRes*  SkinManager::GetSkinResByIndex(long lIndex)
{
	int nSize = (int)m_vSkinRes.size();
	if (lIndex < 0 || lIndex >= nSize )
		return NULL;
	
	return m_vSkinRes[lIndex];
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

SkinRes* SkinManager::GetActiveSkin()
{
	if (NULL == m_pCurActiveSkinRes)
		return NULL;

	return m_pCurActiveSkinRes;
}

*/

IUIApplication*  SkinManager::GetUIApplication()
{
	return m_pUIApplication;
}

void SkinManager::SetUIApplication(IUIApplication* pUIApp)
{ 
	m_pUIApplication = pUIApp; 
	m_SkinBuilderRes.SetUIApplication(pUIApp);
}

// void  ChangeSkinTimerProc(UINT_PTR, TimerItem* pItem)
// {
//     SkinManager* pThis = (SkinManager*)pItem->wParam;
//     pThis->ChangeSkin((ISkinRes*)pItem->lParam, true);
// }

void SkinManager::ChangeSkinHLS( short h, short l, short s, int nFlag )
{
	vector<SkinRes*>::iterator iter = m_vSkinRes.begin();
	for (; iter != m_vSkinRes.end(); ++iter)
	{
		(*iter)->ChangeSkinHLS(h,l,s,nFlag);
	}

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
}

//
//	加载皮肤数据
//
SkinRes*  SkinManager::LoadSkinRes(LPCTSTR szPath)
{ 
	if (!szPath)
		return NULL;

	UI_LOG_INFO( _T("\n\n------------  LoadSkinRes: %s ----------------\n"), szPath);

	TCHAR szSkinName[MAX_PATH] = {0};
	SKIN_PACKET_TYPE eSkinPackageType = SKIN_PACKET_TYPE_DIR;

	if (PathIsDirectory(szPath))
	{
		// 从路径中获取皮肤名。
		TCHAR szDir[MAX_PATH] = {0};
		_tcscpy(szDir, szPath);
		int nLength = _tcslen(szDir);
		if (nLength < 1)
			return NULL;

		// 如果最后一个字符是 \，删除。
		if (szDir[nLength-1] == TEXT('\\'))
			szDir[nLength-1] = 0;
		Util::GetPathFileName(szDir, szSkinName);
		SkinRes* pTest = GetSkinResByName(szSkinName);
		if (pTest)
		{
			UI_LOG_WARN(TEXT("Skin Exist: name=%s"), szSkinName);
			return pTest;
		}
		eSkinPackageType = SKIN_PACKET_TYPE_DIR;
	}
	else
	{
		TCHAR szExt[MAX_PATH] = _T("");
		Util::GetPathFileExt(szPath, szExt);

		int nExtLength = 0; 

		// 如果没有带后缀名，尝试一下补个后缀
		if (!szExt[0])
		{
			String strPathTry(szPath);
			strPathTry.append(TEXT(".") XML_SKIN_PACKET_EXT);
			if (!PathFileExists(strPathTry.c_str()))
			{
				UI_LOG_ERROR(TEXT("Skin File not exist: %s"), strPathTry.c_str());
				return NULL;
			}
		}
		else if (0 != _tcscmp(szExt, XML_SKIN_PACKET_EXT))
		{
			UI_LOG_ERROR(TEXT("Skin File Format Error: %s"), szExt);
			return NULL;
		}
		else
		{
			nExtLength = _tcslen(XML_SKIN_PACKET_EXT) + 1;
		}

		Util::GetPathFileName(szPath, szSkinName);
		szSkinName[_tcslen(szSkinName)-nExtLength] = 0;
		SkinRes* pTest = GetSkinResByName(szSkinName);
		if (pTest)
		{
			UI_LOG_WARN(TEXT("Skin Exist: name=%s"), szSkinName);
			return pTest;
		}

		eSkinPackageType = SKIN_PACKET_TYPE_ZIP;
	}

	SkinRes* pSkin = new SkinRes(*this);
	pSkin->SetParam(eSkinPackageType, szSkinName, szPath);
	if (!pSkin->Load())
	{
		UI_LOG_ERROR(TEXT("Skin load failed: %s"), szPath);
		SAFE_DELETE(pSkin);
		return NULL;
	}

	m_vSkinRes.push_back(pSkin);
	return pSkin;
}

SkinRes*  SkinManager::GetDefaultSkinRes()
{
	if (0 == m_vSkinRes.size())
		return NULL;
	
	return m_vSkinRes[0];
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

uint  SkinManager::GetSkinResCount()
{
	return m_vSkinRes.size();
}
SkinRes*  SkinManager::GetSkinResByIndex(uint i)
{
	if (i >= m_vSkinRes.size())
		return NULL;

	return m_vSkinRes[i];
}