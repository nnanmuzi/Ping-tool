#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string>
#include <thread>
#include <winhttp.h>
#include <iphlpapi.h>
#include <sstream>

#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"iphlpapi.lib")

using namespace std;

HWND 输入框_IP, 输入框_次数, 按钮_开始;
HWND 标签_公网IP, 标签_网络速率;

UINT_PTR 定时器ID_更新 = 1;
HBRUSH h刷透明 = nullptr;

static HFONT g_大号字体 = nullptr;

wstring 格式化速率(DWORD bps) {
    double v = (double)bps;
    const wchar_t* 单位[] = { L"bps", L"Kbps", L"Mbps", L"Gbps" };
    int i = 0;
    while (v > 1024 && i < 3) {
        v /= 1024;
        i++;
    }
    wchar_t buf[32];
    swprintf(buf, 32, L"%.2f %s", v, 单位[i]);
    return buf;
}

wstring 获取网络速率() {
    ULONG 缓冲大小 = 0;
    if (GetIfTable(nullptr, &缓冲大小, FALSE) != ERROR_INSUFFICIENT_BUFFER) {
        return L"未知";
    }
    MIB_IFTABLE* 表 = (MIB_IFTABLE*)malloc(缓冲大小);
    if (!表) return L"未知";

    if (GetIfTable(表, &缓冲大小, FALSE) != NO_ERROR) {
        free(表);
        return L"未知";
    }

    wstring 速率 = L"未知";
    for (DWORD i = 0; i < 表->dwNumEntries; i++) {
        MIB_IFROW& 行 = 表->table[i];
        if (行.dwOperStatus == IF_OPER_STATUS_OPERATIONAL && 行.dwSpeed > 0) {
            速率 = 格式化速率(行.dwSpeed);
            break;
        }
    }
    free(表);
    return 速率;
}

void 获取公网IP(HWND 显示控件) {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    DWORD dwSize = 0;
    wstring 公网IP = L"公网IP: ";

    hSession = WinHttpOpen(L"木子网络工具/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        SetWindowTextW(显示控件, L"公网IP: 获取失败");
        goto 清理;
    }

    hConnect = WinHttpConnect(hSession, L"ipinfo.io",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        SetWindowTextW(显示控件, L"公网IP: 获取失败");
        goto 清理;
    }

    hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/ip",
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        SetWindowTextW(显示控件, L"公网IP: 获取失败");
        goto 清理;
    }

    if (!WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0,
        0, 0)) {
        SetWindowTextW(显示控件, L"公网IP: 获取失败");
        goto 清理;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        SetWindowTextW(显示控件, L"公网IP: 获取失败");
        goto 清理;
    }

    do {
        DWORD dwDownloaded = 0;
        char szBuffer[128] = { 0 };

        if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
            break;
        if (dwSize == 0)
            break;
        if (!WinHttpReadData(hRequest, szBuffer, min(dwSize, sizeof(szBuffer) - 1), &dwDownloaded))
            break;

        szBuffer[dwDownloaded] = 0;

        wchar_t bufw[128] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, szBuffer, -1, bufw, 128);

        公网IP += bufw;

    } while (dwSize > 0);

    SetWindowTextW(显示控件, 公网IP.c_str());

清理:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
}

void 点击开始(HWND 窗口) {
    wchar_t IP缓冲[128], 次数缓冲[16];
    GetWindowTextW(输入框_IP, IP缓冲, 128);
    GetWindowTextW(输入框_次数, 次数缓冲, 16);

    wstring IP地址 = IP缓冲;
    int 次数 = _wtoi(次数缓冲);

    if (IP地址.empty() || 次数 <= 0 || 次数 > 100) {
        MessageBoxW(窗口, L"请输入有效 IP 和次数（1-100）", L"错误", MB_ICONWARNING);
        return;
    }

    wstring CMD命令 = L"/k title 木子网络工具 && ping " + IP地址 + L" -n " + to_wstring(次数);
    ShellExecuteW(nullptr, L"open", L"cmd.exe", CMD命令.c_str(), nullptr, SW_SHOWDEFAULT);
}

