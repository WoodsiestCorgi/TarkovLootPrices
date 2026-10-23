#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>

namespace {
constexpr wchar_t kWindowClass[] = L"TarkovLootPricesNativeOverlay";
constexpr COLORREF kColorKey = RGB(1, 2, 3);
constexpr int kWidth = 330;
constexpr int kHeight = 148;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kRefreshMs = 400;
constexpr int kToggleHotkey = 1;
constexpr int kQuitHotkey = 2;
bool g_enabled = true;
bool g_demo = false;
bool g_isShowing = false;

[[nodiscard]] bool TarkovIsForeground() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    wchar_t title[512]{};
    if (GetWindowTextW(foreground, title, 512) == 0) return false;
    std::wstring value(title);
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value.find(L"tarkov") != std::wstring::npos;
}

void DrawLine(HDC dc, RECT bounds, const wchar_t* text,
              COLORREF color, int fontSize, int weight = FW_NORMAL) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (!font) return;
    const HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &bounds, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
}

void Paint(HDC dc, const RECT& rect) {
    const HBRUSH transparent = CreateSolidBrush(kColorKey);
    FillRect(dc, &rect, transparent);
    DeleteObject(transparent);

    const RECT panel{0, 0, kWidth, kHeight};
    const HBRUSH background = CreateSolidBrush(RGB(24, 29, 27));
    FillRect(dc, &panel, background);
    DeleteObject(background);

    DrawLine(dc, RECT{16, 9, 320, 37}, L"TARKOV LOOT PRICES  |  v0.1", RGB(212, 207, 176), 19, FW_BOLD);
    if (g_demo) {
        DrawLine(dc, RECT{16, 41, 320, 65}, L"DEMO - SAMPLE PRICES ONLY", RGB(250, 177, 85), 16, FW_BOLD);
        DrawLine(dc, RECT{16, 67, 320, 91}, L"Graphics card             850 000 RUB", RGB(139, 232, 171), 16);
        DrawLine(dc, RECT{16, 94, 320, 118}, L"Bolts                         42 000 RUB", RGB(139, 232, 171), 16);
    } else {
        DrawLine(dc, RECT{16, 43, 320, 70}, L"No item detection in this build", RGB(250, 177, 85), 17);
        DrawLine(dc, RECT{16, 72, 320, 100}, L"Prices are not available yet", RGB(225, 225, 225), 16);
    }
    DrawLine(dc, RECT{16, 119, 320, 143}, L"Ctrl+Shift+P toggle  |  Ctrl+Shift+Q exit", RGB(166, 174, 171), 14);
}

void UpdatePlacement(HWND hwnd) {
    const bool shouldShow = g_enabled && (g_demo || TarkovIsForeground());
    if (!shouldShow) {
        if (g_isShowing) {
            ShowWindow(hwnd, SW_HIDE);
            g_isShowing = false;
        }
        return;
    }

    RECT target{};
    if (g_demo) {
        target = RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    } else if (!GetWindowRect(GetForegroundWindow(), &target)) {
        return;
    }

    const int x = std::max(target.left, target.right - kWidth - 24);
    const int y = target.top + 120;
    RECT current{};
    if (!GetWindowRect(hwnd, &current) || current.left != x || current.top != y) {
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, kWidth, kHeight,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    if (!g_isShowing) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        g_isShowing = true;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        RegisterHotKey(hwnd, kToggleHotkey, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'P');
        RegisterHotKey(hwnd, kQuitHotkey, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'Q');
        SetTimer(hwnd, kTimerId, kRefreshMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId) UpdatePlacement(hwnd);
        return 0;
    case WM_HOTKEY:
        if (wParam == kToggleHotkey) {
            g_enabled = !g_enabled;
            UpdatePlacement(hwnd);
        } else if (wParam == kQuitHotkey) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        Paint(dc, rect);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        UnregisterHotKey(hwnd, kToggleHotkey);
        UnregisterHotKey(hwnd, kQuitHotkey);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    g_demo = commandLine && wcsstr(commandLine, L"--demo") != nullptr;
    SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = kWindowClass;
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClass, L"Tarkov Loot Prices", WS_POPUP,
        0, 0, kWidth, kHeight, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 2;

    SetLayeredWindowAttributes(hwnd, kColorKey, 0, LWA_COLORKEY);
    UpdatePlacement(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
