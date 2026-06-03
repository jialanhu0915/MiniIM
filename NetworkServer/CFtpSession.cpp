/**
 * @file   CFtpSession.cpp
 * @brief  FTP 会话实现（RFC 959 子集）
 *
 * 本课实现的命令：
 *   USER / PASS / QUIT / PWD / CWD / TYPE / NOOP
 *   PASV / LIST / RETR / STOR / DELE / MKD / RMD  留作下节课
 */
#include "pch.h"
#include "CFtpSession.h"
#include "NetworkServerDlg.h"
#include <WS2tcpip.h>
#include <algorithm>
#include <cctype>
#include <direct.h>     // _getcwd, _chdir, _mkdir, _rmdir
#include <io.h>         // _findfirst, _findnext, _A_SUBDIR
#include <sys/stat.h>   // _S_IWRITE 等
#include <cstdio>        // _popen（LIST 备用）
#include <cstring>
#include <utility>      // std::move
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

 // ============================================================
 //  工厂
 // ============================================================

 /**
  * @brief 启动一个新会话（new + AfxBeginThread）
  */
void CFtpSession::Launch(SOCKET ctrlSocket,
	const std::string& rootDir,
	const std::string& clientIp,
	CNetworkServerDlg* pDlg) {
	CFtpSession* pSession = new CFtpSession(ctrlSocket, rootDir, clientIp, pDlg);
	AfxBeginThread([](LPVOID p) -> UINT {
		CFtpSession* s = static_cast<CFtpSession*>(p);
		s->Run();
		delete s;  // 自销毁
		return 0;
		}, pSession);
}

// ============================================================
//  构造 / 析构
// ============================================================

CFtpSession::CFtpSession(SOCKET sock, std::string root,
	std::string ip, CNetworkServerDlg* pDlg)
	: m_ctrl(sock)
	, m_root(std::move(root))
	, m_clientIp(std::move(ip))
	, m_pDlg(pDlg)
	, m_cwd("/")
	, m_authed(false)
	, m_type('I') {
	// 确保 m_root 末尾是 '\\'
	if (!m_root.empty() && (m_root.back() != '\\' && m_root.back() != '/')) {
		m_root.push_back('\\');
	}
}

CFtpSession::~CFtpSession() {
	if (m_ctrl != INVALID_SOCKET) {
		::closesocket(m_ctrl);
		m_ctrl = INVALID_SOCKET;
	}
	// 数据连接已由对应命令关闭
}

// ============================================================
//  主循环
// ============================================================

/**
 * @brief 会话主循环
 *
 * 1. 发 220 欢迎
 * 2. 循环收命令 → 解析 "CMD arg" → 分发
 * 3. 收到 QUIT 或 socket 断开 → 退出
 */
void CFtpSession::Run() {
	TRACE(_T("[CFtpSession] 客户端 %hs 已连接\n"), m_clientIp.c_str());
	SendResponse(220, "Mini Ftp Server ready.");

	while (true) {
		std::string line;
		if (!RecvLine(line)) {
			// 客户端断开
			break;
		}

		// 解析 "CMD arg"（大写命令 + 首尾去空白的参数）
		std::string cmd, arg;
		auto sp = line.find(' ');
		if (sp == std::string::npos) {
			cmd = line;
			arg = "";
		}
		else {
			cmd = line.substr(0, sp);
			arg = line.substr(sp + 1);
		}
		// 去尾部空白
		while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.back()))) {
			arg.pop_back();
		}
		// 命令转大写（FTP 命令是大小写不敏感的）
		std::transform(cmd.begin(), cmd.end(), cmd.begin(),
			[](unsigned char c) { return std::toupper(c); });

		TRACE(_T("[CFtpSession] %hs > %hs %hs\n"),
			m_clientIp.c_str(), cmd.c_str(), arg.c_str());

		// 分发
		if (cmd == "USER")      HandleUSER(arg);
		else if (cmd == "PASS") HandlePASS(arg);
		else if (cmd == "QUIT") { HandleQUIT(arg); break; }
		else if (cmd == "PWD" || cmd == "XPWD") HandlePWD(arg);
		else if (cmd == "CWD" || cmd == "XCWD") HandleCWD(arg);
		else if (cmd == "TYPE") HandleTYPE(arg);
		else if (cmd == "NOOP") HandleNOOP(arg);
		// ---- 下节课实现 ----
		else if (cmd == "PASV") HandlePASV(arg);
		else if (cmd == "LIST") HandleLIST(arg);
		else if (cmd == "RETR") HandleRETR(arg);
		else if (cmd == "STOR") HandleSTOR(arg);
		else if (cmd == "DELE") HandleDELE(arg);
		else if (cmd == "MKD" || cmd == "XMKD") HandleMKD(arg);
		else if (cmd == "RMD" || cmd == "XRMD") HandleRMD(arg);
		else                    HandleUnknown(cmd, arg);
	}

	TRACE(_T("[CFtpSession] 客户端 %hs 断开\n"), m_clientIp.c_str());
}

