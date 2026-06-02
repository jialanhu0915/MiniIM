
/**
 * @file NetworkClient.h
 * @brief 即时通讯客户端应用程序入口
 * @author Yan Runxin
 * @date 2026-05-25
 */

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号


/** @brief MFC 应用程序单例类
 * @note InitInstance 中初始化 Winsock，ExitInstance 中清理 Winsock。
 *       整个进程只有一个实例（theApp），负责应用生命周期管理。
 */
class CNetworkClientApp : public CWinApp
{
public:
	/** @brief MFC 应用单例构造，设置重启管理器支持标志 */
	CNetworkClientApp();

public:
	/** @brief 应用程序入口：初始化 Winsock → 显示主对话框
	 * @return FALSE 退出消息循环（对话框关闭后返回 FALSE）
	 */
	virtual BOOL InitInstance();
	/** @brief 清理 Winsock 库
	 * @return 应用退出码
	 */
	virtual int ExitInstance() override;

// 实现

	DECLARE_MESSAGE_MAP()
};

///< MFC 应用程序单例（全局唯一）
extern CNetworkClientApp theApp;

