#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <malloc.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Dwmapi.lib")

#define WINDOW_CLASS_NAME "edc6a340-968a-4ccf-8af3-de0a71c427fd"

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

#define DWM_WINDOW_MARGINS_ENUM \
    X(DwmMarginsOff) \
    X(DwmMarginsNeg) \
    X(DwmMarginsZero) \
    X(DwmMarginsTop) \
    X(DwmMarginsBottom)

#define NCCALCSIZE_MARGINS_ENUM \
    X(NcCalcSizeMarginsOff) \
    X(NcCalcSizeMarginsBypass) \
    X(NcCalcSizeMarginsCaptionOff) \
    X(NcCalcSizeMarginsBottomInset) \
    X(NcCalcSizeMarginsBottomOutset) \
    X(NcCalcSizeMarginsInset) \
    X(NcCalcSizeMarginsOutset)

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

RECT grid_area = {0};
int grid_columns = NcCalcSizeMargins_MAX;
int grid_rows = 5;

typedef struct WindowSettings
{
    DwmMarginsEnum dwm_margins;
    NcCalcSizeMarginsEnum nccalcsize_margins;
}
WindowSettings;

#define INTERCEPTOR(name) BOOL(name)(WindowSettings * settings, HWND handle, UINT message, WPARAM wp, LPARAM lp, LRESULT * res)

INTERCEPTOR(dwm)
{
    if (settings->dwm_margins == DwmMarginsOff)
        return FALSE;

    if (message == WM_CREATE)
    {
        dwm(settings, handle, WM_DWMCOMPOSITIONCHANGED, wp, lp, res);
        return FALSE;
    }

    if (message == WM_DWMCOMPOSITIONCHANGED)
    {
        BOOL enabled = FALSE;
        DwmIsCompositionEnabled(&enabled);
        if (enabled)
        {
            MARGINS margins = {0};
            switch (settings->dwm_margins)
            {
            case DwmMarginsNeg:
                margins = (MARGINS){-1};
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
            }
            DwmExtendFrameIntoClientArea(handle, &margins);
        }

        *res = 0;
        return TRUE;
    }

    return FALSE;
}

INTERCEPTOR(nccalcsize)
{
    if (settings->nccalcsize_margins == NcCalcSizeMarginsOff)
        return FALSE;

    if (message == WM_NCCALCSIZE)
    {
        RECT nc_rect = *((RECT *)lp);
        DefWindowProc(handle, message, wp, lp);
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
        }

        if (wp == 0)
        {
            // ControlzEx: Using the combination of WVR.VALIDRECTS and WVR.REDRAW gives the smoothest
            // resize behavior we can achieve here.
            //
            // This might be true for WM_PAINT-based drawing, but we'll see.
            return WVR_VALIDRECTS | WVR_HREDRAW | WVR_VREDRAW;
        }

        *res = 0;
        return TRUE;
    }

    return FALSE;
}

static LRESULT
window_proc(
    HWND handle,
    UINT message,
    WPARAM wp,
    LPARAM lp)
{
    WindowSettings *settings = NULL;
    if (message == WM_CREATE)
    {
        CREATESTRUCTA *create_struct = (CREATESTRUCTA *)lp;
        settings = (WindowSettings *)create_struct->lpCreateParams;
        SetPropA(handle, WINDOW_CLASS_NAME "/settings", settings);

        // This will initialize WM_NCCALCSIZE
        RECT rect;
        GetWindowRect(handle, &rect);
        SetWindowPos(
            handle, NULL,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
        );
    }
    else
    {
        settings = GetPropA(handle, WINDOW_CLASS_NAME "/settings");
    }

    if (settings)
    {
        LRESULT res = 0;
        if (dwm(settings, handle, message, wp, lp, &res))
            return res;
        if (nccalcsize(settings, handle, message, wp, lp, &res))
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

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(handle, &ps);

        HBRUSH brush_bg = CreateSolidBrush(RGB(0xF0, 0xF0, 0xF0));
        FillRect(hdc, &ps.rcPaint, brush_bg);
        DeleteObject(brush_bg);

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

        EndPaint(handle, &ps);

        return 0;
    }
    break;

    case WM_NCHITTEST:
    {
        POINT pt = {LOWORD(lp), HIWORD(lp)};
        ScreenToClient(handle, &pt);

        RECT rect;
        GetClientRect(handle, &rect);

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
            return HTCAPTION;
    }

    case WM_NCMOUSEMOVE:
    {
        printf("WM_NCMOUSEMOVE\n");

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
            InvalidateRect(handle, NULL, FALSE);

        if (!is_tracking)
        {
            TRACKMOUSEEVENT tme = {
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE | TME_NONCLIENT,
                .hwndTrack = handle,
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
        InvalidateRect(handle, NULL, FALSE);
        return 0;
    }

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

    return DefWindowProcA(handle, message, wp, lp);
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
        "Win32 Borderless",
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
    int wc = DwmMargins_MAX * NcCalcSizeMargins_MAX;
    for (int i0 = 0; i0 < DwmMargins_MAX; ++i0)
    for (int i1 = 0; i1 < NcCalcSizeMargins_MAX; ++i1)
    {
        // if (i0 == DwmMarginsNeg) continue;
        create_window((WindowSettings){
            .dwm_margins = i0,
            .nccalcsize_margins = i1
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
