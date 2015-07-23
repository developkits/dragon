// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>
#include <map>
#include <list>
#include <vector>
#include <stack>
#include <algorithm>

#include <atlbase.h>
#include <atlcomcli.h>

using namespace std;

#pragma warning(disable:4996)
//warning C4481: ʹ���˷Ǳ�׼��չ: ����д˵������override��
#pragma warning(disable:4481)
//warning C4100: ��x��: δ���õ��β�
#pragma warning(disable:4100)
//warning C4706: ��������ʽ�ڵĸ�ֵ
#pragma warning(disable:4706)
#pragma warning(disable:4192)

#pragma comment(lib, "comctl32.lib")

// ������
#include "UISDK\Project\UIUtil\h\util.h"
#ifdef _UNICODE
#	pragma comment(lib,"uiutil.lib")
#else
#	pragma comment(lib,"uiutil.lib")
#endif

// ע����#import "msxml6.dll"����leakdetect.h����ᵼ��new�������屻�޸ģ��ڴ�й¶�޷��õ��ļ�line
#import "msxml6.dll" raw_interfaces_only named_guids no_implementation 

// ע������GdiplusBase.h�е�new/delete ���أ�ʹ��leakdetect��new/delete
//#include <GdiPlus.h>
#include "3rd\gdiplus\gdiplusfix.h"

// �Լ��Ĺ���ͷ�ļ�
#include "UISDK\Kernel\Inc\Base\inc.h"
#include "UISDK\Kernel\Inc\Base\leakdetect.h"
#include "UISDK\Kernel\Src\Base\globalfunc.h"

using namespace UI;  // �ڱ�ģ���ڲ��ſ� using ʹ������

extern HINSTANCE g_hInstance;

// ��ӡ��������
#ifdef _DEBUGx
#define TRACE_DRAW_PROCESS
#endif