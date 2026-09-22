#include <windows.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>

#include "PathUtils.h"
#include "FileAssociation.h"
#include "Resource.h"
#include "Settings.h"
#include "SteamDetector.h"
#include "VRChatLauncher.h"
#include "VrcwDropTarget.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Uxtheme.lib")

namespace
{
    const wchar_t WINDOW_CLASS[] = L"VRCWLauncherWindow";
    const wchar_t WINDOW_TITLE[] = L"VRCW Launcher";

    //
    // Theme colours. ApplyThemeColors fills them in for the
    // light or the dark appearance.
    //
    COLORREF BACKGROUND_COLOR = RGB(246, 246, 248);
    COLORREF CARD_COLOR = RGB(255, 255, 255);
    COLORREF TEXT_COLOR = RGB(28, 28, 30);
    COLORREF SECONDARY_TEXT_COLOR = RGB(108, 108, 112);
    COLORREF BORDER_COLOR = RGB(226, 226, 229);
    COLORREF SECONDARY_BUTTON_COLOR = RGB(237, 237, 239);
    COLORREF SECONDARY_BUTTON_HOVER_TOP_COLOR = RGB(250, 250, 252);
    COLORREF SECONDARY_BUTTON_HOVER_BOTTOM_COLOR = RGB(230, 230, 234);
    COLORREF SECONDARY_BUTTON_PRESSED_COLOR = RGB(219, 219, 224);
    COLORREF ACCENT_COLOR = RGB(0, 122, 255);
    COLORREF ACCENT_HOVER_TOP_COLOR = RGB(46, 145, 255);
    COLORREF ACCENT_HOVER_BOTTOM_COLOR = RGB(0, 110, 232);
    COLORREF ACCENT_PRESSED_COLOR = RGB(0, 96, 200);
    COLORREF ON_ACCENT_COLOR = RGB(255, 255, 255);
    COLORREF SUCCESS_COLOR = RGB(52, 199, 89);
    COLORREF DISABLED_BUTTON_COLOR = RGB(233, 233, 236);
    COLORREF DISABLED_TEXT_COLOR = RGB(163, 163, 168);
    COLORREF ATTENTION_COLOR = RGB(255, 149, 0);
    COLORREF DROP_HOVER_BACKGROUND_COLOR = RGB(242, 247, 255);
    COLORREF ROW_HOVER_COLOR = RGB(240, 240, 243);
    COLORREF ROW_PRESSED_COLOR = RGB(228, 228, 233);

    bool g_darkMode = false;

    int AdjustChannel(
        int value,
        int amount)
    {
        if (amount >= 0)
        {
            return value +
                ((255 - value) * amount) / 255;
        }

        return value +
            (value * amount) / 255;
    }

    //
    // Moves a colour towards white (positive) or black
    // (negative). Used to build the hover and pressed
    // shades of the system accent colour.
    //
    COLORREF AdjustColor(
        COLORREF color,
        int amount)
    {
        return RGB(
            AdjustChannel(
                GetRValue(color),
                amount),
            AdjustChannel(
                GetGValue(color),
                amount),
            AdjustChannel(
                GetBValue(color),
                amount));
    }

    bool IsLightColor(
        COLORREF color)
    {
        const int luminance =
            (GetRValue(color) * 299 +
                GetGValue(color) * 587 +
                GetBValue(color) * 114) / 1000;

        return luminance >= 160;
    }

    //
    // The accent colour the user picked in Windows.
    // Windows stores it as 0xAABBGGRR.
    //
    COLORREF ReadSystemAccentColor()
    {
        HKEY key = nullptr;

        const LSTATUS opened =
            RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\DWM",
                0,
                KEY_QUERY_VALUE,
                &key);

        if (opened == ERROR_SUCCESS)
        {
            DWORD value = 0;
            DWORD size = sizeof(value);
            DWORD type = 0;

            const LSTATUS status =
                RegQueryValueExW(
                    key,
                    L"AccentColor",
                    nullptr,
                    &type,
                    reinterpret_cast<LPBYTE>(&value),
                    &size);

            RegCloseKey(key);

            if (status == ERROR_SUCCESS &&
                type == REG_DWORD)
            {
                const COLORREF color = RGB(
                    value & 0xFF,
                    (value >> 8) & 0xFF,
                    (value >> 16) & 0xFF);

                if (color != RGB(0, 0, 0))
                {
                    return color;
                }
            }
        }

        //
        // Fall back to the colour Windows uses for
        // window frames.
        //
        DWORD colorization = 0;
        BOOL opaque = FALSE;

        if (SUCCEEDED(
            DwmGetColorizationColor(
                &colorization,
                &opaque)))
        {
            return RGB(
                (colorization >> 16) & 0xFF,
                (colorization >> 8) & 0xFF,
                colorization & 0xFF);
        }

