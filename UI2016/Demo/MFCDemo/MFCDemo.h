
// MFCDemo.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CMFCDemoApp: 
// �йش����ʵ�֣������ MFCDemo.cpp
//

class CMFCDemoApp : public CWinApp
{
public:
	CMFCDemoApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��
    UI::IUIApplication*  m_pUIApp = nullptr;
    UI::ISkinRes* m_pSkinRes = nullptr;

	DECLARE_MESSAGE_MAP()


};

extern CMFCDemoApp theApp;