// ============================================================
//  网络 I/O
// ============================================================

/**
 * @brief  收一行命令（处理半包 + 拼包）
 */
bool CFtpSession::RecvLine(std::string& out) {
	char buf[1024];
	while (true) {
		// 先看缓存里有没有完整行
		auto pos = m_recvBuf.find("\r\n");
		if (pos != std::string::npos) {
			out = m_recvBuf.substr(0, pos);
			m_recvBuf.erase(0, pos + 2);
			return true;
		}
		// 缓存里没有 → recv 一次
		int n = ::recv(m_ctrl, buf, sizeof(buf), 0);
		if (n <= 0) {
			return false;  // 断开或错误
		}
		m_recvBuf.append(buf, n);
	}
}

/**
 * @brief  发响应（自动拼 "code text\r\n"）
 */
void CFtpSession::SendResponse(int code, const std::string& text) {
	char line[1024];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "%d %s\r\n", code, text.c_str());
	::send(m_ctrl, line, (int)strlen(line), 0);
}

/**
 * @brief  发原始字符串（PASV 响应需要完整控制）
 */
void CFtpSession::SendRaw(const std::string& s) {
	::send(m_ctrl, s.data(), (int)s.size(), 0);
}

// ============================================================
//  命令实现
// ============================================================

/**
 * @brief USER <username> — 提供用户名
 */
void CFtpSession::HandleUSER(const std::string& arg) {
	m_user = arg;
	m_authed = false;
	SendResponse(331, "User name okay, need password.");
}

/**
 * @brief PASS <password> — 提供密码
 *
 * 教学简化：任意非空密码都通过。
 * 真实实现：查数据库 / 配置文件。
 */
void CFtpSession::HandlePASS(const std::string& arg) {
	if (m_user.empty()) {
		SendResponse(503, "Login with USER first.");
		return;
	}
	if (arg.empty()) {
		SendResponse(501, "Password required.");
		return;
	}
	m_authed = true;
	SendResponse(230, "User logged in, proceed.");
}

/**
 * @brief QUIT — 退出
 */
void CFtpSession::HandleQUIT(const std::string&) {
	SendResponse(221, "Goodbye.");
}

/**
 * @brief PWD — 打印当前工作目录
 */
void CFtpSession::HandlePWD(const std::string&) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }
	std::string msg = "\"" + m_cwd + "\" is current directory.";
	SendResponse(257, msg);
}

/**
 * @brief CWD <path> — 切换目录
 *
 * 支持 /、..、.、相对/绝对路径。
 * 不允许逃出沙箱。
 */
void CFtpSession::HandleCWD(const std::string& arg) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }

	// 把 m_cwd 和 arg 拼起来，规范化
	std::string combined;
	if (arg.empty() || arg == ".") {
		combined = m_cwd;
	}
	else if (arg[0] == '/') {
		combined = arg;  // 绝对路径
	}
	else {
		// 相对路径：去掉 cwd 末尾的 '/'
		std::string base = m_cwd;
		if (base.size() > 1 && base.back() == '/') base.pop_back();
		combined = base + "/" + arg;
	}

	// 规范化：处理 "." / ".." / 多余的 "/"
	std::string normalized;
	normalized.push_back('/');
	std::string seg;
	for (size_t i = 1; i < combined.size(); ++i) {
		char c = combined[i];
		if (c == '/') {
			if (seg == "..") {
				// 弹一段
				auto slash = normalized.find_last_of('/');
				if (slash != std::string::npos && slash > 0) {
					normalized.erase(slash);
				}
				seg.clear();
			}
			else if (seg != "." && !seg.empty()) {
				normalized += seg;
				normalized.push_back('/');
				seg.clear();
			}
			else {
				seg.clear();
			}
		}
		else {
			seg.push_back(c);
		}
	}
	// 处理最后一段
	if (seg == "..") {
		auto slash = normalized.find_last_of('/');
		if (slash != std::string::npos && slash > 0) normalized.erase(slash);
	}
	else if (!seg.empty() && seg != ".") {
		normalized += seg;
		normalized.push_back('/');
	}
	// 至少保证以 '/' 结尾
	if (normalized.empty()) normalized = "/";
	if (normalized.back() != '/') normalized.push_back('/');

	// 沙箱校验：尝试拼真实路径
	std::string real;
	if (!ResolveRealPath(normalized, real)) {
		SendResponse(550, "CWD failed: path escapes sandbox.");
		return;
	}
	// 必须是已存在目录
	struct _stat st;
	if (_stat(real.c_str(), &st) != 0 || !(st.st_mode & _S_IFDIR)) {
		SendResponse(550, "CWD failed: no such directory.");
		return;
	}

	m_cwd = normalized;
	SendResponse(250, "Directory changed to \"" + m_cwd + "\".");
}

