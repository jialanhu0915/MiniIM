
// NetworkServer.h: PROJECT_NAME 应用程序的主头文件
//

/**
 * @file   NetworkServer.h
 * @brief  MFC 服务端应用程序类
 * @author Yan Runxin
 * @date   2026-05-25
 */

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号


/**
 * @brief MFC 服务端应用程序类（Winsock 初始化）
 */
// CNetworkServerApp:
// 有关此类的实现，请参阅 NetworkServer.cpp
//

class CNetworkServerApp : public CWinApp
{
public:
	CNetworkServerApp();

// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance() override; /** @brief 清理 Winsock @return 退出码 */

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CNetworkServerApp theApp;
