#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <malloc.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Dwmapi.lib")

// see https://source.chromium.org/chromium/chromium/src/+/main:ui/views/win/hwnd_message_handler.h;l=72?q=WM_NCUAHDRAWCAPTION%20&ss=chromium%2Fchromium%2Fsrc:ui%2Fviews%2F
#define UNDOCUMENTED_WM_NCUAHDRAWCAPTION 0x00AE
#define UNDOCUMENTED_WM_NCUAHDRAWFRAME 0x00AF

#define GetWindowStyle(hwnd) ((DWORD)GetWindowLong(hwnd, GWL_STYLE))

// TODO: Look into https://source.chromium.org/chromium/chromium/src/+/main:ui/views/win/hwnd_message_handler.cc;l=2329;bpv=0;bpt=1

#define WINDOW_CLASS_NAME "edc6a340-968a-4ccf-8af3-de0a71c427fd"

#define WINDOW_BG_1 RGB(0xA0, 0xA0, 0xA0)
#define WINDOW_BG_2 RGB(0xF0, 0xF0, 0xF0)

// TODO: Variants of Minimize/Maximize/Close and windows drag interactions:
//
// Maximize
// 1) PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0)
// 2) ShowWindow(Handle, SW_MAXIMIZE)
//
// Restore
// 1) PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0)
// 2) ShowWindow(Handle, SW_NORMAL)
//
// Minimize
// 1) PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0)
// 2) ShowWindow(Handle, SW_MINIMIZE)
//
// Close
// 1) PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0)
// 2) PostMessage(hwnd, WM_CLOSE, 0, 0)
//
// Drag
// PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)
// NOTE: This starts it's own modal loop while the drag is active.

typedef struct Button
{
    RECT bounds;
    COLORREF color;
    int ht;
}
Button;

Button buttons[] = {
    {
        .bounds = {0, 0, 64, 32},
        .color = RGB(255, 0, 0),
        .ht = HTMINBUTTON,
    },
    {
        .bounds = {64, 0, 128, 32},
        .color = RGB(0, 255, 0),
        .ht = HTMAXBUTTON,
    },
    {
        .bounds = {128, 0, 192, 32},
        .color = RGB(0, 0, 255),
        .ht = HTCLOSE,
    },
};
int button_hovered = -1;

int is_tracking = 0;
int idle_message = 0;
int window_count = 0;
int s_dwm_composition_enabled = 0;

#define DWM_WINDOW_MARGINS_ENUM \
    X(DwmMarginsOff) \
    X(DwmMarginsNeg) \
    X(DwmMarginsOne) \
    X(DwmMarginsZero) \
    X(DwmMarginsTop) \
    X(DwmMarginsBottom) \
    X(DwmMarginsTerminal) \
    X(DwmMarginsTerminalBorderless)

#define NCCALCSIZE_MARGINS_ENUM \
    X(NcCalcSizeMarginsOff) \
    X(NcCalcSizeMarginsBypass) \
    X(NcCalcSizeMarginsCaptionOff) \
    X(NcCalcSizeMarginsBottomInset) \
    X(NcCalcSizeMarginsBottomOutset) \
    X(NcCalcSizeMarginsLeftOutset) \
    X(NcCalcSizeMarginsInset) \
    X(NcCalcSizeMarginsOutset) \
    X(NcCalcSizeMarginsTerminal)

typedef enum DwmMarginsEnum
{
    #define X(n) n,
    DWM_WINDOW_MARGINS_ENUM
    #undef X

    DwmMargins_MAX
} DwmMarginsEnum;

typedef enum NcCalcSizeMarginsEnum
{
    #define X(n) n,
    NCCALCSIZE_MARGINS_ENUM
    #undef X

    NcCalcSizeMargins_MAX
}
NcCalcSizeMarginsEnum;

char* dwm_margins_enum[] = {
    #define X(n) #n,
    DWM_WINDOW_MARGINS_ENUM
    #undef X
};

char* nccalcsize_margins_enum[] = {
    #define X(n) #n,
    NCCALCSIZE_MARGINS_ENUM
    #undef X
};

DwmMarginsEnum selected_dwm[] = {
    DwmMarginsTop,
    DwmMarginsBottom,
    DwmMarginsTerminal,
    DwmMarginsTerminalBorderless,
};