LRESULT CALLBACK 窗口过程(HWND 窗口, UINT 消息, WPARAM wParam, LPARAM lParam) {
    switch (消息) {
    case WM_CREATE: {
        h刷透明 = CreateSolidBrush(RGB(255, 255, 255));

        CreateWindowW(L"STATIC", L"目标 IP：", WS_VISIBLE | WS_CHILD,
            20, 30, 120, 40, 窗口, nullptr, nullptr, nullptr);
        输入框_IP = CreateWindowW(L"EDIT", L"8.8.8.8", WS_VISIBLE | WS_CHILD | WS_BORDER,
            140, 30, 250, 40, 窗口, nullptr, nullptr, nullptr);

        CreateWindowW(L"STATIC", L"次数：", WS_VISIBLE | WS_CHILD,
            420, 30, 60, 40, 窗口, nullptr, nullptr, nullptr);
        输入框_次数 = CreateWindowW(L"EDIT", L"4", WS_VISIBLE | WS_CHILD | WS_BORDER,
            480, 30, 60, 40, 窗口, nullptr, nullptr, nullptr);

        按钮_开始 = CreateWindowW(L"BUTTON", L"开始 Ping", WS_VISIBLE | WS_CHILD,
            560, 30, 120, 40, 窗口, (HMENU)1, nullptr, nullptr);

        标签_公网IP = CreateWindowW(L"STATIC", L"公网IP: 获取中...", WS_VISIBLE | WS_CHILD,
            20, 100, 660, 30, 窗口, nullptr, nullptr, nullptr);

        标签_网络速率 = CreateWindowW(L"STATIC", L"网络速率: 获取中...", WS_VISIBLE | WS_CHILD,
            20, 140, 660, 30, 窗口, nullptr, nullptr, nullptr);

        HFONT 系统字体默认 = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        LOGFONT lf;
        GetObject(系统字体默认, sizeof(lf), &lf);

        int dpiY = GetDeviceCaps(GetDC(窗口), LOGPIXELSY);
        lf.lfHeight = -MulDiv(14, dpiY, 52);
        HFONT 输入框字体 = CreateFontIndirect(&lf);

        SendMessageW(输入框_IP, WM_SETFONT, (WPARAM)输入框字体, TRUE);
        SendMessageW(输入框_次数, WM_SETFONT, (WPARAM)输入框字体, TRUE);

        // 设置大号字体给公网IP和网络速率标签（16pt）
        lf.lfHeight = -MulDiv(16, dpiY, 72);
        g_大号字体 = CreateFontIndirect(&lf);
        SendMessageW(标签_公网IP, WM_SETFONT, (WPARAM)g_大号字体, TRUE);
        SendMessageW(标签_网络速率, WM_SETFONT, (WPARAM)g_大号字体, TRUE);

        // 其它控件使用系统默认字体
        SendMessageW(按钮_开始, WM_SETFONT, (WPARAM)系统字体默认, TRUE);

        thread([=]() {
            获取公网IP(标签_公网IP);
            }).detach();

        SetTimer(窗口, 定时器ID_更新, 5000, nullptr);
        break;
    }
    case WM_TIMER:
        if (wParam == 定时器ID_更新) {
            thread([=]() {
                获取公网IP(标签_公网IP);
                }).detach();
            wstring 速率 = L"网络速率: " + 获取网络速率();
            SetWindowTextW(标签_网络速率, 速率.c_str());
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (INT_PTR)h刷透明;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetBkMode(hdcEdit, TRANSPARENT);
        SetBkColor(hdcEdit, RGB(255, 255, 255));
        return (INT_PTR)h刷透明;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) 点击开始(窗口);
        break;
    case WM_DESTROY:
        KillTimer(窗口, 定时器ID_更新);
        if (g_大号字体) DeleteObject(g_大号字体);
        DeleteObject(h刷透明);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(窗口, 消息, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t* 类名 = L"木子Ping窗口";

    WNDCLASSW wc{};
    wc.lpfnWndProc = 窗口过程;
    wc.hInstance = hInst;
    wc.lpszClassName = 类名;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        类名,
        L"木子网络工具 - 一键 Ping",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 220,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