        return GetSysColor(
            COLOR_HIGHLIGHT);
    }

    void ApplyThemeColors()
    {
        //
        // Buttons and highlights use the accent colour
        // from the system, not a colour of our own.
        //
        const COLORREF accent =
            ReadSystemAccentColor();

        ACCENT_COLOR = accent;
        ACCENT_HOVER_TOP_COLOR = AdjustColor(accent, 52);
        ACCENT_HOVER_BOTTOM_COLOR = AdjustColor(accent, 16);
        ACCENT_PRESSED_COLOR = AdjustColor(accent, -64);

        //
        // Light accents need dark text to stay readable.
        //
        ON_ACCENT_COLOR =
            IsLightColor(accent)
                ? RGB(28, 28, 30)
                : RGB(255, 255, 255);

        if (g_darkMode)
        {
            BACKGROUND_COLOR = RGB(28, 28, 30);
            CARD_COLOR = RGB(44, 44, 46);
            TEXT_COLOR = RGB(245, 245, 247);
            SECONDARY_TEXT_COLOR = RGB(152, 152, 157);
            BORDER_COLOR = RGB(58, 58, 60);
            SECONDARY_BUTTON_COLOR = RGB(58, 58, 60);
            SECONDARY_BUTTON_HOVER_TOP_COLOR = RGB(84, 84, 86);
            SECONDARY_BUTTON_HOVER_BOTTOM_COLOR = RGB(64, 64, 66);
            SECONDARY_BUTTON_PRESSED_COLOR = RGB(44, 44, 46);
            SUCCESS_COLOR = RGB(48, 209, 88);
            DISABLED_BUTTON_COLOR = RGB(52, 52, 54);
            DISABLED_TEXT_COLOR = RGB(112, 112, 117);
            ATTENTION_COLOR = RGB(255, 159, 10);
            DROP_HOVER_BACKGROUND_COLOR = RGB(30, 46, 66);
            ROW_HOVER_COLOR = RGB(62, 62, 64);
            ROW_PRESSED_COLOR = RGB(78, 78, 81);

            return;
        }

        BACKGROUND_COLOR = RGB(246, 246, 248);
        CARD_COLOR = RGB(255, 255, 255);
        TEXT_COLOR = RGB(28, 28, 30);
        SECONDARY_TEXT_COLOR = RGB(108, 108, 112);
        BORDER_COLOR = RGB(226, 226, 229);
        SECONDARY_BUTTON_COLOR = RGB(237, 237, 239);
        SECONDARY_BUTTON_HOVER_TOP_COLOR = RGB(255, 255, 255);
        SECONDARY_BUTTON_HOVER_BOTTOM_COLOR = RGB(228, 228, 234);
        SECONDARY_BUTTON_PRESSED_COLOR = RGB(213, 213, 219);
        SUCCESS_COLOR = RGB(52, 199, 89);
        DISABLED_BUTTON_COLOR = RGB(233, 233, 236);
        DISABLED_TEXT_COLOR = RGB(163, 163, 168);
        ATTENTION_COLOR = RGB(255, 149, 0);
        DROP_HOVER_BACKGROUND_COLOR = RGB(242, 247, 255);
        ROW_HOVER_COLOR = RGB(236, 236, 241);
        ROW_PRESSED_COLOR = RGB(222, 222, 229);
    }

    //
    // Windows keeps this setting per user. Zero means the
    // dark appearance is active.
    //
    bool IsDarkModeEnabled()
    {
        HKEY key = nullptr;

        const LSTATUS opened =
            RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows"
                L"\\CurrentVersion\\Themes\\Personalize",
                0,
                KEY_QUERY_VALUE,
                &key);

        if (opened != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD value = 1;
        DWORD size = sizeof(value);
        DWORD type = 0;

        const LSTATUS status =
            RegQueryValueExW(
                key,
                L"AppsUseLightTheme",
                nullptr,
                &type,
                reinterpret_cast<LPBYTE>(&value),
                &size);

        RegCloseKey(key);

        if (status != ERROR_SUCCESS ||
            type != REG_DWORD)
        {
            return false;
        }

        return value == 0;
    }

    //
    // Tells DWM to draw the title bar for the current
    // appearance.
    //
    void UpdateWindowTheme(
        HWND hwnd)
    {
        const BOOL darkMode =
            g_darkMode ? TRUE : FALSE;

        //
        // DWMWA_USE_IMMERSIVE_DARK_MODE
        //
        DwmSetWindowAttribute(
            hwnd,
            20,
            &darkMode,
            sizeof(darkMode));
    }

    //
    // Popup menus are drawn by the system and stay light
    // unless the process opts into the dark appearance.
    // uxtheme exposes that switch as an ordinal rather than
    // through the public headers.
    //
    using SetPreferredAppModeFn =
        int(WINAPI*)(int);

    using AllowDarkModeForAppFn =
        bool(WINAPI*)(bool);

    //
    // AllowDark. Zero means Default.
    //
    constexpr int PREFERRED_APP_MODE_ALLOW_DARK = 1;

    SetPreferredAppModeFn g_setPreferredAppMode = nullptr;
    AllowDarkModeForAppFn g_allowDarkModeForApp = nullptr;

    void InitializeDarkModeSupport()
    {
        HMODULE uxtheme =
            GetModuleHandleW(L"uxtheme.dll");

        if (uxtheme == nullptr)
        {
            uxtheme = LoadLibraryW(L"uxtheme.dll");
        }

        if (uxtheme == nullptr)
        {
            return;
        }

        g_setPreferredAppMode =
            reinterpret_cast<SetPreferredAppModeFn>(
                GetProcAddress(
                    uxtheme,
                    MAKEINTRESOURCEA(135)));

        g_allowDarkModeForApp =
            reinterpret_cast<AllowDarkModeForAppFn>(
                GetProcAddress(
                    uxtheme,
                    MAKEINTRESOURCEA(104)));
    }

    void UpdatePreferredAppMode()
    {
        if (g_allowDarkModeForApp != nullptr)
        {
            g_allowDarkModeForApp(g_darkMode);
        }

        if (g_setPreferredAppMode != nullptr)
        {
            g_setPreferredAppMode(
                g_darkMode
                    ? PREFERRED_APP_MODE_ALLOW_DARK
                    : 0);
        }
    }

    //
    // Identifies the payload sent to an already
    // running launcher through WM_COPYDATA.
    //
    constexpr DWORD COPYDATA_VRCW_PATH = 0x56524357;

    //
    // Window sizes, in logical units.
    //
    constexpr int WINDOW_WIDTH = 760;
    constexpr int WINDOW_HEIGHT = 600;

    //
    // The layout is measured inside the client area, while
    // this value sizes the whole window, so the recent
    // section needs the title bar and borders on top.
    //
    constexpr int WINDOW_HEIGHT_WITH_RECENT = 740;

    //
    // Recent section layout, in logical units.
    //
    constexpr int RECENT_HEADER_Y = 476;
    constexpr int RECENT_FIRST_ROW_Y = 502;
    constexpr int RECENT_ROW_HEIGHT = 28;
    constexpr int RECENT_MAX_ROWS = 3;
    constexpr int SEPARATOR_BOTTOM_OFFSET = 94;

    //
    // The VRChat status line sits in the empty band between
    // the VRChat heading and the recent heading.
    //
    constexpr int VRCHAT_HEADER_Y = 399;
    constexpr int STATUS_ROW_CENTER_Y = 447;
    constexpr int STATUS_DOT_RADIUS = 4;

    //
    // Smallest client size the layout still fits in. The
    // default window is 760x600, so the minimum stays just
    // below that and never forces the window to grow.
    //
    constexpr int MIN_WINDOW_WIDTH = 620;
    constexpr int MIN_WINDOW_HEIGHT = 560;

    //
    // Settings menu commands.
    //
    constexpr UINT MENU_KEEP_LAUNCHER_OPEN = 1;
    constexpr UINT MENU_ASSOCIATE_VRCW = 2;
    constexpr UINT MENU_CHOOSE_VRCHAT_FOLDER = 3;
    constexpr UINT MENU_CLEAR_RECENT_FILES = 4;
    constexpr UINT MENU_LANGUAGE = 5;
    constexpr UINT MENU_ABOUT = 6;

    //
    // Everything the pointer can hover over.
    //
    enum ButtonId
    {
        BUTTON_NONE = 0,
        BUTTON_CHOOSE_VRCW,
        BUTTON_CHANGE,
        BUTTON_LAUNCH,
        BUTTON_CHOOSE_VRCHAT_FOLDER,
        BUTTON_SETTINGS,
        BUTTON_RECENT_FIRST
    };

    std::wstring g_selectedVrcwPath;
    std::wstring g_vrchatExePath;
    std::wstring g_lastLaunchError;
    Settings::Data g_settings;
    bool g_dragHover = false;
    bool g_trackingMouse = false;
    int g_hoveredButton = BUTTON_NONE;
    int g_pressedButton = BUTTON_NONE;
    HWND g_mainWindow = nullptr;

    //
    // Base design size:
    // All layout values are designed at 96 DPI.
    //
    constexpr int BASE_DPI = 96;

    //
    // The interface can be English or Simplified Chinese.
    // Every user visible string lives in these two tables.
    //
    struct Strings
    {
        const wchar_t* dropFile;
        const wchar_t* orText;
        const wchar_t* chooseVrcw;
        const wchar_t* worldType;
        const wchar_t* change;
        const wchar_t* steamDetected;
        const wchar_t* steamNotFound;
        const wchar_t* chooseVrchatFolder;
        const wchar_t* selectVrchatFolderTitle;
        const wchar_t* recent;
        const wchar_t* missingSuffix;
        const wchar_t* launch;
        const wchar_t* menuKeepOpen;
        const wchar_t* menuAssociate;
        const wchar_t* menuClearRecent;
        const wchar_t* menuLanguage;
        const wchar_t* folderError;
        const wchar_t* associateError;
        const wchar_t* launchErrorMissing;
        const wchar_t* launchErrorWorldMissing;
        const wchar_t* launchErrorWorldGone;
        const wchar_t* launchErrorStart;
        const wchar_t* fontFamily;
        const wchar_t* menuAbout;
        const wchar_t* aboutText;
    };

    const Strings ENGLISH_STRINGS =
    {
        L"Drop a VRCW file here",
        L"or",
        L"Choose VRCW...",
        L"VRCW World",
        L"Change...",
        L"Steam version detected",
        L"Steam version not found",
        L"Choose VRChat Folder...",
        L"Select the VRChat folder",
        L"Recent",
        L"  (missing)",
        L"Launch",
        L"Keep Launcher Open",
        L"Associate .vrcw Files",
        L"Clear Recent Files",
        L"中文界面 (Chinese)",
        L"VRChat.exe was not found in that folder.",
        L"Windows could not update the .vrcw file association.",
        L"VRChat could not be found.",
        L"Choose a VRCW file first.",
        L"The selected VRCW file no longer exists.",
        L"VRChat could not be started.",
        L"Segoe UI",
        L"About",
        L"VRCW Launcher 1.0.2\r\n\r\nOpen source project:\r\ngithub.com/vosd04/vrcw-launcher\r\n\r\nThis is an unofficial fan tool. VRChat is a trademark of VRChat Inc., and this project is not affiliated with them in any way.\r\n\r\nThe software is free and open source. If you paid money for it, you have been scammed."
    };

    const Strings CHINESE_STRINGS =
    {
        L"把 VRCW 文件拖到这里",
        L"或",
        L"选择 VRCW…",
        L"VRCW 世界",
        L"更换…",
        L"已检测到 Steam 版",
        L"未找到 Steam 版",
        L"选择 VRChat 文件夹…",
        L"选择 VRChat 文件夹",
        L"最近",
        L"（文件已失效）",
        L"启动",
        L"启动后保留启动器",
        L"关联 .vrcw 文件",
        L"清空最近记录",
        L"中文界面 (Chinese)",
        L"该文件夹里没有找到 VRChat.exe。",
        L"Windows 无法更新 .vrcw 文件关联。",
        L"没有找到 VRChat。",
        L"请先选择一个 VRCW 文件。",
        L"所选的 VRCW 文件已不存在。",
        L"VRChat 启动失败。",
        L"Microsoft YaHei UI",
        L"关于",
        L"VRCW Launcher 1.0.2\r\n\r\n开源项目：\r\ngithub.com/vosd04/vrcw-launcher\r\n\r\n这是第三方非官方工具。VRChat 是 VRChat Inc. 的商标，本项目与 VRChat Inc. 没有任何关联。\r\n\r\n本软件完全免费开源。如果你是花钱买到的，那你被骗了。"
    };

    bool g_chineseUi = false;

    wchar_t ToLowerText(
        wchar_t character)
    {
        if (character >= L'A' &&
            character <= L'Z')
        {
            return character - L'A' + L'a';
        }

        return character;
    }

    bool EqualsText(
        const wchar_t* left,
        const wchar_t* right)
    {
        while (*left != L'\0' &&
            *right != L'\0')
        {
            if (ToLowerText(*left) !=
                ToLowerText(*right))
            {
                return false;
            }

            ++left;
            ++right;
        }

        return *left == L'\0' &&
            *right == L'\0';
    }

    bool ContainsText(
        const wchar_t* text,
        const wchar_t* word)
    {
        if (word == nullptr ||
            *word == L'\0')
        {
            return false;
        }

        for (const wchar_t* start = text;
            *start != L'\0';
            ++start)
        {
            const wchar_t* left = start;
            const wchar_t* right = word;

            while (*right != L'\0')
            {
                if (*left == L'\0' ||
                    ToLowerText(*left) !=
                    ToLowerText(*right))
                {
                    break;
                }

                ++left;
                ++right;
            }

            if (*right == L'\0')
            {
                return true;
            }
        }

        return false;
    }

    //
    // True when Windows itself is set to Simplified Chinese.
    // Traditional Chinese (Taiwan, Hong Kong, Macau) stays
    // English, together with every other language.
    //
    bool IsSystemSimplifiedChinese()
    {
        const LANGID language =
            GetUserDefaultUILanguage();

        if (PRIMARYLANGID(language) == LANG_CHINESE)
        {
            const WORD subLanguage =
                SUBLANGID(language);

            if (subLanguage ==
                SUBLANG_CHINESE_SIMPLIFIED ||
                subLanguage ==
                SUBLANG_CHINESE_SINGAPORE)
            {
                return true;
            }

            if (subLanguage ==
                SUBLANG_CHINESE_TRADITIONAL ||
                subLanguage ==
                SUBLANG_CHINESE_HONGKONG ||
                subLanguage ==
                SUBLANG_CHINESE_MACAU)
            {
                return false;
            }
        }

        //
        // Newer Windows versions report a pseudo language id
        // for Chinese, so the locale name is checked as well.
        //
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};

        if (GetUserDefaultLocaleName(
            localeName,
            LOCALE_NAME_MAX_LENGTH) > 0)
        {
            if (EqualsText(localeName, L"zh-Hans") ||
                ContainsText(localeName, L"zh-Hans-") ||
                EqualsText(localeName, L"zh-CN") ||
                EqualsText(localeName, L"zh-SG") ||
                EqualsText(localeName, L"zh"))
            {
                return true;
            }
        }

        return false;
    }

    const Strings& GetStrings()
    {
        return g_chineseUi
            ? CHINESE_STRINGS
            : ENGLISH_STRINGS;
    }

    int ScaleValue(
        HWND hwnd,
        int value)
    {
        UINT dpi = GetDpiForWindow(hwnd);

        if (dpi == 0)
        {
            dpi = BASE_DPI;
        }

        return MulDiv(
            value,
            static_cast<int>(dpi),
            BASE_DPI);
    }

    HFONT CreateFontForUI(
        HWND hwnd,
        int size,
        int weight)
    {
        HDC hdc = GetDC(hwnd);

        int dpi = GetDeviceCaps(
            hdc,
            LOGPIXELSY);

        int height = -MulDiv(
            size,
            dpi,
            72);

        ReleaseDC(
            hwnd,
            hdc);

        return CreateFontW(
            height,
            0,
            0,
            0,
            weight,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            GetStrings().fontFamily);
    }

    struct FontCacheEntry
    {
        int dpi;
        int size;
        int weight;
        HFONT font;
    };

    std::vector<FontCacheEntry> g_fontCache;

    //
    // The interface reuses a handful of fonts for every
    // repaint. Creating them once keeps repainting cheap
    // and keeps the GDI object count flat.
    //
    HFONT GetFontForUI(
        HWND hwnd,
        int size,
        int weight)
    {
        HDC hdc = GetDC(hwnd);

        const int dpi = GetDeviceCaps(
            hdc,
            LOGPIXELSY);

        ReleaseDC(
            hwnd,
            hdc);

        for (const FontCacheEntry& entry : g_fontCache)
        {
            if (entry.dpi == dpi &&
                entry.size == size &&
                entry.weight == weight)
            {
                return entry.font;
            }
        }

        FontCacheEntry entry{};

        entry.dpi = dpi;
        entry.size = size;
        entry.weight = weight;
        entry.font = CreateFontForUI(
            hwnd,
            size,
            weight);

        g_fontCache.push_back(entry);

        return entry.font;
    }

    //
    // Fonts are created for a specific DPI, so the cache
    // has to be rebuilt when the window moves to a
    // display with a different scaling factor.
    //
    void ClearFontCache()
    {
        for (const FontCacheEntry& entry : g_fontCache)
        {
            DeleteObject(entry.font);
        }

        g_fontCache.clear();
    }

    struct BrushCacheEntry
    {
        COLORREF color;
        HBRUSH brush;
    };

    struct PenCacheEntry
    {
        int style;
        int width;
        COLORREF color;
        HPEN pen;
    };

    std::vector<BrushCacheEntry> g_brushCache;
    std::vector<PenCacheEntry> g_penCache;

    //
    // The window paints with a small, fixed set of brushes
    // and pens. Caching them keeps the GDI object count
    // steady instead of creating and destroying objects on
    // every repaint.
    //
    HBRUSH GetSolidBrush(
        COLORREF color)
    {
        for (const BrushCacheEntry& entry : g_brushCache)
        {
            if (entry.color == color)
            {
                return entry.brush;
            }
        }

        BrushCacheEntry entry{};

        entry.color = color;
        entry.brush = CreateSolidBrush(color);

        g_brushCache.push_back(entry);

        return entry.brush;
    }

    HPEN GetPen(
        int style,
        int width,
        COLORREF color)
    {
        for (const PenCacheEntry& entry : g_penCache)
        {
            if (entry.style == style &&
                entry.width == width &&
                entry.color == color)
            {
                return entry.pen;
            }
        }

        PenCacheEntry entry{};

        entry.style = style;
        entry.width = width;
        entry.color = color;
        entry.pen = CreatePen(
            style,
            width,
            color);

        g_penCache.push_back(entry);

        return entry.pen;
    }

    void ClearGdiCache()
    {
        for (const BrushCacheEntry& entry : g_brushCache)
        {
            DeleteObject(entry.brush);
        }

        g_brushCache.clear();

        for (const PenCacheEntry& entry : g_penCache)
        {
            DeleteObject(entry.pen);
        }

        g_penCache.clear();
    }

    //
    // Draws one line of text inside a rectangle: vertically
    // centred, and either centred horizontally or cut off
    // with an ellipsis. Text drawn with TextOut sits on the
    // top of the font cell, which is why the rows above used
    // to look off centre.
    //
    void DrawTextInRect(
        HWND hwnd,
        HDC hdc,
        const wchar_t* text,
        int left,
        int top,
        int right,
        int bottom,
        int size,
        COLORREF color,
        int weight,
        bool centered,
        bool ellipsis)
    {
        if (right <= left ||
            bottom <= top)
        {
            return;
        }

        HFONT font = GetFontForUI(
            hwnd,
            size,
            weight);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            color);

        RECT rect{};

        rect.left = ScaleValue(hwnd, left);
        rect.top = ScaleValue(hwnd, top);
        rect.right = ScaleValue(hwnd, right);
        rect.bottom = ScaleValue(hwnd, bottom);

        UINT format =
            DT_SINGLELINE |
            DT_VCENTER |
            DT_NOPREFIX;

        format |= centered
            ? DT_CENTER
            : DT_LEFT;

        if (ellipsis)
        {
            format |= DT_END_ELLIPSIS;
        }

        DrawTextW(
            hdc,
            text,
            -1,
            &rect,
            format);

        SelectObject(
            hdc,
            oldFont);
    }

    void DrawTextUI(
        HWND hwnd,
        HDC hdc,
        const wchar_t* text,
        int x,
        int y,
        int size,
        COLORREF color,
        int weight)
    {
        HFONT font = GetFontForUI(
            hwnd,
            size,
            weight);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            color);

        TextOutW(
            hdc,
            ScaleValue(hwnd, x),
            ScaleValue(hwnd, y),
            text,
            lstrlenW(text));

        SelectObject(
            hdc,
            oldFont);
    }

    void DrawCenteredText(
        HWND hwnd,
        HDC hdc,
        const wchar_t* text,
        int centerX,
        int y,
        int size,
        COLORREF color,
        int weight)
    {
        HFONT font = GetFontForUI(
            hwnd,
            size,
            weight);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        SIZE textSize{};

        GetTextExtentPoint32W(
            hdc,
            text,
            lstrlenW(text),
            &textSize);

        int scaledCenterX =
            ScaleValue(hwnd, centerX);

        int scaledY =
            ScaleValue(hwnd, y);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            color);

        TextOutW(
            hdc,
            scaledCenterX - (textSize.cx / 2),
            scaledY,
            text,
            lstrlenW(text));

        SelectObject(
            hdc,
            oldFont);
    }

    //
    // Draws text that ends with an ellipsis when it does
    // not fit. File names can be arbitrarily long.
    //
    void DrawTextEllipsized(
        HWND hwnd,
        HDC hdc,
        const wchar_t* text,
        int x,
        int y,
        int size,
        COLORREF color,
        int weight,
        int maxX)
    {
        if (maxX <= x)
        {
            return;
        }

        HFONT font = GetFontForUI(
            hwnd,
            size,
            weight);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            color);

        RECT rect{};

        rect.left = ScaleValue(hwnd, x);
        rect.top = ScaleValue(hwnd, y);
        rect.right = ScaleValue(hwnd, maxX);
        rect.bottom =
            rect.top +
            ScaleValue(hwnd, size * 3);

        DrawTextW(
            hdc,
            text,
            -1,
            &rect,
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_NOPREFIX |
            DT_LEFT |
            DT_TOP);

        SelectObject(
            hdc,
            oldFont);
    }

    //
    // A document with a folded corner. The outline stops where
    // the fold starts, so the corner is not drawn twice, and an
    // optional label sits in the middle of the sheet.
    //
    void DrawFileIcon(
        HWND hwnd,
        HDC hdc,
        int left,
        int top,
        int width,
        int height,
        int foldSize,
        COLORREF color,
        int labelSize,
        const wchar_t* label)
    {
        const int right = left + width;
        const int bottom = top + height;
        const int foldX = right - foldSize;
        const int foldY = top + foldSize;

        HPEN pen = GetPen(
            PS_SOLID,
            ScaleValue(hwnd, 2),
            color);

        HPEN oldPen =
            (HPEN)SelectObject(
                hdc,
                pen);

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(
                hdc,
                GetStockObject(
                    NULL_BRUSH));

        POINT outline[5] =
        {
            { ScaleValue(hwnd, left), ScaleValue(hwnd, bottom) },
            { ScaleValue(hwnd, left), ScaleValue(hwnd, top) },
            { ScaleValue(hwnd, foldX), ScaleValue(hwnd, top) },
            { ScaleValue(hwnd, right), ScaleValue(hwnd, foldY) },
            { ScaleValue(hwnd, right), ScaleValue(hwnd, bottom) }
        };

        Polyline(
            hdc,
            outline,
            5);

        MoveToEx(
            hdc,
            outline[4].x,
            outline[4].y,
            nullptr);

        LineTo(
            hdc,
            outline[0].x,
            outline[0].y);

        //
        // The folded corner itself.
        //
        MoveToEx(
            hdc,
            outline[2].x,
            outline[2].y,
            nullptr);

        LineTo(
            hdc,
            ScaleValue(hwnd, foldX),
            ScaleValue(hwnd, foldY));

        LineTo(
            hdc,
            ScaleValue(hwnd, right),
            ScaleValue(hwnd, foldY));

        SelectObject(
            hdc,
            oldBrush);

        SelectObject(
            hdc,
            oldPen);

        if (label != nullptr &&
            labelSize > 0)
        {
            //
            // Centred in the part of the sheet below the fold.
            //
            DrawTextInRect(
                hwnd,
                hdc,
                label,
                left + 2,
                foldY + 1,
                right - 2,
                bottom - 1,
                labelSize,
                color,
                FW_NORMAL,
                true,
                false);
        }
    }

    void FillRoundedRect(
        HWND hwnd,
        HDC hdc,
        int left,
        int top,
        int right,
        int bottom,
        int radius,
        COLORREF color)
    {
        HBRUSH brush =
            GetSolidBrush(color);

        HPEN pen =
            GetPen(
                PS_NULL,
                1,
                color);

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(
                hdc,
                brush);

        HPEN oldPen =
            (HPEN)SelectObject(
                hdc,
                pen);

        RoundRect(
            hdc,
            ScaleValue(hwnd, left),
            ScaleValue(hwnd, top),
            ScaleValue(hwnd, right),
            ScaleValue(hwnd, bottom),
            ScaleValue(hwnd, radius),
            ScaleValue(hwnd, radius));

        SelectObject(
            hdc,
            oldPen);

        SelectObject(
            hdc,
            oldBrush);
    }

    void DrawRoundedOutline(
        HWND hwnd,
        HDC hdc,
        int left,
        int top,
        int right,
        int bottom,
        int radius,
        COLORREF color,
        int lineWidth = 1)
    {
        HPEN pen =
            GetPen(
                PS_SOLID,
                ScaleValue(hwnd, lineWidth),
                color);

        HBRUSH brush =
            (HBRUSH)GetStockObject(
                NULL_BRUSH);

        HPEN oldPen =
            (HPEN)SelectObject(
                hdc,
                pen);

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(
                hdc,
                brush);

        RoundRect(
            hdc,
            ScaleValue(hwnd, left),
            ScaleValue(hwnd, top),
            ScaleValue(hwnd, right),
            ScaleValue(hwnd, bottom),
            ScaleValue(hwnd, radius),
            ScaleValue(hwnd, radius));

        SelectObject(
            hdc,
            oldBrush);

        SelectObject(
            hdc,
            oldPen);
    }

    bool SetSelectedVrcwPath(
        const std::wstring& path);

    int GetInitialWindowHeight();

    void EnsureRecentSectionFits(
        HWND hwnd);

    int ToLogical(
        HWND hwnd,
        int deviceValue);

    void GetLogicalClientSize(
        HWND hwnd,
        int& widthLogical,
        int& heightLogical);

    bool ChooseVRCWFile(
        HWND owner)
    {
        IFileOpenDialog* dialog = nullptr;

        HRESULT result =
            CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog));

        if (FAILED(result))
        {
            return false;
        }

        const COMDLG_FILTERSPEC fileType =
        {
            L"VRCW World Files",
            L"*.vrcw"
        };

        dialog->SetFileTypes(
            1,
            &fileType);

        dialog->SetFileTypeIndex(1);

        DWORD options = 0;

        if (SUCCEEDED(
            dialog->GetOptions(&options)))
        {
            dialog->SetOptions(
                options |
                FOS_FORCEFILESYSTEM |
                FOS_FILEMUSTEXIST |
                FOS_STRICTFILETYPES);
        }

        result = dialog->Show(owner);

        if (result != S_OK)
        {
            dialog->Release();
            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();
            return false;
        }

        PWSTR filePath = nullptr;

        result =
            item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &filePath);

        if (SUCCEEDED(result) &&
            filePath != nullptr)
        {
            const bool accepted =
                SetSelectedVrcwPath(
                    filePath);

            CoTaskMemFree(filePath);

            item->Release();
            dialog->Release();

            return accepted;
        }

        if (filePath != nullptr)
        {
            CoTaskMemFree(filePath);
        }

        item->Release();
        dialog->Release();

        return false;
    }

    void InvalidateMainWindow()
    {
        if (g_mainWindow != nullptr)
        {
            InvalidateRect(
                g_mainWindow,
                nullptr,
                TRUE);
        }
    }

    bool SetSelectedVrcwPath(
        const std::wstring& path)
    {
        if (!PathUtils::IsVrcwFile(path) ||
            !PathUtils::FileExists(path))
        {
            return false;
        }

        g_selectedVrcwPath = path;

        InvalidateMainWindow();

        return true;
    }

    //
    // Only used when Steam detection failed, so the
    // user can point the launcher at VRChat themselves.
    //
    bool ChooseVRChatFolder(
        HWND owner)
    {
        IFileOpenDialog* dialog = nullptr;

        HRESULT result =
            CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog));

        if (FAILED(result))
        {
            return false;
        }

        DWORD options = 0;

        if (SUCCEEDED(
            dialog->GetOptions(&options)))
        {
            dialog->SetOptions(
                options |
                FOS_PICKFOLDERS |
                FOS_FORCEFILESYSTEM |
                FOS_PATHMUSTEXIST);
        }

        dialog->SetTitle(
            GetStrings().selectVrchatFolderTitle);

        result = dialog->Show(owner);

        if (result != S_OK)
        {
            dialog->Release();

            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();

            return false;
        }

        PWSTR folderPath = nullptr;

        result =
            item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &folderPath);

        bool found = false;

        if (SUCCEEDED(result) &&
            folderPath != nullptr)
        {
            const std::wstring vrchatExePath =
                SteamDetector::FindVRChatExecutableInFolder(
                    folderPath);

            if (!vrchatExePath.empty())
            {
                g_vrchatExePath = vrchatExePath;

                //
                // Remember the manual choice so the user
                // only has to make it once.
                //
                g_settings.vrchatExecutablePath =
                    vrchatExePath;

                Settings::Save(g_settings);

                found = true;
            }

            CoTaskMemFree(folderPath);
        }

        item->Release();
        dialog->Release();

        if (!found)
        {
            MessageBoxW(
                owner,
                GetStrings().folderError,
                WINDOW_TITLE,
                MB_OK | MB_ICONINFORMATION);
        }

        return found;
    }

    const wchar_t* DescribeLaunchError(
        VRChatLauncher::LaunchError error)
    {
        switch (error)
        {
        case VRChatLauncher::LaunchError::VrchatExecutableMissing:
            return GetStrings().launchErrorMissing;

        case VRChatLauncher::LaunchError::WorldFileMissing:
            return GetStrings().launchErrorWorldMissing;

        case VRChatLauncher::LaunchError::WorldFileNotFound:
            return GetStrings().launchErrorWorldGone;

        default:
            return GetStrings().launchErrorStart;
        }
    }

    bool TryLaunch()
    {
        g_lastLaunchError.clear();

        const VRChatLauncher::LaunchError error =
            VRChatLauncher::Launch(
                g_vrchatExePath,
                g_selectedVrcwPath);

        if (error != VRChatLauncher::LaunchError::None)
        {
            g_lastLaunchError =
                DescribeLaunchError(error);

            return false;
        }

        //
        // Only worlds that were actually started are
        // remembered.
        //
        Settings::AddRecentFile(
            g_settings,
            g_selectedVrcwPath);

        Settings::Save(g_settings);

        return true;
    }

    void PerformLaunch(
        HWND hwnd)
    {
        if (TryLaunch())
        {
            //
            // Default behaviour: the launcher is done
            // once VRChat has been started. The user can
            // ask it to stay open instead.
            //
            if (!g_settings.keepLauncherOpen)
            {
                DestroyWindow(hwnd);

                return;
            }

            //
            // The window has to be tall enough for the
            // recent list that was just updated.
            //
            EnsureRecentSectionFits(hwnd);

            //
            // The recent list just changed.
            //
            InvalidateRect(
                hwnd,
                nullptr,
                TRUE);

            return;
        }

        MessageBoxW(
            hwnd,
            g_lastLaunchError.c_str(),
            WINDOW_TITLE,
            MB_OK | MB_ICONERROR);
    }

    //
    // Returns the first .vrcw file passed to the
    // executable, which is how drag-onto-the-exe and
    // the .vrcw file association reach the launcher.
    //
    std::wstring GetVrcwPathFromCommandLine()
    {
        int argumentCount = 0;

        LPWSTR* arguments =
            CommandLineToArgvW(
                GetCommandLineW(),
                &argumentCount);

        if (arguments == nullptr)
        {
            return std::wstring();
        }

        std::wstring result;

        //
        // Index 0 is the executable itself.
        //
        for (int index = 1;
            index < argumentCount;
            ++index)
        {
            if (PathUtils::IsVrcwFile(arguments[index]) &&
                PathUtils::FileExists(arguments[index]))
            {
                result = arguments[index];

                break;
            }
        }

        LocalFree(arguments);

        return result;
    }

    void SendVrcwPathToWindow(
        HWND window,
        const std::wstring& path)
    {
        COPYDATASTRUCT data{};

        data.dwData = COPYDATA_VRCW_PATH;

        data.cbData = static_cast<DWORD>(
            (path.size() + 1) * sizeof(wchar_t));

        data.lpData = const_cast<wchar_t*>(
            path.c_str());

        SendMessageW(
            window,
            WM_COPYDATA,
            0,
            reinterpret_cast<LPARAM>(&data));
    }

    void OnVrcwDropped(
        const std::wstring& path)
    {
        SetSelectedVrcwPath(path);
    }

    void OnDragHoverChanged(
        bool hovering)
    {
        if (g_dragHover == hovering)
        {
            return;
        }

        g_dragHover = hovering;

        InvalidateMainWindow();
    }

    struct ButtonRect
    {
        int left;
        int top;
        int right;
        int bottom;
    };

    ButtonRect GetLaunchButtonRect(
        int widthLogical,
        int heightLogical)
    {
        ButtonRect rect{};

        rect.left = widthLogical - 188;
        rect.top = heightLogical - 70;
        rect.right = widthLogical - 48;
        rect.bottom = heightLogical - 24;

        return rect;
    }

    //
    // Text button on the VRChat row. It is only
    // shown when VRChat could not be detected.
    //
    ButtonRect GetChooseVrchatButtonRect(
        int widthLogical)
    {
        ButtonRect rect{};

        rect.right = widthLogical - 72;
        rect.left = rect.right - 210;
        rect.top = STATUS_ROW_CENTER_Y - 16;
        rect.bottom = STATUS_ROW_CENTER_Y + 16;

        return rect;
    }

    //
    // Launch is only enabled when the app has
    // everything it needs to start VRChat.
    //
    bool IsLaunchEnabled()
    {
        if (g_selectedVrcwPath.empty())
        {
            return false;
        }

        if (g_vrchatExePath.empty())
        {
            return false;
        }

        return true;
    }

    //
    // How many recent entries fit above the separator.
    //
    int GetVisibleRecentCount(
        int heightLogical)
    {
        const int separatorY =
            heightLogical - SEPARATOR_BOTTOM_OFFSET;

        const int available =
            static_cast<int>(
                g_settings.recentVrcwFiles.size());

        int visible = 0;

        while (visible < available &&
            visible < RECENT_MAX_ROWS)
        {
            const int rowBottom =
                RECENT_FIRST_ROW_Y +
                (visible + 1) * RECENT_ROW_HEIGHT;

            if (rowBottom > separatorY - 16)
            {
                break;
            }

            ++visible;
        }

        return visible;
    }

    ButtonRect GetRecentRowRect(
        int widthLogical,
        int index)
    {
        ButtonRect rect{};

        rect.left = 48;
        rect.right = widthLogical - 48;
        rect.top =
            RECENT_FIRST_ROW_Y +
            index * RECENT_ROW_HEIGHT;
        rect.bottom =
            rect.top + RECENT_ROW_HEIGHT;

        return rect;
    }

    bool IsRecentFileMissing(
        const std::wstring& path)
    {
        return !PathUtils::FileExists(path);
    }

    //
    // The "..." button in the header.
    //
    ButtonRect GetSettingsButtonRect(
        int widthLogical)
    {
        ButtonRect rect{};

        rect.right = widthLogical - 52;
        rect.left = rect.right - 36;
        rect.top = 34;
        rect.bottom = 70;

        return rect;
    }

    //
    // Where the settings menu hangs from. It follows the
    // visible dots rather than the whole button, so the
    // panel does not float away from the button.
    //
    ButtonRect GetSettingsMenuAnchorRect(
        int widthLogical)
    {
        ButtonRect rect =
            GetSettingsButtonRect(widthLogical);

        rect.left += 8;
        rect.right -= 6;
        rect.bottom += 4;

        return rect;
    }
    //
    // The settings menu is painted by the launcher itself.
    // A system popup menu ignores the dark appearance on
    // current Windows 11 builds, so the menu is a small
    // popup window that uses the same palette, fonts and
    // corner radius as the rest of the window.
    //
    constexpr wchar_t POPUP_MENU_CLASS[] =
        L"VRCWLauncherPopupMenu";

    constexpr int POPUP_ITEM_HEIGHT = 26;
    constexpr int POPUP_SEPARATOR_HEIGHT = 6;
    constexpr int POPUP_PANEL_PADDING = 4;
    constexpr int POPUP_CHECK_COLUMN = 20;
    constexpr int POPUP_LEFT_PADDING = 8;
    constexpr int POPUP_RIGHT_PADDING = 12;
    constexpr int POPUP_ITEM_RADIUS = 5;
    constexpr int POPUP_PANEL_RADIUS = 8;
    constexpr int POPUP_ITEM_INSET = 4;
    constexpr int POPUP_FONT_SIZE = 12;

    struct PopupMenuItem
    {
        UINT command;
        const wchar_t* label;
        bool checked;
    };

    struct PopupMenuState
    {
        std::vector<PopupMenuItem> items;
        int widthLogical = 0;
        int heightLogical = 0;
        int hoveredIndex = -1;
        int pressedIndex = -1;
        UINT result = 0;
        bool open = false;
    };

    PopupMenuState g_popupMenu;

    int PopupItemIndexAt(
        int logicalY)
    {
        int top = POPUP_PANEL_PADDING;

        for (size_t index = 0;
            index < g_popupMenu.items.size();
            ++index)
        {
            const PopupMenuItem& item =
                g_popupMenu.items[index];

            const int height =
                item.label == nullptr
                    ? POPUP_SEPARATOR_HEIGHT
                    : POPUP_ITEM_HEIGHT;

            if (logicalY >= top &&
                logicalY < top + height)
            {
                return item.label == nullptr
                    ? -1
                    : static_cast<int>(index);
            }

            top += height;
        }

        return -1;
    }

    //
    // Rectangle of one menu entry, in logical units.
    //
    bool GetPopupItemRect(
        int index,
        ButtonRect& rect)
    {
        if (index < 0 ||
            index >= static_cast<int>(
                g_popupMenu.items.size()) ||
            g_popupMenu.items[
                static_cast<size_t>(index)].label == nullptr)
        {
            return false;
        }

        int top = POPUP_PANEL_PADDING;

        for (int current = 0;
            current < index;
            ++current)
        {
            top += g_popupMenu.items[
                static_cast<size_t>(current)].label == nullptr
                ? POPUP_SEPARATOR_HEIGHT
                : POPUP_ITEM_HEIGHT;
        }

        rect.left = POPUP_ITEM_INSET;
        rect.top = top;
        rect.right =
            g_popupMenu.widthLogical -
            POPUP_ITEM_INSET;
        rect.bottom = top + POPUP_ITEM_HEIGHT;

        return true;
    }

    //
    // Only the row whose state changed is repainted, which
    // keeps the menu from flashing while the pointer moves.
    //
    void InvalidatePopupItem(
        HWND hwnd,
        int index)
    {
        ButtonRect rect{};

        if (!GetPopupItemRect(index, rect))
        {
            return;
        }

        RECT deviceRect{};

        deviceRect.left = ScaleValue(hwnd, rect.left);
        deviceRect.top = ScaleValue(hwnd, rect.top);
        deviceRect.right = ScaleValue(hwnd, rect.right);
        deviceRect.bottom = ScaleValue(hwnd, rect.bottom);

        InflateRect(
            &deviceRect,
            2,
            2);

        InvalidateRect(
            hwnd,
            &deviceRect,
            FALSE);
    }

    void PaintPopupMenu(
        HWND hwnd,
        HDC hdc)
    {
        //
        // RoundRect does not fill its right and bottom edge, so
        // the whole client is filled first. Anything left
        // unpainted would show the window that sits behind the
        // menu.
        //
        RECT clientRect{};

        GetClientRect(
            hwnd,
            &clientRect);

        FillRect(
            hdc,
            &clientRect,
            GetSolidBrush(CARD_COLOR));

        DrawRoundedOutline(
            hwnd,
            hdc,
            0,
            0,
            g_popupMenu.widthLogical,
            g_popupMenu.heightLogical,
            POPUP_PANEL_RADIUS,
            BORDER_COLOR);

        int top = POPUP_PANEL_PADDING;

        for (int index = 0;
            index < static_cast<int>(
                g_popupMenu.items.size());
            ++index)
        {
            const PopupMenuItem& item =
                g_popupMenu.items[
                    static_cast<size_t>(index)];

            if (item.label == nullptr)
            {
                const int lineY =
                    top + POPUP_SEPARATOR_HEIGHT / 2;

                HPEN pen = GetPen(
                    PS_SOLID,
                    ScaleValue(hwnd, 1),
                    BORDER_COLOR);

                HPEN oldPen =
                    (HPEN)SelectObject(
                        hdc,
                        pen);

                MoveToEx(
                    hdc,
                    ScaleValue(
                        hwnd,
                        POPUP_ITEM_INSET),
                    ScaleValue(hwnd, lineY),
                    nullptr);

                LineTo(
                    hdc,
                    ScaleValue(
                        hwnd,
                        g_popupMenu.widthLogical -
                        POPUP_ITEM_INSET),
                    ScaleValue(hwnd, lineY));

                SelectObject(
                    hdc,
                    oldPen);

                top += POPUP_SEPARATOR_HEIGHT;

                continue;
            }

            const bool hovered =
                g_popupMenu.hoveredIndex == index;

            const bool pressed =
                g_popupMenu.pressedIndex == index;

            if (hovered || pressed)
            {
                FillRoundedRect(
                    hwnd,
                    hdc,
                    POPUP_ITEM_INSET,
                    top + 1,
                    g_popupMenu.widthLogical -
                        POPUP_ITEM_INSET,
                    top + POPUP_ITEM_HEIGHT - 1,
                    POPUP_ITEM_RADIUS,
                    pressed
                        ? ROW_PRESSED_COLOR
                        : ROW_HOVER_COLOR);
            }

            if (item.checked)
            {
                DrawTextInRect(
                    hwnd,
                    hdc,
                    L"\u2713",
                    POPUP_LEFT_PADDING + 1,
                    top,
                    POPUP_LEFT_PADDING +
                        POPUP_CHECK_COLUMN,
                    top + POPUP_ITEM_HEIGHT,
                    POPUP_FONT_SIZE,
                    ACCENT_COLOR,
                    FW_SEMIBOLD,
                    true,
                    false);
            }

            DrawTextInRect(
                hwnd,
                hdc,
                item.label,
                POPUP_LEFT_PADDING +
                    POPUP_CHECK_COLUMN,
                top,
                g_popupMenu.widthLogical -
                    POPUP_ITEM_INSET - 2,
                top + POPUP_ITEM_HEIGHT,
                POPUP_FONT_SIZE,
                TEXT_COLOR,
                FW_NORMAL,
                false,
                false);

            top += POPUP_ITEM_HEIGHT;
        }
    }

    LRESULT CALLBACK PopupMenuProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};

            HDC hdc = BeginPaint(
                hwnd,
                &ps);

            RECT clientRect{};

            GetClientRect(
                hwnd,
                &clientRect);

            const int width = clientRect.right;
            const int height = clientRect.bottom;

            //
            // The menu is drawn into a memory bitmap and copied
            // over in one go, so repainting a row never shows
            // the panel being cleared first.
            //
            HDC bufferDc = CreateCompatibleDC(hdc);

            HBITMAP bufferBitmap =
                bufferDc != nullptr
                    ? CreateCompatibleBitmap(
                        hdc,
                        width,
                        height)
                    : nullptr;

            if (bufferDc != nullptr &&
                bufferBitmap != nullptr)
            {
                HGDIOBJ oldBitmap =
                    SelectObject(
                        bufferDc,
                        bufferBitmap);

                PaintPopupMenu(
                    hwnd,
                    bufferDc);

                BitBlt(
                    hdc,
                    0,
                    0,
                    width,
                    height,
                    bufferDc,
                    0,
                    0,
                    SRCCOPY);

                SelectObject(
                    bufferDc,
                    oldBitmap);

                DeleteObject(bufferBitmap);
                DeleteDC(bufferDc);
            }
            else
            {
                PaintPopupMenu(
                    hwnd,
                    hdc);
            }

            EndPaint(
                hwnd,
                &ps);

            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
        {
            const int index =
                PopupItemIndexAt(
                    ToLogical(
                        hwnd,
                        GET_Y_LPARAM(lParam)));

            if (index != g_popupMenu.hoveredIndex)
            {
                const int previous =
                    g_popupMenu.hoveredIndex;

                g_popupMenu.hoveredIndex = index;

                InvalidatePopupItem(
                    hwnd,
                    previous);

                InvalidatePopupItem(
                    hwnd,
                    index);
            }

            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            const int index =
                PopupItemIndexAt(
                    ToLogical(
                        hwnd,
                        GET_Y_LPARAM(lParam)));

            const int previous =
                g_popupMenu.pressedIndex;

            g_popupMenu.pressedIndex = index;

            InvalidatePopupItem(
                hwnd,
                previous);

            InvalidatePopupItem(
                hwnd,
                index);

            return 0;
        }

        case WM_LBUTTONUP:
        {
            const int index =
                PopupItemIndexAt(
                    ToLogical(
                        hwnd,
                        GET_Y_LPARAM(lParam)));

            const int pressedIndex =
                g_popupMenu.pressedIndex;

            g_popupMenu.pressedIndex = -1;

            if (index >= 0 &&
                index == pressedIndex)
            {
                g_popupMenu.result =
                    g_popupMenu.items[
                        static_cast<size_t>(index)].command;
            }

            g_popupMenu.open = false;

            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            g_popupMenu.open = false;

            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                g_popupMenu.open = false;

                return 0;
            }

            break;

        case WM_KILLFOCUS:
            g_popupMenu.open = false;

            return 0;

        default:
            break;
        }

        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam);
    }

    //
    // Shows the menu under the given button and returns the
    // chosen command, or zero when the menu was dismissed.
    //
    UINT ShowPopupMenu(
        HWND owner,
        const std::vector<PopupMenuItem>& items,
        const ButtonRect& anchorLogical)
    {
        g_popupMenu = PopupMenuState{};
        g_popupMenu.items = items;

        //
        // The panel fits its widest label.
        //
        HDC hdc = GetDC(owner);

        HFONT font = GetFontForUI(
            owner,
            POPUP_FONT_SIZE,
            FW_NORMAL);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        const int dpi = GetDeviceCaps(
            hdc,
            LOGPIXELSY);

        int widestLabel = 0;

        for (const PopupMenuItem& item :
            g_popupMenu.items)
        {
            if (item.label == nullptr)
            {
                continue;
            }

            SIZE size{};

            GetTextExtentPoint32W(
                hdc,
                item.label,
                lstrlenW(item.label),
                &size);

            const int logicalWidth = MulDiv(
                size.cx,
                BASE_DPI,
                dpi);

            if (logicalWidth > widestLabel)
            {
                widestLabel = logicalWidth;
            }
        }

        SelectObject(
            hdc,
            oldFont);

        ReleaseDC(
            owner,
            hdc);

        g_popupMenu.widthLogical =
            POPUP_LEFT_PADDING +
            POPUP_CHECK_COLUMN +
            widestLabel +
            POPUP_RIGHT_PADDING;

        int height = POPUP_PANEL_PADDING * 2;

        for (const PopupMenuItem& item :
            g_popupMenu.items)
        {
            height += item.label == nullptr
                ? POPUP_SEPARATOR_HEIGHT
                : POPUP_ITEM_HEIGHT;
        }

        g_popupMenu.heightLogical = height;

        const int widthDevice = ScaleValue(
            owner,
            g_popupMenu.widthLogical);

        const int heightDevice = ScaleValue(
            owner,
            g_popupMenu.heightLogical);

        //
        // Below the button, right aligned with it.
        //
        POINT position{};

        position.x = ScaleValue(
            owner,
            anchorLogical.right);

        position.y =
            ScaleValue(owner, anchorLogical.bottom) +
            ScaleValue(owner, 8);

        ClientToScreen(
            owner,
            &position);

        int x = position.x - widthDevice;
        int y = position.y;

        const HMONITOR monitor =
            MonitorFromWindow(
                owner,
                MONITOR_DEFAULTTONEAREST);

        MONITORINFO monitorInfo{};

        monitorInfo.cbSize = sizeof(monitorInfo);

        if (GetMonitorInfoW(
            monitor,
            &monitorInfo))
        {
            const RECT& workArea =
                monitorInfo.rcWork;

            if (x + widthDevice > workArea.right)
            {
                x = workArea.right - widthDevice;
            }

            if (x < workArea.left)
            {
                x = workArea.left;
            }

            if (y + heightDevice > workArea.bottom)
            {
                y = workArea.bottom - heightDevice;
            }

            if (y < workArea.top)
            {
                y = workArea.top;
            }
        }

        static bool classRegistered = false;

        if (!classRegistered)
        {
            WNDCLASSW menuClass{};

            menuClass.style = CS_DROPSHADOW;
            menuClass.lpfnWndProc = PopupMenuProc;
            menuClass.hInstance = GetModuleHandleW(nullptr);
            menuClass.hCursor = LoadCursorW(
                nullptr,
                IDC_ARROW);
            menuClass.lpszClassName = POPUP_MENU_CLASS;

            RegisterClassW(&menuClass);

            classRegistered = true;
        }

        HWND menuWindow = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            POPUP_MENU_CLASS,
            L"",
            WS_POPUP,
            x,
            y,
            widthDevice,
            heightDevice,
            owner,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        if (menuWindow == nullptr)
        {
            return 0;
        }

        int cornerPreference = 2;

        DwmSetWindowAttribute(
            menuWindow,
            33,
            &cornerPreference,
            sizeof(cornerPreference));

        g_popupMenu.open = true;

        ShowWindow(
            menuWindow,
            SW_SHOW);

        SetForegroundWindow(menuWindow);

        SetCapture(menuWindow);

        MSG message{};

        while (g_popupMenu.open &&
            GetMessageW(
                &message,
                nullptr,
                0,
                0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (!IsWindow(owner))
            {
                break;
            }
        }

        if (GetCapture() == menuWindow)
        {
            ReleaseCapture();
        }

        DestroyWindow(menuWindow);

        if (IsWindow(owner))
        {
            SetForegroundWindow(owner);
        }

        const UINT result =
            g_popupMenu.result;

        g_popupMenu.items.clear();

        return result;
    }

    void ToggleFileAssociation(
        HWND hwnd)
    {
        const bool registered =
            FileAssociation::IsRegistered();

        if (FileAssociation::SetRegistered(
            !registered))
        {
            return;
        }

        MessageBoxW(
            hwnd,
            GetStrings().associateError,
            WINDOW_TITLE,
            MB_OK | MB_ICONINFORMATION);
    }

    void ShowSettingsMenu(
        HWND hwnd)
    {
        std::vector<PopupMenuItem> items;

        items.push_back(
            PopupMenuItem{
                MENU_KEEP_LAUNCHER_OPEN,
                GetStrings().menuKeepOpen,
                g_settings.keepLauncherOpen });

        items.push_back(
            PopupMenuItem{
                MENU_ASSOCIATE_VRCW,
                GetStrings().menuAssociate,
                FileAssociation::IsRegistered() });

        items.push_back(
            PopupMenuItem{
                0,
                nullptr,
                false });

        items.push_back(
            PopupMenuItem{
                MENU_CHOOSE_VRCHAT_FOLDER,
                GetStrings().chooseVrchatFolder,
                false });

        if (!g_settings.recentVrcwFiles.empty())
        {
            items.push_back(
                PopupMenuItem{
                    MENU_CLEAR_RECENT_FILES,
                    GetStrings().menuClearRecent,
                    false });
        }

        items.push_back(
            PopupMenuItem{
                0,
                nullptr,
                false });

        items.push_back(
            PopupMenuItem{
                MENU_LANGUAGE,
                GetStrings().menuLanguage,
                g_chineseUi });

        items.push_back(
            PopupMenuItem{
                MENU_ABOUT,
                GetStrings().menuAbout,
                false });

        int widthLogical = 0;
        int heightLogical = 0;

        GetLogicalClientSize(
            hwnd,
            widthLogical,
            heightLogical);

        const UINT command =
            ShowPopupMenu(
                hwnd,
                items,
                GetSettingsMenuAnchorRect(
                    widthLogical));

        switch (command)
        {
        case MENU_KEEP_LAUNCHER_OPEN:
            g_settings.keepLauncherOpen =
                !g_settings.keepLauncherOpen;

            Settings::Save(g_settings);

            break;

        case MENU_ASSOCIATE_VRCW:
            ToggleFileAssociation(hwnd);

            break;

        case MENU_CHOOSE_VRCHAT_FOLDER:
            ChooseVRChatFolder(hwnd);

            break;

        case MENU_CLEAR_RECENT_FILES:
            Settings::ClearRecentFiles(g_settings);

            Settings::Save(g_settings);

            InvalidateMainWindow();

            break;

        case MENU_ABOUT:
            MessageBoxW(
                hwnd,
                GetStrings().aboutText,
                WINDOW_TITLE,
                MB_OK | MB_ICONINFORMATION);

            break;

        case MENU_LANGUAGE:
            g_chineseUi = !g_chineseUi;

            //
            // An explicit choice replaces the automatic one.
            //
            g_settings.chineseUi =
                g_chineseUi ? 1 : 0;

            Settings::Save(g_settings);

            //
            // Chinese text uses a different font family.
            //
            ClearFontCache();

            InvalidateMainWindow();

            break;

        default:
            break;
        }
    }

    //
    // The window grows when recent worlds are remembered,
    // but never past the usable area of the display.
    //
    int GetInitialWindowHeight()
    {
        UINT dpi = GetDpiForSystem();

        if (dpi == 0)
        {
            dpi = BASE_DPI;
        }

        const int logicalHeight =
            g_settings.recentVrcwFiles.empty()
                ? WINDOW_HEIGHT
                : WINDOW_HEIGHT_WITH_RECENT;

        int height = MulDiv(
            logicalHeight,
            static_cast<int>(dpi),
            BASE_DPI);

        RECT workArea{};

        if (SystemParametersInfoW(
            SPI_GETWORKAREA,
            0,
            &workArea,
            0))
        {
            const int margin = MulDiv(
                24,
                static_cast<int>(dpi),
                BASE_DPI);

            const int maximum =
                (workArea.bottom - workArea.top) -
                margin;

            if (maximum > 0 && height > maximum)
            {
                height = maximum;
            }
        }

        return height;
    }

    //
    // Grows the window so that remembered worlds become
    // visible without restarting the launcher.
    //
    void EnsureRecentSectionFits(
        HWND hwnd)
    {
        if (hwnd == nullptr ||
            g_settings.recentVrcwFiles.empty())
        {
            return;
        }

        RECT windowRect{};

        if (!GetWindowRect(
            hwnd,
            &windowRect))
        {
            return;
        }

        const int currentHeight =
            windowRect.bottom - windowRect.top;

        const int desiredHeight =
            GetInitialWindowHeight();

        if (desiredHeight <= currentHeight)
        {
            return;
        }

        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            windowRect.right - windowRect.left,
            desiredHeight,
            SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);
    }

    //
    // Every layout value is designed at 96 DPI and scaled
    // for the current window.
    //
    int ToLogical(
        HWND hwnd,
        int deviceValue)
    {
        UINT dpi = GetDpiForWindow(hwnd);

        if (dpi == 0)
        {
            dpi = BASE_DPI;
        }

        return MulDiv(
            deviceValue,
            BASE_DPI,
            static_cast<int>(dpi));
    }

    void GetLogicalClientSize(
        HWND hwnd,
        int& widthLogical,
        int& heightLogical)
    {
        RECT clientRect{};

        GetClientRect(
            hwnd,
            &clientRect);

        widthLogical =
            ToLogical(
                hwnd,
                clientRect.right);

        heightLogical =
            ToLogical(
                hwnd,
                clientRect.bottom);
    }

    bool IsInsideRect(
        const ButtonRect& rect,
        int x,
        int y)
    {
        return x >= rect.left &&
            x <= rect.right &&
            y >= rect.top &&
            y <= rect.bottom;
    }

    ButtonRect GetChooseVrcwButtonRect(
        int widthLogical)
    {
        ButtonRect rect{};

        rect.left = widthLogical / 2 - 76;
        rect.right = widthLogical / 2 + 76;
        rect.top = 291;
        rect.bottom = 335;

        return rect;
    }

    //
    // "Change..." sits on the right side of the card row.
    //
    ButtonRect GetChangeButtonRect(
        int widthLogical)
    {
        ButtonRect rect{};

        rect.right = widthLogical - 72;
        rect.left = rect.right - 104;
        rect.top = 231;
        rect.bottom = 263;

        return rect;
    }

    //
    // Returns the rect of a button for the current lay-out.
    //
    bool GetButtonRectById(
        HWND hwnd,
        int widthLogical,
        int heightLogical,
        int id,
        ButtonRect& rect)
    {
        switch (id)
        {
        case BUTTON_CHOOSE_VRCW:
            rect = GetChooseVrcwButtonRect(widthLogical);
            return true;

        case BUTTON_CHANGE:
            rect = GetChangeButtonRect(widthLogical);
            return true;

        case BUTTON_LAUNCH:
            rect = GetLaunchButtonRect(
                widthLogical,
                heightLogical);
            return true;

        case BUTTON_CHOOSE_VRCHAT_FOLDER:
            rect = GetChooseVrchatButtonRect(widthLogical);
            return true;

        case BUTTON_SETTINGS:
            rect = GetSettingsButtonRect(widthLogical);
            return true;

        default:
            break;
        }

        if (id >= BUTTON_RECENT_FIRST)
        {
            const int index =
                id - BUTTON_RECENT_FIRST;

            if (index < GetVisibleRecentCount(heightLogical))
            {
                rect = GetRecentRowRect(
                    widthLogical,
                    index);

                return true;
            }
        }

        (void)hwnd;

        return false;
    }

    //
    // Which button is under the given point. Buttons that
    // cannot be used right now report nothing.
    //
    int HitTestButtons(
        HWND hwnd,
        int widthLogical,
        int heightLogical,
        int logicalX,
        int logicalY)
    {
        if (g_selectedVrcwPath.empty())
        {
            if (IsInsideRect(
                GetChooseVrcwButtonRect(widthLogical),
                logicalX,
                logicalY))
            {
                return BUTTON_CHOOSE_VRCW;
            }
        }
        else if (IsInsideRect(
            GetChangeButtonRect(widthLogical),
            logicalX,
            logicalY))
        {
            return BUTTON_CHANGE;
        }

        if (IsLaunchEnabled() &&
            IsInsideRect(
                GetLaunchButtonRect(
                    widthLogical,
                    heightLogical),
                logicalX,
                logicalY))
        {
            return BUTTON_LAUNCH;
        }

        if (g_vrchatExePath.empty() &&
            IsInsideRect(
                GetChooseVrchatButtonRect(widthLogical),
                logicalX,
                logicalY))
        {
            return BUTTON_CHOOSE_VRCHAT_FOLDER;
        }

        if (IsInsideRect(
            GetSettingsButtonRect(widthLogical),
            logicalX,
            logicalY))
        {
            return BUTTON_SETTINGS;
        }

        const int visibleRecentCount =
            GetVisibleRecentCount(heightLogical);

        for (int index = 0;
            index < visibleRecentCount;
            ++index)
        {
            if (IsRecentFileMissing(
                g_settings.recentVrcwFiles[
                    static_cast<size_t>(index)]))
            {
                continue;
            }

            if (IsInsideRect(
                GetRecentRowRect(widthLogical, index),
                logicalX,
                logicalY))
            {
                return BUTTON_RECENT_FIRST + index;
            }
        }

        (void)hwnd;

        return BUTTON_NONE;
    }

    int HitTestCurrentPosition(
        HWND hwnd)
    {
        POINT cursor{};

        if (GetCursorPos(&cursor) == FALSE ||
            ScreenToClient(hwnd, &cursor) == FALSE)
        {
            return BUTTON_NONE;
        }

        int widthLogical = 0;
        int heightLogical = 0;

        GetLogicalClientSize(
            hwnd,
            widthLogical,
            heightLogical);

        return HitTestButtons(
            hwnd,
            widthLogical,
            heightLogical,
            ToLogical(hwnd, cursor.x),
            ToLogical(hwnd, cursor.y));
    }

    //
    // Only the button that changed is repainted.
    //
    void InvalidateButton(
        HWND hwnd,
        int id)
    {
        if (id == BUTTON_NONE)
        {
            return;
        }

        int widthLogical = 0;
        int heightLogical = 0;

        GetLogicalClientSize(
            hwnd,
            widthLogical,
            heightLogical);

        ButtonRect rect{};

        if (!GetButtonRectById(
            hwnd,
            widthLogical,
            heightLogical,
            id,
            rect))
        {
            return;
        }

        RECT deviceRect{};

        deviceRect.left = ScaleValue(hwnd, rect.left);
        deviceRect.top = ScaleValue(hwnd, rect.top);
        deviceRect.right = ScaleValue(hwnd, rect.right);
        deviceRect.bottom = ScaleValue(hwnd, rect.bottom);

        //
        // GDI clips the repaint of a rounded button.
        //
        InflateRect(
            &deviceRect,
            2,
            2);

        InvalidateRect(
            hwnd,
            &deviceRect,
            FALSE);
    }

    //
    // The pointer has to be refreshed after a modal dialog
    // or the settings menu closes, otherwise it keeps the
    // last shape it had.
    //
    void UpdateCursorForWindow(
        HWND hwnd)
    {
        const int hovered =
            HitTestCurrentPosition(hwnd);

        SetCursor(
            LoadCursorW(
                nullptr,
                hovered == BUTTON_NONE
                    ? IDC_ARROW
                    : IDC_HAND));
    }

    void DrawTextCenteredInRect(
        HWND hwnd,
        HDC hdc,
        const wchar_t* text,
        const ButtonRect& rect,
        int size,
        COLORREF color,
        int weight)
    {
        HFONT font = GetFontForUI(
            hwnd,
            size,
            weight);

        HFONT oldFont =
            (HFONT)SelectObject(
                hdc,
                font);

        SetBkMode(
            hdc,
            TRANSPARENT);

        SetTextColor(
            hdc,
            color);

        RECT target{};

        target.left = ScaleValue(hwnd, rect.left);
        target.top = ScaleValue(hwnd, rect.top);
        target.right = ScaleValue(hwnd, rect.right);
        target.bottom = ScaleValue(hwnd, rect.bottom);

        //
        // DrawText centres on both axes, which a plain
        // TextOut call cannot do.
        //
        DrawTextW(
            hdc,
            text,
            -1,
            &target,
            DT_SINGLELINE |
            DT_CENTER |
            DT_VCENTER |
            DT_NOPREFIX);

        SelectObject(
            hdc,
            oldFont);
    }

    //
    // Vertical gradient clipped to a rounded rectangle.
    //
    void FillRoundedGradient(
        HWND hwnd,
        HDC hdc,
        const ButtonRect& rect,
        int radius,
        COLORREF topColor,
        COLORREF bottomColor)
    {
        const int left = ScaleValue(hwnd, rect.left);
        const int top = ScaleValue(hwnd, rect.top);
        const int right = ScaleValue(hwnd, rect.right);
        const int bottom = ScaleValue(hwnd, rect.bottom);
        const int cornerRadius =
            ScaleValue(hwnd, radius);

        HRGN region =
            CreateRoundRectRgn(
                left,
                top,
                right + 1,
                bottom + 1,
                cornerRadius,
                cornerRadius);

        if (region == nullptr)
        {
            FillRoundedRect(
                hwnd,
                hdc,
                rect.left,
                rect.top,
                rect.right,
                rect.bottom,
                radius,
                topColor);

            return;
        }

        const int savedState = SaveDC(hdc);

        SelectClipRgn(
            hdc,
            region);

        TRIVERTEX vertices[2] = {};

        vertices[0].x = left;
        vertices[0].y = top;
        vertices[0].Red =
            static_cast<COLOR16>(
                GetRValue(topColor) << 8);
        vertices[0].Green =
            static_cast<COLOR16>(
                GetGValue(topColor) << 8);
        vertices[0].Blue =
            static_cast<COLOR16>(
                GetBValue(topColor) << 8);
        vertices[0].Alpha = 0xFFFF;

        vertices[1].x = right;
        vertices[1].y = bottom;
        vertices[1].Red =
            static_cast<COLOR16>(
                GetRValue(bottomColor) << 8);
        vertices[1].Green =
            static_cast<COLOR16>(
                GetGValue(bottomColor) << 8);
        vertices[1].Blue =
            static_cast<COLOR16>(
                GetBValue(bottomColor) << 8);
        vertices[1].Alpha = 0xFFFF;

        GRADIENT_RECT gradient{};

        gradient.UpperLeft = 0;
        gradient.LowerRight = 1;

        GradientFill(
            hdc,
            vertices,
            2,
            &gradient,
            1,
            GRADIENT_FILL_RECT_V);

        RestoreDC(
            hdc,
            savedState);

        DeleteObject(region);
    }

    enum ButtonVisual
    {
        VISUAL_PRIMARY,
        VISUAL_SECONDARY
    };

    void DrawButton(
        HWND hwnd,
        HDC hdc,
        const ButtonRect& rect,
        const wchar_t* label,
        ButtonVisual visual,
        int id,
        bool enabled,
        int radius,
        int fontSize)
    {
        const bool hovered =
            enabled &&
            g_hoveredButton == id;

        const bool pressed =
            enabled &&
            g_pressedButton == id;

        COLORREF topColor = DISABLED_BUTTON_COLOR;
        COLORREF bottomColor = DISABLED_BUTTON_COLOR;
        COLORREF textColor = DISABLED_TEXT_COLOR;

        if (enabled)
        {
            if (visual == VISUAL_PRIMARY)
            {
                textColor = ON_ACCENT_COLOR;

                if (pressed)
                {
                    topColor = ACCENT_PRESSED_COLOR;
                    bottomColor = ACCENT_PRESSED_COLOR;
                }
                else if (hovered)
                {
                    topColor = ACCENT_HOVER_TOP_COLOR;
                    bottomColor = ACCENT_HOVER_BOTTOM_COLOR;
                }
                else
                {
                    topColor = ACCENT_COLOR;
                    bottomColor = ACCENT_COLOR;
                }
            }
            else
            {
                textColor = TEXT_COLOR;

                if (pressed)
                {
                    topColor = SECONDARY_BUTTON_PRESSED_COLOR;
                    bottomColor = SECONDARY_BUTTON_PRESSED_COLOR;
                }
                else if (hovered)
                {
                    topColor = SECONDARY_BUTTON_HOVER_TOP_COLOR;
                    bottomColor = SECONDARY_BUTTON_HOVER_BOTTOM_COLOR;
                }
                else
                {
                    topColor = SECONDARY_BUTTON_COLOR;
                    bottomColor = SECONDARY_BUTTON_COLOR;
                }
            }
        }

        if (topColor == bottomColor)
        {
            FillRoundedRect(
                hwnd,
                hdc,
                rect.left,
                rect.top,
                rect.right,
                rect.bottom,
                radius,
                topColor);
        }
        else
        {
            FillRoundedGradient(
                hwnd,
                hdc,
                rect,
                radius,
                topColor,
                bottomColor);
        }

        DrawTextCenteredInRect(
            hwnd,
            hdc,
            label,
            rect,
            fontSize,
            textColor,
            FW_SEMIBOLD);
    }

    void ActivateButton(
        HWND hwnd,
        int id)
    {
        switch (id)
        {
        case BUTTON_CHOOSE_VRCW:
        case BUTTON_CHANGE:
            ChooseVRCWFile(hwnd);
            break;

        case BUTTON_LAUNCH:
            //
            // May close the window.
            //
            PerformLaunch(hwnd);
            return;

        case BUTTON_CHOOSE_VRCHAT_FOLDER:
            ChooseVRChatFolder(hwnd);
            break;

        case BUTTON_SETTINGS:
            ShowSettingsMenu(hwnd);
            break;

        default:
            if (id >= BUTTON_RECENT_FIRST)
            {
                const size_t index =
                    static_cast<size_t>(
                        id - BUTTON_RECENT_FIRST);

                if (index <
                    g_settings.recentVrcwFiles.size())
                {
                    SetSelectedVrcwPath(
                        g_settings.recentVrcwFiles[index]);
                }
            }

            break;
        }

        //
        // A dialog or menu has just closed.
        //
        UpdateCursorForWindow(hwnd);

        InvalidateMainWindow();
    }

    LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_DPICHANGED:
        {
            RECT* suggestedRect =
                reinterpret_cast<RECT*>(
                    lParam);

            //
            // The cached fonts were built for the
            // previous DPI.
            //
            ClearFontCache();
            ClearGdiCache();

            SetWindowPos(
                hwnd,
                nullptr,
                suggestedRect->left,
                suggestedRect->top,
                suggestedRect->right -
                suggestedRect->left,
                suggestedRect->bottom -
                suggestedRect->top,
                SWP_NOZORDER |
                SWP_NOACTIVATE);

            InvalidateRect(
                hwnd,
                nullptr,
                TRUE);

            return 0;
        }

        case WM_DROPFILES:
        {
            HDROP drop =
                reinterpret_cast<HDROP>(wParam);

            UINT fileCount =
                DragQueryFileW(
                    drop,
                    0xFFFFFFFF,
                    nullptr,
                    0);

            //
            // Only accept one file at a time.
            //
            if (fileCount == 1)
            {
                UINT pathLength =
                    DragQueryFileW(
                        drop,
                        0,
                        nullptr,
                        0);

                std::wstring path(
                    pathLength + 1,
                    L'\0');

                DragQueryFileW(
                    drop,
                    0,
                    &path[0],
                    pathLength + 1);

                path.resize(pathLength);

                //
                // Only accept .vrcw files.
                //
                SetSelectedVrcwPath(path);
            }

            DragFinish(drop);

            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            int clientWidth = 0;
            int clientHeight = 0;

            GetLogicalClientSize(
                hwnd,
                clientWidth,
                clientHeight);

            const int id =
                HitTestButtons(
                    hwnd,
                    clientWidth,
                    clientHeight,
                    ToLogical(hwnd, GET_X_LPARAM(lParam)),
                    ToLogical(hwnd, GET_Y_LPARAM(lParam)));

            if (id != BUTTON_NONE)
            {
                g_pressedButton = id;

                SetCapture(hwnd);

                InvalidateButton(hwnd, id);
            }

            return 0;
        }

        case WM_LBUTTONUP:
        {
            int clientWidth = 0;
            int clientHeight = 0;

            GetLogicalClientSize(
                hwnd,
                clientWidth,
                clientHeight);

            const int id =
                HitTestButtons(
                    hwnd,
                    clientWidth,
                    clientHeight,
                    ToLogical(hwnd, GET_X_LPARAM(lParam)),
                    ToLogical(hwnd, GET_Y_LPARAM(lParam)));

            const int pressedButton =
                g_pressedButton;

            if (pressedButton != BUTTON_NONE)
            {
                g_pressedButton = BUTTON_NONE;

                if (GetCapture() == hwnd)
                {
                    ReleaseCapture();
                }

                InvalidateButton(
                    hwnd,
                    pressedButton);
            }

            //
            // A button only reacts when the press started
            // on the same button.
            //
            if (id != BUTTON_NONE &&
                id == pressedButton)
            {
                ActivateButton(hwnd, id);
            }

            UpdateCursorForWindow(hwnd);

            return 0;
        }

        case WM_MOUSEMOVE:
        {
            if (!g_trackingMouse)
            {
                TRACKMOUSEEVENT track{};

                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;

                if (TrackMouseEvent(&track))
                {
                    g_trackingMouse = true;
                }
            }

            int clientWidth = 0;
            int clientHeight = 0;

            GetLogicalClientSize(
                hwnd,
                clientWidth,
                clientHeight);

            const int hovered =
                HitTestButtons(
                    hwnd,
                    clientWidth,
                    clientHeight,
                    ToLogical(hwnd, GET_X_LPARAM(lParam)),
                    ToLogical(hwnd, GET_Y_LPARAM(lParam)));

            if (hovered != g_hoveredButton)
            {
                const int previous =
                    g_hoveredButton;

                g_hoveredButton = hovered;

                InvalidateButton(hwnd, previous);
                InvalidateButton(hwnd, hovered);
            }

            return 0;
        }

        case WM_MOUSELEAVE:
        {
            g_trackingMouse = false;

            if (g_hoveredButton != BUTTON_NONE)
            {
                const int previous =
                    g_hoveredButton;

                g_hoveredButton = BUTTON_NONE;

                InvalidateButton(
                    hwnd,
                    previous);
            }

            return 0;
        }

        case WM_CAPTURECHANGED:
        {
            if (g_pressedButton != BUTTON_NONE)
            {
                const int previous =
                    g_pressedButton;

                g_pressedButton = BUTTON_NONE;

                InvalidateButton(
                    hwnd,
                    previous);
            }

            return 0;
        }

        case WM_SETCURSOR:
        {
            //
            // Leave the border and the title bar to
            // Windows.
            //
            if (LOWORD(lParam) != HTCLIENT)
            {
                return DefWindowProcW(
                    hwnd,
                    message,
                    wParam,
                    lParam);
            }

            UpdateCursorForWindow(hwnd);

            return TRUE;
        }

        case WM_COPYDATA:
        {
            const COPYDATASTRUCT* data =
                reinterpret_cast<const COPYDATASTRUCT*>(
                    lParam);

            if (data == nullptr ||
                data->dwData != COPYDATA_VRCW_PATH ||
                data->lpData == nullptr ||
                data->cbData < sizeof(wchar_t))
            {
                return FALSE;
            }

            const std::wstring path(
                reinterpret_cast<const wchar_t*>(
                    data->lpData));

            //
            // Another launch arrived while this window
            // was open: take the file over and start.
            //
            if (SetSelectedVrcwPath(path) &&
                IsLaunchEnabled())
            {
                PerformLaunch(hwnd);
            }

            return TRUE;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc =
                BeginPaint(
                    hwnd,
                    &ps);

            RECT clientRect{};
            GetClientRect(
                hwnd,
                &clientRect);

            UINT dpi =
                GetDpiForWindow(hwnd);

            if (dpi == 0)
            {
                dpi = BASE_DPI;
            }

            int widthLogical =
                MulDiv(
                    clientRect.right,
                    BASE_DPI,
                    static_cast<int>(dpi));

            int heightLogical =
                MulDiv(
                    clientRect.bottom,
                    BASE_DPI,
                    static_cast<int>(dpi));

            //
            // Background
            //
            FillRect(
                hdc,
                &clientRect,
                GetSolidBrush(
                    BACKGROUND_COLOR));

            //
            // Header
            //
            DrawTextUI(
                hwnd,
                hdc,
                L"VRCW Launcher",
                48,
                38,
                25,
                TEXT_COLOR,
                FW_SEMIBOLD);

            //
            // Settings button
            //
            {
                const ButtonRect settingsRect =
                    GetSettingsButtonRect(
                        widthLogical);

                const int centerX =
                    (settingsRect.left +
                        settingsRect.right) / 2;

                const int centerY =
                    (settingsRect.top +
                        settingsRect.bottom) / 2;

                const bool hovered =
                    g_hoveredButton == BUTTON_SETTINGS;

                const bool pressed =
                    g_pressedButton == BUTTON_SETTINGS;

                if (hovered || pressed)
                {
                    FillRoundedRect(
                        hwnd,
                        hdc,
                        settingsRect.left,
                        settingsRect.top,
                        settingsRect.right,
                        settingsRect.bottom,
                        8,
                        pressed
                            ? ROW_PRESSED_COLOR
                            : ROW_HOVER_COLOR);
                }

                HBRUSH dotBrush =
                    GetSolidBrush(
                        hovered || pressed
                            ? TEXT_COLOR
                            : SECONDARY_TEXT_COLOR);

                HBRUSH oldDotBrush =
                    (HBRUSH)SelectObject(
                        hdc,
                        dotBrush);

                for (int index = -1;
                    index <= 1;
                    ++index)
                {
                    const int dotCenterX =
                        centerX + index * 11;

                    Ellipse(
                        hdc,
                        ScaleValue(
                            hwnd,
                            dotCenterX - 2),
                        ScaleValue(
                            hwnd,
                            centerY - 2),
                        ScaleValue(
                            hwnd,
                            dotCenterX + 2),
                        ScaleValue(
                            hwnd,
                            centerY + 2));
                }

                SelectObject(
                    hdc,
                    oldDotBrush);
            }

            //
            // VRCW section
            //
            DrawTextUI(
                hwnd,
                hdc,
                L"VRCW",
                48,
                105,
                14,
                SECONDARY_TEXT_COLOR,
                FW_SEMIBOLD);

            //
            // Main card
            //
            const int cardLeft = 48;
            const int cardTop = 136;
            const int cardRight =
                widthLogical - 48;
            const int cardBottom = 358;

            FillRoundedRect(
                hwnd,
                hdc,
                cardLeft,
                cardTop,
                cardRight,
                cardBottom,
                16,
                g_dragHover
                    ? DROP_HOVER_BACKGROUND_COLOR
                    : CARD_COLOR);

            DrawRoundedOutline(
                hwnd,
                hdc,
                cardLeft,
                cardTop,
                cardRight,
                cardBottom,
                16,
                g_dragHover
                    ? ACCENT_COLOR
                    : BORDER_COLOR,
                g_dragHover
                    ? 2
                    : 1);

            //
            // No file selected
            //
            if (g_selectedVrcwPath.empty())
            {
                int iconCenterX =
                    widthLogical / 2;

                int iconTop = 171;

                const int bigIconWidth = 46;
                const int bigIconHeight = 54;

                DrawFileIcon(
                    hwnd,
                    hdc,
                    iconCenterX - bigIconWidth / 2,
                    iconTop - 5,
                    bigIconWidth,
                    bigIconHeight,
                    16,
                    SECONDARY_TEXT_COLOR,
                    0,
                    nullptr);

                DrawCenteredText(
                    hwnd,
                    hdc,
                    GetStrings().dropFile,
                    widthLogical / 2,
                    232,
                    17,
                    TEXT_COLOR,
                    FW_NORMAL);

                DrawCenteredText(
                    hwnd,
                    hdc,
                    GetStrings().orText,
                    widthLogical / 2,
                    263,
                    13,
                    SECONDARY_TEXT_COLOR,
                    FW_NORMAL);

                DrawButton(
                    hwnd,
                    hdc,
                    GetChooseVrcwButtonRect(
                        widthLogical),
                    GetStrings().chooseVrcw,
                    VISUAL_SECONDARY,
                    BUTTON_CHOOSE_VRCW,
                    true,
                    10,
                    14);
            }
            //
            // File selected
            //
else
{
    std::wstring fileName =
        PathUtils::GetFileName(
            g_selectedVrcwPath);

    //
    // Everything in this row shares the same
    // vertical center.
    //
    const int rowCenter =
        (cardTop + cardBottom) / 2;

    //
    // File icon
    //
    const int iconWidth = 30;
    const int iconHeight = 38;

    const int iconX = 84;

    const int iconY =
        rowCenter - (iconHeight / 2);

    DrawFileIcon(
        hwnd,
        hdc,
        iconX,
        iconY,
        iconWidth,
        iconHeight,
        11,
        SECONDARY_TEXT_COLOR,
        0,
        nullptr);

    //
    // Text block
    //
    const int textX = iconX + 48;

    //
    // The two text lines are centered around
    // the same row center as the icon.
    //
    const int fileNameY =
        rowCenter - 22;

    const int fileTypeY =
        rowCenter + 5;

    DrawTextEllipsized(
        hwnd,
        hdc,
        fileName.c_str(),
        textX,
        fileNameY,
        17,
        TEXT_COLOR,
        FW_SEMIBOLD,
        cardRight - 124);

    DrawTextUI(
        hwnd,
        hdc,
        GetStrings().worldType,
        textX,
        fileTypeY,
        13,
        SECONDARY_TEXT_COLOR,
        FW_NORMAL);

    //
    // Change button
    //
    DrawButton(
        hwnd,
        hdc,
        GetChangeButtonRect(
            widthLogical),
        GetStrings().change,
        VISUAL_SECONDARY,
        BUTTON_CHANGE,
        true,
        10,
        13);
    }

            //
            // VRChat section
            //
            DrawTextUI(
                hwnd,
                hdc,
                L"VRChat",
                48,
                VRCHAT_HEADER_Y,
                14,
                SECONDARY_TEXT_COLOR,
                FW_SEMIBOLD);

            //
            // Status indicator
            //
            const bool vrchatDetected =
                !g_vrchatExePath.empty();

            HBRUSH statusBrush =
                GetSolidBrush(
                    vrchatDetected
                        ? SUCCESS_COLOR
                        : ATTENTION_COLOR);

            HBRUSH oldStatusBrush =
                (HBRUSH)SelectObject(
                    hdc,
                    statusBrush);

            Ellipse(
                hdc,
                ScaleValue(hwnd, 49),
                ScaleValue(
                    hwnd,
                    STATUS_ROW_CENTER_Y -
                    STATUS_DOT_RADIUS),
                ScaleValue(hwnd, 57),
                ScaleValue(
                    hwnd,
                    STATUS_ROW_CENTER_Y +
                    STATUS_DOT_RADIUS));

            SelectObject(
                hdc,
                oldStatusBrush);

            DrawTextInRect(
                hwnd,
                hdc,
                vrchatDetected
                    ? GetStrings().steamDetected
                    : GetStrings().steamNotFound,
                70,
                STATUS_ROW_CENTER_Y - 12,
                widthLogical - 48,
                STATUS_ROW_CENTER_Y + 12,
                14,
                TEXT_COLOR,
                FW_NORMAL,
                false,
                false);

            //
            // Only shown when VRChat could not be found
            // automatically.
            //
            if (!vrchatDetected)
            {
                DrawButton(
                    hwnd,
                    hdc,
                    GetChooseVrchatButtonRect(
                        widthLogical),
                    GetStrings().chooseVrchatFolder,
                    VISUAL_SECONDARY,
                    BUTTON_CHOOSE_VRCHAT_FOLDER,
                    true,
                    10,
                    13);
            }

            //
            // Recent section
            //
            const int visibleRecentCount =
                GetVisibleRecentCount(
                    heightLogical);

            if (visibleRecentCount > 0)
            {
                DrawTextUI(
                    hwnd,
                    hdc,
                    GetStrings().recent,
                    48,
                    RECENT_HEADER_Y,
                    14,
                    SECONDARY_TEXT_COLOR,
                    FW_SEMIBOLD);

                for (int index = 0;
                    index < visibleRecentCount;
                    ++index)
                {
                    const ButtonRect rowRect =
                        GetRecentRowRect(
                            widthLogical,
                            index);

                    const std::wstring& path =
                        g_settings.recentVrcwFiles[
                            static_cast<size_t>(index)];

                    std::wstring text =
                        PathUtils::GetFileName(path);

                    const bool missing =
                        IsRecentFileMissing(path);

                    if (missing)
                    {
                        text += GetStrings().missingSuffix;
                    }

                    const int rowId =
                        BUTTON_RECENT_FIRST + index;

                    const bool rowHovered =
                        !missing &&
                        g_hoveredButton == rowId;

                    const bool rowPressed =
                        !missing &&
                        g_pressedButton == rowId;

                    if (rowHovered || rowPressed)
                    {
                        FillRoundedRect(
                            hwnd,
                            hdc,
                            rowRect.left,
                            rowRect.top,
                            rowRect.right,
                            rowRect.bottom,
                            8,
                            rowPressed
                                ? ROW_PRESSED_COLOR
                                : ROW_HOVER_COLOR);
                    }

                    DrawTextInRect(
                        hwnd,
                        hdc,
                        text.c_str(),
                        rowRect.left + 10,
                        rowRect.top,
                        rowRect.right - 8,
                        rowRect.bottom,
                        13,
                        missing
                            ? DISABLED_TEXT_COLOR
                            : TEXT_COLOR,
                        FW_NORMAL,
                        false,
                        true);
                }
            }

            //
            // Separator
            //
            HPEN separatorPen =
                GetPen(
                    PS_SOLID,
                    ScaleValue(hwnd, 1),
                    BORDER_COLOR);

            HPEN oldSeparatorPen =
                (HPEN)SelectObject(
                    hdc,
                    separatorPen);

            MoveToEx(
                hdc,
                ScaleValue(hwnd, 48),
                ScaleValue(
                    hwnd,
                    heightLogical -
                    SEPARATOR_BOTTOM_OFFSET),
                NULL);

            LineTo(
                hdc,
                ScaleValue(
                    hwnd,
                    widthLogical - 48),
                ScaleValue(
                    hwnd,
                    heightLogical -
                    SEPARATOR_BOTTOM_OFFSET));

            SelectObject(
                hdc,
                oldSeparatorPen);

            //
            // Launch button
            //
            const ButtonRect launchRect =
                GetLaunchButtonRect(
                    widthLogical,
                    heightLogical);

            const bool launchEnabled =
                IsLaunchEnabled();

            DrawButton(
                hwnd,
                hdc,
                launchRect,
                GetStrings().launch,
                VISUAL_PRIMARY,
                BUTTON_LAUNCH,
                launchEnabled,
                11,
                14);

            EndPaint(
                hwnd,
                &ps);

            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
        {
            //
            // The layout is derived from the client size, so
            // resizing only needs a repaint. The buttons have
            // moved, so the pointer shape is re-checked too.
            //
            InvalidateRect(
                hwnd,
                nullptr,
                TRUE);

            UpdateCursorForWindow(hwnd);

            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* limits =
                reinterpret_cast<MINMAXINFO*>(
                    lParam);

            RECT minimum{};

            minimum.right = ScaleValue(
                hwnd,
                MIN_WINDOW_WIDTH);

            minimum.bottom = ScaleValue(
                hwnd,
                MIN_WINDOW_HEIGHT);

            AdjustWindowRectEx(
                &minimum,
                static_cast<DWORD>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWL_STYLE)),
                FALSE,
                static_cast<DWORD>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWL_EXSTYLE)));

            limits->ptMinTrackSize.x =
                minimum.right - minimum.left;

            limits->ptMinTrackSize.y =
                minimum.bottom - minimum.top;

            return 0;
        }

        case WM_SETTINGCHANGE:
        {
            if (lParam == 0)
            {
                return 0;
            }

            const wchar_t* area =
                reinterpret_cast<const wchar_t*>(
                    lParam);

            if (lstrcmpiW(
                area,
                L"ImmersiveColorSet") != 0)
            {
                return 0;
            }

            const bool darkMode =
                IsDarkModeEnabled();

            if (darkMode == g_darkMode)
            {
                return 0;
            }

            g_darkMode = darkMode;

            ApplyThemeColors();

            UpdatePreferredAppMode();

            ClearGdiCache();

            UpdateWindowTheme(hwnd);

            InvalidateRect(
                hwnd,
                nullptr,
                TRUE);

            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(
                hwnd,
                message,
                wParam,
                lParam);
        }
    }
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{
    //
    // Enable Per-Monitor DPI Awareness V2.
    // Must be called before creating any windows.
    //
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    //
    // Follow the Windows light or dark appearance and use
    // the accent colour the user picked.
    //
    g_darkMode = IsDarkModeEnabled();

    ApplyThemeColors();

    InitializeDarkModeSupport();

    UpdatePreferredAppMode();

    //
    // Resolve everything the launcher needs before any
    // window exists, so a launch started from a .vrcw
    // file can finish without showing anything.
    //
    const std::wstring requestedVrcwPath =
        GetVrcwPathFromCommandLine();

    //
    // Settings are loaded first: a VRChat folder that
    // the user picked before skips Steam detection.
    //
    g_settings = Settings::Load();

    //
    // Until the user picks a language in the menu, the
    // interface follows the Windows language.
    //
    g_chineseUi =
        g_settings.chineseUi < 0
            ? IsSystemSimplifiedChinese()
            : g_settings.chineseUi != 0;

    if (!g_settings.vrchatExecutablePath.empty() &&
        PathUtils::FileExists(
            g_settings.vrchatExecutablePath))
    {
        g_vrchatExePath =
            g_settings.vrchatExecutablePath;
    }
    else
    {
        g_vrchatExePath =
            SteamDetector::FindVRChatExecutable();

        //
        // Remember where VRChat was found so the next start
        // does not have to read the Steam configuration
        // again. The path is checked before it is used, so a
        // moved installation still falls back to detection.
        //
        if (!g_vrchatExePath.empty() &&
            g_vrchatExePath !=
                g_settings.vrchatExecutablePath)
        {
            g_settings.vrchatExecutablePath =
                g_vrchatExePath;

            Settings::Save(g_settings);
        }
    }

    //
    // Keep a single launcher instance. A second launch
    // hands its file over instead of opening another
    // launcher window.
    //
    HANDLE instanceMutex =
        CreateMutexW(
            nullptr,
            TRUE,
            L"VRCWLauncher.SingleInstance");

    const bool alreadyRunning =
        instanceMutex != nullptr &&
        GetLastError() == ERROR_ALREADY_EXISTS;

    if (alreadyRunning)
    {
        HWND existingWindow =
            FindWindowW(
                WINDOW_CLASS,
                nullptr);

        if (existingWindow != nullptr)
        {
            if (!requestedVrcwPath.empty())
            {
                SendVrcwPathToWindow(
                    existingWindow,
                    requestedVrcwPath);
            }
            else
            {
                SetForegroundWindow(
                    existingWindow);
            }
        }

        CloseHandle(instanceMutex);

        return 0;
    }

    //
    // A VRCW file was supplied by Explorer, either by
    // dropping it on the executable or by opening it
    // through the file association.
    //
    if (!requestedVrcwPath.empty())
    {
        SetSelectedVrcwPath(requestedVrcwPath);
    }

    //
    // Starting from a file argument normally happens
    // without a window. When the launcher is asked to
    // stay open the window appears instead, with the file
    // already selected.
    //
    if (!g_settings.keepLauncherOpen &&
        !g_selectedVrcwPath.empty() &&
        IsLaunchEnabled() &&
        TryLaunch())
    {
        CloseHandle(instanceMutex);

        return 0;
    }

    WNDCLASSEXW windowClass{};

    windowClass.cbSize =
        sizeof(windowClass);

    windowClass.lpfnWndProc =
        WindowProc;

    windowClass.hInstance =
        hInstance;

    windowClass.lpszClassName =
        WINDOW_CLASS;

    windowClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    //
    // The window icon is set explicitly. Leaving it to
    // Windows means the icon is guessed from the executable,
    // which can pick the wrong size or stay stuck on a cached
    // icon.
    //
    windowClass.hIcon =
        static_cast<HICON>(
            LoadImageW(
                hInstance,
                MAKEINTRESOURCEW(IDI_VRCWLAUNCHER),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXICON),
                GetSystemMetrics(SM_CYICON),
                LR_DEFAULTCOLOR));

    windowClass.hIconSm =
        static_cast<HICON>(
            LoadImageW(
                hInstance,
                MAKEINTRESOURCEW(IDI_SMALL),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON),
                GetSystemMetrics(SM_CYSMICON),
                LR_DEFAULTCOLOR));

    RegisterClassExW(
        &windowClass);

    //
    // Create the initial window using
    // logical design dimensions.
    //
    UINT dpi =
        GetDpiForSystem();

    if (dpi == 0)
    {
        dpi = BASE_DPI;
    }

    int initialWidth =
        MulDiv(
            WINDOW_WIDTH,
            dpi,
            BASE_DPI);

    //
    // The recent list needs extra room, and the window
    // must still fit on the display.
    //
    int initialHeight =
        GetInitialWindowHeight();

    HWND hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX |
        WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        initialWidth,
        initialHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (hwnd == nullptr)
    {
        CloseHandle(instanceMutex);

        return 0;
    }

    g_mainWindow = hwnd;

    UpdateWindowTheme(hwnd);

    DragAcceptFiles(
        hwnd,
        TRUE);

    //
    // Windows 11 rounded corners
    //
    int cornerPreference = 2;

    DwmSetWindowAttribute(
        hwnd,
        33,
        &cornerPreference,
        sizeof(cornerPreference));

    ShowWindow(
        hwnd,
        nCmdShow);

    UpdateWindow(hwnd);

    //
    // OLE is only needed for the file drop target and for
    // the file dialogs, so it is started after the window is
    // already on screen. That keeps the first paint fast and
    // keeps the OLE libraries out of the start-up path.
    //
    bool oleInitialized = false;

    if (SUCCEEDED(OleInitialize(nullptr)))
    {
        oleInitialized = true;

        //
        // The OLE drop target provides the drag feedback
        // inside the window. DragAcceptFiles stays as a
        // fallback for sources that do not use OLE.
        //
        VrcwDropTarget::Register(
            hwnd,
            OnVrcwDropped,
            OnDragHoverChanged);
    }

    MSG message{};

    while (GetMessageW(
        &message,
        nullptr,
        0,
        0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    g_mainWindow = nullptr;

    ClearFontCache();
    ClearGdiCache();

    CloseHandle(instanceMutex);

    if (oleInitialized)
    {
        VrcwDropTarget::Unregister(hwnd);

        OleUninitialize();
    }

    return static_cast<int>(
        message.wParam);
}