NcCalcSizeMarginsEnum selected_calcsize[] = {
    NcCalcSizeMarginsLeftOutset,
    NcCalcSizeMarginsInset,
    NcCalcSizeMarginsOutset,
    NcCalcSizeMarginsTerminal,
};

RECT grid_area = {0};
int grid_columns = ARRAYSIZE(selected_calcsize);
int grid_rows = ARRAYSIZE(selected_dwm);

typedef struct WindowSettings
{
    DwmMarginsEnum dwm_margins;
    NcCalcSizeMarginsEnum nccalcsize_margins;
}
WindowSettings;

#define INTERCEPTOR(name) BOOL(name)(WindowSettings * settings, HWND hwnd, UINT message, WPARAM wp, LPARAM lp, LRESULT * res)

INTERCEPTOR(dwm)
{
    if (message == WM_CREATE)
    {
        s_dwm_composition_enabled = FALSE;
        DwmIsCompositionEnabled(&s_dwm_composition_enabled);
        dwm(settings, hwnd, WM_DWMCOMPOSITIONCHANGED, wp, lp, res);
        return FALSE;
    }

    if (settings->dwm_margins == DwmMarginsOff)
        return FALSE;

    if (message == WM_DWMCOMPOSITIONCHANGED)
    {
        s_dwm_composition_enabled = FALSE;
        DwmIsCompositionEnabled(&s_dwm_composition_enabled);

        if (s_dwm_composition_enabled)
        {
            MARGINS margins = {0};
            switch (settings->dwm_margins)
            {
            case DwmMarginsNeg:
                margins = (MARGINS){-1};
                break;
            case DwmMarginsOne:
                margins = (MARGINS){1};
                break;
            case DwmMarginsZero:
                margins = (MARGINS){0};
                break;
            case DwmMarginsTop:
                margins = (MARGINS){0, 0, 1, 0};
                break;
            case DwmMarginsBottom:
                margins = (MARGINS){0, 0, 0, 1};
                break;
            case DwmMarginsTerminal:
                RECT frame = {0};
                UINT dpi = GetDpiForWindow(hwnd);
                AdjustWindowRectExForDpi(&frame, GetWindowStyle(hwnd), FALSE, 0, dpi);
                margins = (MARGINS) {
                    .cxLeftWidth = 0,
                    .cxRightWidth = 0,
                    .cyTopHeight = -frame.top,
                    .cyBottomHeight = 0,
                };
                break;
            case DwmMarginsTerminalBorderless:
                margins = (MARGINS) {0};
                margins.cyTopHeight = 1;
                break;
            }
            DwmExtendFrameIntoClientArea(hwnd, &margins);

            // Not very sure this has any positive effect.
            // https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmncrenderingpolicy
            // int attr = DWMNCRP_DISABLED;
            // int attr = DWMNCRP_ENABLED;
            // DwmSetWindowAttribute(
            //     hwnd,
            //     DWMWA_NCRENDERING_POLICY,
            //     &attr,
            //     sizeof(int));

            // Windows 11 (build 22000) stuff:
            // - DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
            // - DWM_WINDOW_CORNER_PREFERENCE
            // COLORREF attr = RGB(255, 0, 0);
            // DwmSetWindowAttribute(
            //     hwnd,
            //     DWMWA_BORDER_COLOR,
            //     &attr,
            //     sizeof(COLORREF)
            // );

            // This does have some effect on the extra borders, they are now drawn in white.
            // We can perhaps find out how to correctly cover the border with some reasonable color.
            // BOOL attr = TRUE;
            // DwmSetWindowAttribute(
            //     hwnd,
            //     DWMWA_ALLOW_NCPAINT,
            //     &attr,
            //     sizeof(BOOL)
            // );
        }

        // TODO: Pass to DefWindowProc?
        *res = 0;
        return TRUE;
    }

    return FALSE;
}

