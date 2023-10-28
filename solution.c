#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <malloc.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Dwmapi.lib")

#define WINDOW_CLASS_NAME "112b8bb9-6939-43ac-9ef3-e1fa2a9b85c3"

#define UNDOCUMENTED_WM_NCUAHDRAWCAPTION 0x00AE
#define UNDOCUMENTED_WM_NCUAHDRAWFRAME 0x00AF

static BOOL s_dwm_composition_enabled = FALSE;
static BOOL s_is_tracking = FALSE;
static int s_button_hovered = -1;

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

static void
update_dwm(HWND hwnd)
{
    s_dwm_composition_enabled = FALSE;
    DwmIsCompositionEnabled(&s_dwm_composition_enabled);
    if (s_dwm_composition_enabled) {
        MARGINS margins = {
            .cxLeftWidth = 0,
            .cxRightWidth = 0,
            .cyTopHeight = 0,
            .cyBottomHeight = 1,
        };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
    OutputDebugStringA(s_dwm_composition_enabled ? "s_dwm_composition_enabled: TRUE\n" : "s_dwm_composition_enabled: FALSE\n");
}

static LRESULT
window_proc(
    HWND hwnd,
    UINT m,
    WPARAM wp,
    LPARAM lp)
{
    switch (m)
    {
    case WM_NCCREATE:
    {
        OutputDebugStringA("WM_NCCREATE\n");
    }
    break;

    case WM_CREATE:
    {
        OutputDebugStringA("WM_CREATE\n");
        // Do this as early as possible since we might drive some behavior by the s_dwm_composition_enabled it initializes.
        update_dwm(hwnd);

        // This will initialize WM_NCCALCSIZE
        // TODO: We might not need the window rect.
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
        return 0;
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH brush_bg = CreateSolidBrush(RGB(0xA0, 0xA0, 0xA0));
        FillRect(hdc, &ps.rcPaint, brush_bg);
        DeleteObject(brush_bg);

        HBRUSH brush_hover = CreateSolidBrush(RGB(0, 255, 255));
        for (int i = 0; i < 3; ++i)
        {
            HBRUSH brush_normal = CreateSolidBrush(buttons[i].color);
            FillRect(hdc, &buttons[i].bounds, s_button_hovered == i
                ? brush_hover
                : brush_normal);
            DeleteObject(brush_normal);
        }
        DeleteObject(brush_hover);
        EndPaint(hwnd, &ps);

        return 0;
    }
    break;

    case WM_NCCALCSIZE:
    {
        RECT nc_rect = *((RECT *)lp);
        DefWindowProc(hwnd, m, wp, lp);
        RECT *c_rect = (RECT *)lp;
        *c_rect = nc_rect;
        c_rect->bottom++;

        if (wp == 0)
        {
            // ControlzEx: Using the combination of WVR.VALIDRECTS and WVR.REDRAW gives the smoothest
            // resize behavior we can achieve here.
            //
            // This might be true for WM_PAINT-based drawing, but we'll see.
            return WVR_VALIDRECTS | WVR_HREDRAW | WVR_VREDRAW;
        }

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
                if (PtInRect(&buttons[i].bounds, pt))
                    return buttons[i].ht;

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
            return HTCLIENT;
    }

    case WM_DWMCOMPOSITIONCHANGED:
        update_dwm(hwnd);
        return 0;

    case WM_NCMOUSEMOVE:
    {
        int button_hovered_prev = s_button_hovered;
        s_button_hovered = -1;
        for (int i = 0; i < 3; ++i)
        {
            if (wp == buttons[i].ht)
            {
                s_button_hovered = i;
                break;
            }
        }
        if (button_hovered_prev != s_button_hovered)
            InvalidateRect(hwnd, NULL, FALSE);

        if (!s_is_tracking)
        {
            TRACKMOUSEEVENT tme = {
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE | TME_NONCLIENT,
                .hwndTrack = hwnd,
                .dwHoverTime = HOVER_DEFAULT,
            };
            s_is_tracking = TrackMouseEvent(&tme);
        }
        return 0;
    }
    break;

    case WM_NCMOUSELEAVE:
        s_is_tracking = 0;
        s_button_hovered = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    // The WM_NCLBUTTONDOWN when non-blocked paints the Windows 95 buttons on the original
    // position when the `WM_HITTEST` returns HTMAXBUTTON, HTMINBUTTON or HTCLOSE.
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

    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    }

    return DefWindowProcA(hwnd, m, wp, lp);
}


int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    char *pCmdLine,
    int nCmdShow)
{
    WNDCLASSEXA window_class = {
        .cbSize = sizeof(WNDCLASSEXA),
        .lpszClassName = WINDOW_CLASS_NAME,
        .lpfnWndProc = &window_proc,
        .style = CS_HREDRAW | CS_VREDRAW,
    };

    RegisterClassExA(&window_class);

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        WINDOW_CLASS_NAME,
        "Win32 Borderless",
        WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        50, 50, 900, 600,
        0, 0, 0, 0);

    OutputDebugStringA("ShowWindow\n");
    ShowWindow(hwnd, SW_SHOW);

    MSG m = {0};
    for (;;)
    {
        BOOL result = GetMessageA(&m, 0, 0, 0);
        if (result > 0)
        {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
        else
        {
            break;
        }
    }

    return 0;
}