/**
 * @brief TYPE <A|I> — 切换传输模式（binary / ascii）
 */
void CFtpSession::HandleTYPE(const std::string& arg) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }
	if (arg.empty()) { SendResponse(501, "TYPE requires A or I."); return; }
	char t = static_cast<char>(std::toupper(static_cast<unsigned char>(arg[0])));
	if (t != 'A' && t != 'I') {
		SendResponse(504, "TYPE accepts only A or I.");
		return;
	}
	m_type = t;
	SendResponse(200, std::string("Type set to ") + t);
}


/**
 * @brief NOOP — 无操作
 */
void CFtpSession::HandleNOOP(const std::string&) {
	SendResponse(200, "NOOP ok.");
}

/**
 * @brief 未实现命令
 */
void CFtpSession::HandleUnknown(const std::string& cmd, const std::string&) {
	SendResponse(502, "Command not implemented: " + cmd);
}

// ============================================================
//  数据连接（PASV）
// ============================================================


void CFtpSession::HandlePASV(const std::string& arg)
{
	if (!m_authed)
	{
		SendResponse(530, "Please login with USER and PASS.");
		return;
	}

	int port = 0;
	std::string ip;
	if (!OpenDataConnection(port, ip)) {
		SendResponse(425, "Can't open passive connection.");
		return;
	}
	int p1 = port / 256;    // 高字节
	int p2 = port % 256;   // 低字节
	std::replace(ip.begin(), ip.end(), '.', ',');
	char buf[128];
	_snprintf_s(buf, sizeof(buf), _TRUNCATE,
		"Entering Passive Mode (%s,%d,%d).",
		ip.c_str(), p1, p2);
	SendResponse(227, buf);
}

bool CFtpSession::OpenDataConnection(int& outPort, std::string& outIp)
{
	if (m_dataListen != INVALID_SOCKET) {
		::closesocket(m_dataListen);
		m_dataListen = INVALID_SOCKET;
	}

	// 1. socket()
	m_dataListen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_dataListen == INVALID_SOCKET)
	{
		return false;
	}

	// 2. bind() 到任意地址、端口 0
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = 0;

	if (::bind(m_dataListen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		::closesocket(m_dataListen);
		m_dataListen = INVALID_SOCKET;
		return false;
	}

	// 3. listen()
	if (listen(m_dataListen, 1) == SOCKET_ERROR)
	{
		::closesocket(m_dataListen);
		m_dataListen = INVALID_SOCKET;
		return false;
	}

	// 4. getsockname() 拿实际端口
	sockaddr_in actual{};
	int len = sizeof(actual);
	if (::getsockname(m_dataListen, (sockaddr*)&actual, &len) == SOCKET_ERROR)
	{
		::closesocket(m_dataListen);
		m_dataListen = INVALID_SOCKET;
		return false;
	}

	outPort = ntohs(actual.sin_port);

	// 5. 从控制连接本地端获取客户端看到的服务端 IP
	//    （m_dataListen bind 到 INADDR_ANY，getsockname 只返回端口，IP 是 0.0.0.0；
	//      改为从 m_ctrl 的本地地址 getsockname 拿到服务端网卡 IP）
	sockaddr_in serverAddr{};
	int serverLen = sizeof(serverAddr);
	if (::getsockname(m_ctrl, (sockaddr*)&serverAddr, &serverLen) == SOCKET_ERROR) {
		::closesocket(m_dataListen);
		m_dataListen = INVALID_SOCKET;
		return false;
	}
	char ipBuf[64] = { 0 };
	::inet_ntop(AF_INET, &serverAddr.sin_addr, ipBuf, sizeof(ipBuf));
	outIp = ipBuf;

	return true;
}