INTERCEPTOR(nccalcsize)
{
    if (settings->nccalcsize_margins == NcCalcSizeMarginsOff)
        return FALSE;

    if (message == WM_CREATE)
    {
        // This will initialize WM_NCCALCSIZE

        // We might not need the rect, so I'm commenting it out.
        // RECT rect;
        // GetWindowRect(hwnd, &rect);
        SetWindowPos(
            hwnd, NULL,
            0, // rect.left,
            0, // rect.top,
            0, // rect.right - rect.left,
            0, // rect.bottom - rect.top,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
        );
        return FALSE;
    }

    if (message == WM_NCCALCSIZE)
    {
        RECT nc_rect = *((RECT *)lp);
        int def_ret = DefWindowProc(hwnd, message, wp, lp);
        RECT *c_rect = (RECT *)lp;

        switch (settings->nccalcsize_margins)
        {
        case NcCalcSizeMarginsBypass:
            *c_rect = nc_rect;
            break;
        case NcCalcSizeMarginsCaptionOff:
            c_rect->top = nc_rect.top;
            break;
        case NcCalcSizeMarginsBottomInset:
            *c_rect = nc_rect;
            c_rect->bottom--;
            break;

        case NcCalcSizeMarginsBottomOutset:
            *c_rect = nc_rect;
            c_rect->bottom++;
            break;

        case NcCalcSizeMarginsLeftOutset:
            *c_rect = nc_rect;
            c_rect->left--;
            break;

        case NcCalcSizeMarginsInset:
            *c_rect = nc_rect;
            c_rect->top++;
            c_rect->left++;
            c_rect->bottom--;
            c_rect->right--;
            break;

        case NcCalcSizeMarginsOutset:
            *c_rect = nc_rect;
            c_rect->top--;
            c_rect->left--;
            c_rect->bottom++;
            c_rect->right++;
            break;

        case NcCalcSizeMarginsTerminal:
            if (def_ret != 0)
            {
                *res = def_ret;
                return TRUE;
            }
            // Term: Re-apply the original top from before the size of the default frame was applied.
            c_rect->top = nc_rect.top;
            break;
        }

        // COMMENTED: We don't know what ControlzEx is actually solving, so we'll have to see.
        // if (wp == 0)
        // {
        //     // ControlzEx: Using the combination of WVR.VALIDRECTS and WVR.REDRAW gives the smoothest
        //     // resize behavior we can achieve here.
        //     // This might be true for WM_PAINT-based drawing, but we'll see.
        //     // *res = WVR_VALIDRECTS | WVR_REDRAW;
        // }

        *res = 0;
        return TRUE;
    }

    return FALSE;
}

