// ControlDemo.cpp : ����Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "ControlDemo.h"
#include "../Soft3DRotate/ControlDemoWindow.h"
#include "../Clock/clockwindow.h"
#include "../REShadow/reshadow.h"
#include "../Stage3D/Stage3DWindow.h"
#include "../Stage3D_Leaves/StageLeavesWindow.h"
#include "../Animate/AnimateWindow.h"

#define MAX_LOADSTRING 100

// ȫ�ֱ���:
TCHAR szTitle[MAX_LOADSTRING];					// �������ı�
TCHAR szWindowClass[MAX_LOADSTRING];			// ����������
HWND  g_hWnd = NULL;
UI::IUIApplication* g_pUIApp = NULL;
HINSTANCE g_hInstance = NULL;

// �˴���ģ���а����ĺ�����ǰ������:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
    g_hInstance = hInstance;

 	// TODO: �ڴ˷��ô��롣
//	MSG msg;
	HACCEL hAccelTable;

	// ��ʼ��ȫ���ַ���
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CONTROLDEMO, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ִ��Ӧ�ó����ʼ��:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CONTROLDEMO));
	
	CreateUIApplicationInstance(&g_pUIApp);

	TCHAR szPath[MAX_PATH] = _T("");
    UI::Util::GetAppPath_(szPath);
	String str = szPath;

    g_pUIApp->SetSkinDirection(str.c_str());
	g_pUIApp->LoadSkin(_T("Default"));

    str.append(_T("\\Default\\skin.xml"));
    g_pUIApp->SetLog(NULL, (BSTR)str.c_str());

    UI_Ctrl_RegisterUIObject(g_pUIApp);

    {
#ifdef DEMO_CLOCK
        ClockWindow* pWnd = NULL;
        ClockWindow::CreateInstance(g_pUIApp, &pWnd);
    
        SetWindowPos(g_hWnd, 0, 50, 50, 630, 660, SWP_NOZORDER);
        pWnd->Attach(g_pUIApp, g_hWnd, _T("clockwin"));

#elif defined _RESHADOW
        REShadowWindow* pWnd = NULL;
        REShadowWindow::CreateInstance(g_pUIApp, &pWnd);
        SetWindowPos(g_hWnd, 0, 50, 50, 300, 300, SWP_NOZORDER);

        pWnd->Attach(g_pUIApp, g_hWnd, _T("reshadow"));

#elif defined DEMO_STAGE3D
        UI::UI3D_RegisterUIObject(g_pUIApp);

        CStage3DWindow* pWnd = NULL;
        CStage3DWindow::CreateInstance(g_pUIApp, &pWnd);
        SetWindowPos(g_hWnd, 0, 50, 50, 500, 500, SWP_NOZORDER);

        pWnd->Attach(g_pUIApp, g_hWnd, _T("stage3d"));

#elif defined DEMO_STAGELEAVES
		UI::UI3D_RegisterUIObject(g_pUIApp);

		CStageLeavesWindow* pWnd = NULL;
		CStageLeavesWindow::CreateInstance(g_pUIApp, &pWnd);
		SetWindowPos(g_hWnd, 0, 50, 0, 610, 860, SWP_NOZORDER);

		pWnd->Attach(g_pUIApp, g_hWnd, _T("stageparticle"));

#elif defined DEMO_ANIMATE
        CAnimateWindow* pWnd = NULL;
        CAnimateWindow::CreateInstance(g_pUIApp, &pWnd);

        pWnd->Attach(g_pUIApp, g_hWnd, _T("animate"));
#else

        CSoft3DRotateWindow* pWnd = NULL;
        CSoft3DRotateWindow::CreateInstance(g_pUIApp, &pWnd);
        
  		pWnd->Attach(g_pUIApp, g_hWnd, _T("mainwindow"));
#endif

        ShowWindow(g_hWnd, nCmdShow);
        UpdateWindow(g_hWnd);

        g_pUIApp->MsgHandleLoop();

        pWnd->Detach();
        pWnd->delete_this();
  	}
	g_pUIApp->Release();
	return  0;
}



//
//  ����: MyRegisterClass()
//
//  Ŀ��: ע�ᴰ���ࡣ
//
//  ע��:
//
//    ����ϣ��
//    �˴��������ӵ� Windows 95 �еġ�RegisterClassEx��
//    ����֮ǰ�� Win32 ϵͳ����ʱ������Ҫ�˺��������÷������ô˺���ʮ����Ҫ��
//    ����Ӧ�ó���Ϳ��Ի�ù�����
//    ����ʽ��ȷ�ġ�Сͼ�ꡣ
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONTROLDEMO));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_CONTROLDEMO);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_CONTROLDEMO));

	return RegisterClassEx(&wcex);
}

//
//   ����: InitInstance(HINSTANCE, int)
//
//   Ŀ��: ����ʵ�����������������
//
//   ע��:
//
//        �ڴ˺����У�������ȫ�ֱ����б���ʵ�������
//        ��������ʾ�����򴰿ڡ�
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 1200, 500, NULL, NULL, hInstance, NULL);

   if (!g_hWnd)
   {
      return FALSE;
   }

   // CreateWindow(_T("BUTTON"), _T("HostWnd Button"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP, 500,500, 200,30, g_hWnd, (HMENU)100, NULL, NULL);
   return TRUE;
}

//
//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��: ���������ڵ���Ϣ��
//
//  WM_COMMAND	- ����Ӧ�ó���˵�
//  WM_PAINT	- ����������
//  WM_DESTROY	- �����˳���Ϣ������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �����˵�ѡ��:
		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			hdc = BeginPaint(hWnd, &ps);
			// TODO: �ڴ����������ͼ����...
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