void CFtpSession::HandleLIST(const std::string& arg) {
	if (!m_authed) { 
		SendResponse(530, "Please login with USER and PASS."); 
		return; 
	}

	SendResponse(150, "About to open data connection for directory listing.");

	SOCKET dataSock = ::accept(m_dataListen, nullptr, nullptr);
	if (dataSock == INVALID_SOCKET) {
		SendResponse(425, "Can't open data connection.");
		return;
	}
	::closesocket(m_dataListen);
	m_dataListen = INVALID_SOCKET;

	std::string realPath;
	if (!ResolveRealPath(m_cwd, realPath)) {
		closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	struct _finddata_t fd;
	intptr_t handle = _findfirst((realPath + "\\*").c_str(), &fd);
	if (handle == -1) {
		closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Cannot open directory.");
		return;
	}

	while (_findnext(handle, &fd) == 0) {
		bool isDir = (fd.attrib & _A_SUBDIR) != 0;

		std::string line;
		line += isDir ? 'd' : '-';
		line += "rwxr-xr-x ";        // 固定权限
		line += isDir ? "2 " : "1 "; // 固定链接数
		line += "owner    group     "; // 固定用户/组

		// 真实大小
		line += std::to_string(fd.size);
		line += ' ';

		// 真实时间
		struct tm tm_local_tmp;
		localtime_s(&tm_local_tmp, &fd.time_write);   // 安全版本
		char timeBuf[32];
		strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M", &tm_local_tmp);
		line += timeBuf;
		line += ' ';
		line += fd.name;
		line += "\r\n";

		::send(dataSock, line.c_str(), (int)line.size(), 0);
	}

	_findclose(handle);          // 关目录句柄
	closesocket(dataSock);        // 关数据连接
	m_dataListen = INVALID_SOCKET; // 标记 Pasv 已完成，下一个命令要重新 PASV
	SendResponse(226, "Transfer complete.");

}

void CFtpSession::HandleRETR(const std::string& arg) {
	if (!m_authed) {
		SendResponse(530, "Please login with USER and PASS."); 
		return; 
	}
	if (arg.empty()) {
		SendResponse(501, "RETR requires a filename."); 
		return; 
	}

	SendResponse(150, "About to open data connection for file transfer.");

	SOCKET dataSock = ::accept(m_dataListen, nullptr, nullptr);
	if (dataSock == INVALID_SOCKET) {
		SendResponse(425, "Can't open data connection.");
		return;
	}
	::closesocket(m_dataListen);
	m_dataListen = INVALID_SOCKET;

	// 解析 arg，得到真实路径，检查合法性
	std::string realPath;
	if (!ResolveRealPath(m_cwd + "/" + arg, realPath)) {
		::closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	/*
	// 文件存在检查
	struct _stat st;
	if (_stat(realPath.c_str(), &st) != 0 || !(st.st_mode & _S_IFREG)) {
		::closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "File not found or not a regular file.");
		return;
	}
	*/

	std::ifstream file;
	file.open(realPath, std::ios::binary | std::ios::in);
	if (!file.is_open()) {
		::closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Cannot open file.");
		return;
	}

	// 循环读 + 发送
	char buf[4096];
	while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
		::send(dataSock, buf, (int)file.gcount(), 0);
	}

	file.close();
	::closesocket(dataSock);
	SendResponse(226, "Transfer complete.");

}

void CFtpSession::HandleSTOR(const std::string& arg) {
	if (!m_authed) {
		SendResponse(530, "Please login with USER and PASS."); 
		return; 
	}

	if (arg.empty()) {
		SendResponse(501, "RETR requires a filename.");
		return;
	}

	SendResponse(150, "About to open data connection for file transfer.");

	SOCKET dataSock = ::accept(m_dataListen, nullptr, nullptr);
	if (dataSock == INVALID_SOCKET) {
		SendResponse(425, "Can't open data connection.");
		return;
	}
	::closesocket(m_dataListen);
	m_dataListen = INVALID_SOCKET;

	// 解析 arg，得到真实路径，检查合法性
	std::string realPath;
	if (!ResolveRealPath(m_cwd + "/" + arg, realPath)) {
		::closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	std::ofstream file;
	file.open(realPath, std::ios::binary);
	if (!file.is_open()) {
		::closesocket(dataSock);
		m_dataListen = INVALID_SOCKET;
		SendResponse(550, "Cannot open file.");
		return;
	}

	// 循环写入
	char buf[4096];
	int n;
	while ((n = ::recv(dataSock, buf, sizeof(buf), 0)) > 0) {
		file.write(buf, n);
		if (file.fail()) {
			file.close();
			::closesocket(dataSock);
			SendResponse(452, "Error writing file.");
			return;
		}
	}

	if (n < 0) {
		file.close();
		::closesocket(dataSock);
		SendResponse(426, "Connection closed; transfer aborted.");
		return;
	}

	file.close();
	::closesocket(dataSock);
	SendResponse(226, "Transfer complete.");
}


void CFtpSession::HandleDELE(const std::string& arg) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }
	if (arg.empty()) { SendResponse(501, "DELE requires a filename."); return; }

	std::string realPath;
	if (!ResolveRealPath(m_cwd + "/" + arg, realPath)) {
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	if (::_unlink(realPath.c_str()) == 0) {
		SendResponse(250, "File deleted.");
	} else {
		SendResponse(550, "Cannot delete file.");
	}
}

void CFtpSession::HandleMKD(const std::string& arg) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }
	if (arg.empty()) { SendResponse(501, "MKD requires a directory name."); return; }

	std::string realPath;
	if (!ResolveRealPath(m_cwd + "/" + arg, realPath)) {
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	if (::_mkdir(realPath.c_str()) == 0 || errno == EEXIST) {
		SendResponse(257, "\"" + arg + "\" directory created.");
	} else {
		SendResponse(550, "Cannot create directory.");
	}
}