static LRESULT
window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wp,
    LPARAM lp)
{
    WindowSettings *settings = NULL;
    if (message == WM_CREATE)
    {
        CREATESTRUCTA *create_struct = (CREATESTRUCTA *)lp;
        settings = (WindowSettings *)create_struct->lpCreateParams;
        SetPropA(hwnd, WINDOW_CLASS_NAME "/settings", settings);
    }
    else
    {
        settings = GetPropA(hwnd, WINDOW_CLASS_NAME "/settings");
    }

    if (settings)
    {
        LRESULT res = 0;
        if (dwm(settings, hwnd, message, wp, lp, &res))
            return res;
        if (nccalcsize(settings, hwnd, message, wp, lp, &res))
            return res;
    }

    switch (message)
    {
    case WM_SIZE:
    {
        // int w = LOWORD(lp);
        // int h = HIWORD(lp);
        // for (int i = 0; i < 3; ++i)
        //     buttons[i].bounds = (RECT){w - (i + 1) * 64, 0, w - i * 64, 32};
    }
    break;

    case WM_ERASEBKGND:
        return 1;

    // case WM_NCPAINT:
    // {
    //     PAINTSTRUCT ps;
    //     HDC hdc = BeginPaint(hwnd, &ps);
    //     HBRUSH brush_bg = CreateSolidBrush(WINDOW_BG);
    //     FillRect(hdc, &ps.rcPaint, brush_bg);
    //     DeleteObject(brush_bg);
    //     EndPaint(hwnd, &ps);

    //     return 0;
    // }
    // break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r = ps.rcPaint;
        int hw = (r.right - r.left) / 2;
        int hh = (r.bottom - r.top) / 2;
        HBRUSH brush_bg_1 = CreateSolidBrush(WINDOW_BG_1);
        HBRUSH brush_bg_2 = CreateSolidBrush(WINDOW_BG_2);
        FillRect(hdc, &(RECT){ r.left, r.top, r.left + hw, r.top + hh }, brush_bg_1);
        FillRect(hdc, &(RECT){ r.left + hw, r.top, r.right, r.top + hh }, brush_bg_2);
        FillRect(hdc, &(RECT){ r.left, r.top + hh, r.left + hw, r.bottom }, brush_bg_2);
        FillRect(hdc, &(RECT){ r.left + hw, r.top + hh, r.right, r.bottom }, brush_bg_1);
        DeleteObject(brush_bg_2);
        DeleteObject(brush_bg_1);

        HBRUSH brush_hover = CreateSolidBrush(RGB(0, 255, 255));
        for (int i = 0; i < 3; ++i)
        {
            HBRUSH brush_normal = CreateSolidBrush(buttons[i].color);
            FillRect(hdc, &buttons[i].bounds, button_hovered == i
                ? brush_hover
                : brush_normal);
            DeleteObject(brush_normal);
        }
        DeleteObject(brush_hover);

        if (settings)
        {
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);

            int i = 0;
            TextOutA(hdc, 10, 50 + (i++) * 20,
                dwm_margins_enum[settings->dwm_margins],
                strlen(dwm_margins_enum[settings->dwm_margins]));
            TextOutA(hdc, 10, 50 + (i++) * 20,
                nccalcsize_margins_enum[settings->nccalcsize_margins],
                strlen(nccalcsize_margins_enum[settings->nccalcsize_margins]));
        }

        EndPaint(hwnd, &ps);

        return 0;
    }
    break;

    case WM_NCHITTEST:
    {
        POINT pt = {LOWORD(lp), HIWORD(lp)};
        ScreenToClient(hwnd, &pt);

        RECT rect;
        GetClientRect(hwnd, &rect);

        if (PtInRect(&rect, pt))
        {
            for (int i = 0; i < 3; ++i)
            {
                if (PtInRect(&buttons[i].bounds, pt))
                    return buttons[i].ht;
            }

            int gutter = 8;
            if (pt.y < gutter && pt.x < gutter)
                return HTTOPLEFT;
            if (pt.y < gutter && pt.x > rect.right - gutter)
                return HTTOPRIGHT;
            if (pt.y > rect.bottom - gutter && pt.x < gutter)
                return HTBOTTOMLEFT;
            if (pt.y > rect.bottom - gutter && pt.x > rect.right - gutter)
                return HTBOTTOMRIGHT;
            if (pt.y < gutter)
                return HTTOP;
            if (pt.y > rect.bottom - gutter)
                return HTBOTTOM;
            if (pt.x < gutter)
                return HTLEFT;
            if (pt.x > rect.right - gutter)
                return HTRIGHT;
            return HTCLIENT;
        }
        else
            return HTNOWHERE;
    }

    case WM_NCMOUSEMOVE:
    {
        int button_hovered_prev = button_hovered;
        button_hovered = -1;
        for (int i = 0; i < 3; ++i)
        {
            if (wp == buttons[i].ht)
            {
                button_hovered = i;
                break;
            }
        }
        if (button_hovered_prev != button_hovered)
            InvalidateRect(hwnd, NULL, FALSE);

        if (!is_tracking)
        {
            TRACKMOUSEEVENT tme = {
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE | TME_NONCLIENT,
                .hwndTrack = hwnd,
                .dwHoverTime = HOVER_DEFAULT,
            };
            is_tracking = TrackMouseEvent(&tme);
        }
        return 0;
    }
    break;

    case WM_NCMOUSELEAVE:
    {
        is_tracking = 0;
        button_hovered = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    // WM_NCLBUTTONDOWN draws original Windows 95/7 buttons when the button is clicked.
    // Solution here is to block it here.
    // However, this might not be ideal solution for everything.
    // Still would be better if we had this message handled by Windows.
    // Requires investigation and I consider this to be a "hack".
    case WM_NCLBUTTONDOWN:
        switch (wp)
        {
            case HTMAXBUTTON:
            case HTMINBUTTON:
            case HTCLOSE:
                return 0;
        }
        break;

    // Cop-out version of handling the buttons.
    // Normally we'd do CaptureMouse/ReleaseMouse to correctly handle this.
    // One might implement drag on these buttons as drag of the window.
    case WM_NCLBUTTONUP:
        switch (wp)
        {
            case HTMAXBUTTON:
                PostMessageA(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
                return 0;
            case HTMINBUTTON:
                PostMessageA(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                return 0;
            case HTCLOSE:
                PostMessageA(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                return 0;
        }
        break;

    // TODO: The documentation says that this receives hit-test from DefWindowProc in wp, and not the hit-test from calling our WindowProc.
    // But maybe we can force it?
    // case WM_NCLBUTTONDOWN:
    // {
    //     LRESULT r = DefWindowProcA(hwnd, WM_NCLBUTTONDOWN, wp, lp);
    //     return r;
    // }

    // COMMENTED: This might be needed for pre-Windows-10 or dwm-composition-disabled scenarios. Didn't find any kind of effect
    // in default Windows 10 with dwm composition enabled.
    //
    // case WM_NCPAINT:
    //     // If this is blocked when composition is enabled, it won't draw the shadow.
    //     // Be sure that s_dwm_composition_enabled is initialized soon enough.
    //     // If you don't pass the WS_VISIBLE flag to the window, then WM_NCPAINT will be called whenever you later call ShowWindow.
    //     if (!s_dwm_composition_enabled)
    //         return 1;
    //     break;

    // These undocumented messages are sent to draw themed window borders.
    // Block them to prevent drawing borders over the client area.
    // COMMENTED: Didn't have much luck getting these called on Windows 10. Commenting it out to see whether it'll help in some edge case later.    
    // case UNDOCUMENTED_WM_NCUAHDRAWCAPTION:
    //     OutputDebugStringA("UNDOCUMENTED_WM_NCUAHDRAWCAPTION\n");
    //     return 0;
    // case UNDOCUMENTED_WM_NCUAHDRAWFRAME:
    //     OutputDebugStringA("UNDOCUMENTED_WM_NCUAHDRAWFRAME\n");
    //     return 0;

    // Note: for some pre-Windows-10 versions or maybe dwm-composition-disabled scenarios, you might want to also block WM_NCACTIVATE
    // See https://github.com/ControlzEx/ControlzEx/blob/8ea2b94753b39b07310edc847fb825929bf25b81/src/ControlzEx/Behaviors/WindowChrome/WindowChromeBehavior.MessageHandling.cs#L237


    case WM_SETCURSOR:
    {
        // Show an arrow instead of the busy cursor
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
    break;

    case WM_DESTROY:
    {
        --window_count;
        PostMessage(NULL, idle_message, 0, 0);
        return 0;
    }
    break;

    }

    return DefWindowProcA(hwnd, message, wp, lp);
}

void
create_window(WindowSettings settings)
{
    // Weather outside is frightful
    // But the fire is so delightful
    // And since we've no place to go
    // Let it leak, let it leak, let it leak.
    WindowSettings* settings_allocated =
        malloc(sizeof(WindowSettings));
    *settings_allocated = settings;

    int grid_col = window_count % grid_columns;
    int grid_row = window_count / grid_columns;

    int grid_width = grid_area.right - grid_area.left;
    int grid_height = grid_area.bottom - grid_area.top;

    int cell_width = grid_width / grid_columns;
    int cell_height = grid_height / grid_rows;

    int inset = 10;
    int x = inset + grid_area.left + grid_col * cell_width;
    int y = inset + grid_area.top  + grid_row * cell_height;
    int w = cell_width - 2 * inset;
    int h = cell_height - 2 * inset;

    window_count++;

    CreateWindowExA(
        WS_EX_APPWINDOW,
        WINDOW_CLASS_NAME,
        "Win32 Borderless Test Bed",
        WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, w, h,
        0,
        0,
        0,
        settings_allocated);
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    char *pCmdLine,
    int nCmdShow)
{
    idle_message = RegisterWindowMessageA(WINDOW_CLASS_NAME "/idle_message");

    WNDCLASSEXA window_class = {
        .cbSize = sizeof(WNDCLASSEXA),
        .lpszClassName = WINDOW_CLASS_NAME,
        .lpfnWndProc = &window_proc,
        .style = CS_HREDRAW | CS_VREDRAW,
    };

    RegisterClassExA(&window_class);

    grid_area = (RECT){ 0, 0, 1920, 1000 };
    int wc = ARRAYSIZE(selected_dwm) * ARRAYSIZE(selected_calcsize);
    for (int i0 = 0; i0 < ARRAYSIZE(selected_dwm); ++i0)
    for (int i1 = 0; i1 < ARRAYSIZE(selected_calcsize); ++i1)
    {
        create_window((WindowSettings){
            .dwm_margins = selected_dwm[i0],
            .nccalcsize_margins = selected_calcsize[i1]
        });
    }

    MSG message = {0};
    while (window_count > 0)
    {
        BOOL result = GetMessageA(&message, 0, 0, 0);
        if (result > 0)
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        else
        {
            break;
        }
    }
    return 0;
}