void CFtpSession::HandleRMD(const std::string& arg) {
	if (!m_authed) { SendResponse(530, "Please login with USER and PASS."); return; }
	if (arg.empty()) { SendResponse(501, "RMD requires a directory name."); return; }

	std::string realPath;
	if (!ResolveRealPath(m_cwd + "/" + arg, realPath)) {
		SendResponse(550, "Permission denied: path escapes sandbox.");
		return;
	}

	if (::_rmdir(realPath.c_str()) == 0) {
		SendResponse(250, "Directory removed.");
	} else {
		SendResponse(550, "Cannot remove directory.");
	}
}

// ============================================================
//  沙箱
// ============================================================

/**
 * @brief 虚拟路径 → 真实路径（防 ../ 越狱）
 *
 * 规则：
 *   1. 把 vpath 拆成段
 *   2. 在 m_root 下一段段拼
 *   3. 规范化后必须以 m_root 为前缀
 *   4. 不允许符号链接逃逸（教学简化：不做链接检查）
 */
bool CFtpSession::ResolveRealPath(const std::string& vpath, std::string& outReal) const {
	// vpath 形如 "/a/b/c" 或 "/"
	std::string combined;
	if (vpath.empty() || vpath[0] != '/') {
		combined = m_cwd + vpath;  // 相对路径
	}
	else {
		combined = vpath;
	}

	// 拆分 + 规范化
	std::vector<std::string> parts;
	std::string seg;
	for (char c : combined) {
		if (c == '/' || c == '\\') {
			if (!seg.empty()) {
				if (seg == "..") {
					if (!parts.empty()) parts.pop_back();
				}
				else if (seg != ".") {
					parts.push_back(seg);
				}
				seg.clear();
			}
		}
		else {
			seg.push_back(c);
		}
	}
	if (!seg.empty()) {
		if (seg == "..") {
			if (!parts.empty()) parts.pop_back();
		}
		else if (seg != ".") {
			parts.push_back(seg);
		}
	}

	// 拼真实路径
	outReal = m_root;
	for (const auto& p : parts) {
		outReal += p;
		outReal.push_back('\\');
	}
	// 至少保证 m_root 本身
	if (outReal.size() < m_root.size()) outReal = m_root;

	// 沙箱校验：outReal 必须以 m_root 为前缀
	if (outReal.compare(0, m_root.size(), m_root) != 0) {
		return false;
	}
	return true;
}

