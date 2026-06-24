/*
 * window_win32.c — Win32 backend for window.h
 *
 * Implements the cross-platform window.h API using Win32 + WGL + OpenGL.
 * Provides: window creation, event processing, OpenGL rendering context,
 * 2D drawing primitives, clipboard, drag-and-drop, HiDPI support.
 */

#ifndef _WIN32
#error "window_win32.c requires Windows (WIN32)"
#endif

#include "window.h"
#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

/* ── Internal types ───────────────────────────────────────────────────── */

typedef struct {
    char title[WINDOW_MAX_TITLE];
    int width, height;
    bool resizable;
    bool fullscreen;
    bool borderless;
    bool centered;
    int min_width, min_height;
} win32_config_t;

struct window {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    win32_config_t config;
    bool should_close;
    bool focused;
    bool mouse_buttons[3];
    int mouse_x, mouse_y;
    float scale_factor;
    window_cursor_t current_cursor;
    char clipboard_text[WINDOW_MAX_TEXT_INPUT];
    bool dnd_enabled;
    /* Key state */
    bool keys[WINDOW_KEY_COUNT];
    char text_input[WINDOW_MAX_TEXT_INPUT];
    size_t text_len;
    /* Event queue */
    window_event_t event_queue[64];
    int event_head, event_tail;
    /* GL state for 2D rendering */
    GLuint font_texture;
    GLuint vao, vbo;
    GLuint shader_program;
};

/* ── Static state ─────────────────────────────────────────────────────── */

static const char *WINDOW_CLASS_NAME = "HermesWindow";
static bool s_class_registered = false;
static int s_window_count = 0;

/* ── Key code mapping ─────────────────────────────────────────────────── */

static window_key_t map_vk_to_key(WPARAM vk) {
    switch (vk) {
    case VK_RETURN:    return KEY_ENTER;
    case VK_BACK:      return KEY_BACKSPACE;
    case VK_TAB:       return KEY_TAB;
    case VK_ESCAPE:    return KEY_ESCAPE;
    case VK_DELETE:    return KEY_DELETE;
    case VK_SPACE:     return KEY_SPACE;
    case VK_LEFT:      return KEY_LEFT;
    case VK_RIGHT:     return KEY_RIGHT;
    case VK_UP:        return KEY_UP;
    case VK_DOWN:      return KEY_DOWN;
    case VK_HOME:      return KEY_HOME;
    case VK_END:       return KEY_END;
    case VK_PRIOR:     return KEY_PAGE_UP;
    case VK_NEXT:      return KEY_PAGE_DOWN;
    case VK_F1:        return KEY_F1;
    case VK_F2:        return KEY_F2;
    case VK_F3:        return KEY_F3;
    case VK_F4:        return KEY_F4;
    case VK_F5:        return KEY_F5;
    case VK_F6:        return KEY_F6;
    case VK_F7:        return KEY_F7;
    case VK_F8:        return KEY_F8;
    case VK_F9:        return KEY_F9;
    case VK_F10:       return KEY_F10;
    case VK_F11:       return KEY_F11;
    case VK_F12:       return KEY_F12;
    case VK_SHIFT:     return KEY_LSHIFT;
    case VK_LSHIFT:    return KEY_LSHIFT;
    case VK_RSHIFT:    return KEY_RSHIFT;
    case VK_CONTROL:   return KEY_LCTRL;
    case VK_LCONTROL:  return KEY_LCTRL;
    case VK_RCONTROL:  return KEY_RCTRL;
    case VK_MENU:      return KEY_LALT;
    case VK_LMENU:     return KEY_LALT;
    case VK_RMENU:     return KEY_RALT;
    case VK_LWIN:      return KEY_LSUPER;
    case VK_RWIN:      return KEY_RSUPER;
    default:
        if (vk >= 'A' && vk <= 'Z') return (window_key_t)vk;
        if (vk >= '0' && vk <= '9') return (window_key_t)vk;
        return KEY_NONE;
    }
}

static uint32_t get_mods(void) {
    uint32_t mods = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   mods |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)  mods |= MOD_CTRL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)     mods |= MOD_ALT;
    if (GetAsyncKeyState(VK_LWIN) & 0x8000 ||
        GetAsyncKeyState(VK_RWIN) & 0x8000)     mods |= MOD_SUPER;
    return mods;
}

/* ── Event queue ───────────────────────────────────────────────────────── */

static void push_event(window_t *w, const window_event_t *ev) {
    int next = (w->event_head + 1) % 64;
    if (next != w->event_tail) {
        w->event_queue[w->event_head] = *ev;
        w->event_head = next;
    }
}

static bool pop_event(window_t *w, window_event_t *ev) {
    if (w->event_head == w->event_tail) return false;
    *ev = w->event_queue[w->event_tail];
    w->event_tail = (w->event_tail + 1) % 64;
    return true;
}

/* ── Window procedure ─────────────────────────────────────────────────── */

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    window_t *w = (window_t *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!w) return DefWindowProc(hwnd, msg, wparam, lparam);

    window_event_t ev;
    memset(&ev, 0, sizeof(ev));

    switch (msg) {
    case WM_CLOSE:
        ev.type = EVENT_CLOSE;
        push_event(w, &ev);
        w->should_close = true;
        return 0;

    case WM_SIZE:
        ev.type = EVENT_RESIZE;
        ev.width = LOWORD(lparam);
        ev.height = HIWORD(lparam);
        w->config.width = ev.width;
        w->config.height = ev.height;
        push_event(w, &ev);
        return 0;

    case WM_PAINT: {
        ev.type = EVENT_EXPOSE;
        push_event(w, &ev);
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETFOCUS:
        ev.type = EVENT_FOCUS_IN;
        w->focused = true;
        push_event(w, &ev);
        return 0;

    case WM_KILLFOCUS:
        ev.type = EVENT_FOCUS_OUT;
        w->focused = false;
        push_event(w, &ev);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        ev.type = EVENT_KEY_DOWN;
        ev.key = map_vk_to_key(wparam);
        ev.mods = get_mods();
        w->keys[ev.key] = true;
        push_event(w, &ev);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        ev.type = EVENT_KEY_UP;
        ev.key = map_vk_to_key(wparam);
        ev.mods = get_mods();
        w->keys[ev.key] = false;
        push_event(w, &ev);
        return 0;

    case WM_CHAR:
        if (wparam >= 32 && wparam < 127) {
            ev.type = EVENT_TEXT_INPUT;
            ev.text = w->text_input;
            w->text_input[0] = (char)wparam;
            w->text_input[1] = '\0';
            ev.text_len = 1;
            push_event(w, &ev);
        }
        return 0;

    case WM_LBUTTONDOWN:
        ev.type = EVENT_MOUSE_DOWN;
        ev.button = MOUSE_LEFT;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[0] = true;
        SetCapture(hwnd);
        push_event(w, &ev);
        return 0;

    case WM_LBUTTONUP:
        ev.type = EVENT_MOUSE_UP;
        ev.button = MOUSE_LEFT;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[0] = false;
        ReleaseCapture();
        push_event(w, &ev);
        return 0;

    case WM_RBUTTONDOWN:
        ev.type = EVENT_MOUSE_DOWN;
        ev.button = MOUSE_RIGHT;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[2] = true;
        SetCapture(hwnd);
        push_event(w, &ev);
        return 0;

    case WM_RBUTTONUP:
        ev.type = EVENT_MOUSE_UP;
        ev.button = MOUSE_RIGHT;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[2] = false;
        ReleaseCapture();
        push_event(w, &ev);
        return 0;

    case WM_MBUTTONDOWN:
        ev.type = EVENT_MOUSE_DOWN;
        ev.button = MOUSE_MIDDLE;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[1] = true;
        SetCapture(hwnd);
        push_event(w, &ev);
        return 0;

    case WM_MBUTTONUP:
        ev.type = EVENT_MOUSE_UP;
        ev.button = MOUSE_MIDDLE;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_buttons[1] = false;
        ReleaseCapture();
        push_event(w, &ev);
        return 0;

    case WM_MOUSEMOVE:
        ev.type = EVENT_MOUSE_MOVE;
        ev.x = GET_X_LPARAM(lparam);
        ev.y = GET_Y_LPARAM(lparam);
        ev.mods = get_mods();
        w->mouse_x = ev.x;
        w->mouse_y = ev.y;
        push_event(w, &ev);
        return 0;

    case WM_MOUSEWHEEL:
        ev.type = EVENT_MOUSE_SCROLL;
        ev.scroll_dy = (float)GET_WHEEL_DELTA_WPARAM(wparam) / 120.0f;
        ev.scroll_dx = 0;
        {
            POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            ScreenToClient(hwnd, &pt);
            ev.x = pt.x;
            ev.y = pt.y;
        }
        ev.mods = get_mods();
        push_event(w, &ev);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

/* ── Window lifecycle ─────────────────────────────────────────────────── */

window_t *window_create(const window_config_t *config) {
    if (!s_class_registered) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = WINDOW_CLASS_NAME;
        RegisterClassEx(&wc);
        s_class_registered = true;
    }

    window_t *w = (window_t *)calloc(1, sizeof(window_t));
    if (!w) return NULL;

    w->config.width = config->width > 0 ? config->width : 1280;
    w->config.height = config->height > 0 ? config->height : 900;
    w->config.resizable = config->resizable;
    w->config.fullscreen = config->fullscreen;
    w->config.borderless = config->borderless;
    w->config.centered = config->centered;
    w->config.min_width = config->min_width > 0 ? config->min_width : 320;
    w->config.min_height = config->min_height > 0 ? config->min_height : 240;
    strncpy(w->config.title, config->title ? config->title : "Slermes", WINDOW_MAX_TITLE - 1);

    /* HiDPI */
    w->scale_factor = 1.0f;
    HDC screen = GetDC(NULL);
    int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);
    if (dpi > 96) w->scale_factor = (float)dpi / 96.0f;

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!w->config.resizable) style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (w->config.borderless) style = WS_POPUP;

    RECT rect = { 0, 0, w->config.width, w->config.height };
    AdjustWindowRect(&rect, style, FALSE);

    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (w->config.centered) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        x = (screen_w - (rect.right - rect.left)) / 2;
        y = (screen_h - (rect.bottom - rect.top)) / 2;
    }

    w->hwnd = CreateWindowEx(
        0, WINDOW_CLASS_NAME, w->config.title,
        style, x, y,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!w->hwnd) { free(w); return NULL; }

    SetWindowLongPtr(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);

    /* Create OpenGL context */
    w->hdc = GetDC(w->hwnd);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int pf = ChoosePixelFormat(w->hdc, &pfd);
    SetPixelFormat(w->hdc, pf, &pfd);
    w->hglrc = wglCreateContext(w->hdc);
    wglMakeCurrent(w->hdc, w->hglrc);

    ShowWindow(w->hwnd, SW_SHOW);
    s_window_count++;
    return w;
}

void window_destroy(window_t *w) {
    if (!w) return;
    if (w->hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(w->hglrc);
    }
    if (w->hdc && w->hwnd) ReleaseDC(w->hwnd, w->hdc);
    if (w->hwnd) DestroyWindow(w->hwnd);
    free(w);
    s_window_count--;
    if (s_window_count <= 0) s_class_registered = false;
}

void window_set_title(window_t *w, const char *title) {
    if (!w || !title) return;
    strncpy(w->config.title, title, WINDOW_MAX_TITLE - 1);
    SetWindowText(w->hwnd, title);
}

void window_set_min_size(window_t *w, int min_w, int min_h) {
    if (!w) return;
    w->config.min_width = min_w;
    w->config.min_height = min_h;
}

void window_show(window_t *w) { if (w && w->hwnd) ShowWindow(w->hwnd, SW_SHOW); }
void window_hide(window_t *w) { if (w && w->hwnd) ShowWindow(w->hwnd, SW_HIDE); }
void window_focus(window_t *w) { if (w && w->hwnd) SetForegroundWindow(w->hwnd); }
void window_request_close(window_t *w) { if (w) w->should_close = true; }

/* ── Event processing ─────────────────────────────────────────────────── */

bool window_poll_event(window_t *w, window_event_t *ev) {
    if (!w || !ev) return false;
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return pop_event(w, ev);
}

bool window_wait_event(window_t *w, window_event_t *ev) {
    if (!w || !ev) return false;
    if (pop_event(w, ev)) return true;
    WaitMessage();
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return pop_event(w, ev);
}

bool window_wait_event_timeout(window_t *w, window_event_t *ev, int timeout_ms) {
    if (!w || !ev) return false;
    if (pop_event(w, ev)) return true;
    MsgWaitForMultipleObjects(0, NULL, FALSE, timeout_ms, QS_ALLINPUT);
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return pop_event(w, ev);
}

/* ── Size / position ──────────────────────────────────────────────────── */

void window_get_size(window_t *w, int *width, int *height) {
    if (!w) return;
    RECT r;
    GetClientRect(w->hwnd, &r);
    if (width) *width = r.right - r.left;
    if (height) *height = r.bottom - r.top;
}

void window_set_size(window_t *w, int width, int height) {
    if (!w || !w->hwnd) return;
    RECT r = {0, 0, width, height};
    AdjustWindowRect(&r, GetWindowLong(w->hwnd, GWL_STYLE), FALSE);
    SetWindowPos(w->hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

void window_get_position(window_t *w, int *x, int *y) {
    if (!w) return;
    RECT r;
    GetWindowRect(w->hwnd, &r);
    if (x) *x = r.left;
    if (y) *y = r.top;
}

void window_set_position(window_t *w, int x, int y) {
    if (!w || !w->hwnd) return;
    SetWindowPos(w->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

float window_get_scale(window_t *w) {
    return w ? w->scale_factor : 1.0f;
}

void window_set_fullscreen(window_t *w, bool fullscreen) {
    if (!w || !w->hwnd) return;
    if (fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        HMONITOR mon = MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfo(mon, &mi);
        SetWindowLong(w->hwnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(w->hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    } else {
        SetWindowLong(w->hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(w->hwnd, NULL, 0, 0, w->config.width, w->config.height,
            SWP_NOMOVE | SWP_NOZORDER);
    }
    w->config.fullscreen = fullscreen;
}

bool window_is_fullscreen(window_t *w) {
    return w ? w->config.fullscreen : false;
}

/* ── Rendering ─────────────────────────────────────────────────────────── */

window_renderer_t *window_get_renderer(window_t *w) {
    return (window_renderer_t *)w; /* Simplified: renderer is embedded in window */
}

void window_swap_buffers(window_t *w) {
    if (w && w->hdc) SwapBuffers(w->hdc);
}

void window_render_begin(window_t *w, float r, float g, float b, float a) {
    (void)w;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int ww, wh;
    window_get_size(w, &ww, &wh);
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void window_render_end(window_t *w) {
    (void)w;
    glFlush();
}

void window_clear(window_t *w, float r, float g, float b, float a) {
    (void)w;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void window_draw_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void window_fill_rect(window_t *w, float x, float y, float width, float height,
                     float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void window_fill_rect_rounded(window_t *w, float x, float y, float width, float height,
                             float radius, float r, float g, float b, float a) {
    (void)w;
    /* Approximate rounded rect with a regular rect (simplified) */
    window_fill_rect(w, x + radius, y, width - 2*radius, height, r, g, b, a);
    window_fill_rect(w, x, y + radius, width, height - 2*radius, r, g, b, a);
}

void window_fill_circle(window_t *w, float cx, float cy, float radius,
                       float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 32; i++) {
        float angle = 2.0f * 3.14159f * i / 32;
        glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
    }
    glEnd();
}

void window_draw_text(window_t *w, const char *text, float x, float y,
                     float size, float r, float g, float b, float a) {
    (void)w; (void)size;
    glColor4f(r, g, b, a);
    glRasterPos2f(x, y);
    /* Simplified: use bitmap text */
    while (*text) {
        /* Would use wglUseFontBitmaps in full implementation */
        text++;
    }
}

float window_text_width(window_t *w, const char *text, float size) {
    (void)w; (void)text; (void)size;
    return 0.0f; /* TODO: implement font metrics */
}

void window_draw_line(window_t *w, float x0, float y0, float x1, float y1,
                     float line_width, float r, float g, float b, float a) {
    (void)w;
    glColor4f(r, g, b, a);
    glLineWidth(line_width);
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
}

void window_draw_image(window_t *w, const uint8_t *pixels,
                      int img_w, int img_h,
                      float x, float y, float width, float height) {
    (void)w;
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + width, y);
    glTexCoord2f(1, 1); glVertex2f(x + width, y + height);
    glTexCoord2f(0, 1); glVertex2f(x, y + height);
    glEnd();
    glDeleteTextures(1, &tex);
    glDisable(GL_TEXTURE_2D);
}

/* ── Mouse / cursor ────────────────────────────────────────────────────── */

void window_set_cursor(window_t *w, window_cursor_t cursor) {
    if (!w) return;
    w->current_cursor = cursor;
    LPCSTR idc;
    switch (cursor) {
    case CURSOR_IBEAM:    idc = IDC_IBEAM; break;
    case CURSOR_HAND:     idc = IDC_HAND; break;
    case CURSOR_HRESIZE:  idc = IDC_SIZEWE; break;
    case CURSOR_VRESIZE:  idc = IDC_SIZENS; break;
    case CURSOR_CROSSHAIR:idc = IDC_CROSS; break;
    default:              idc = IDC_ARROW; break;
    }
    SetCursor(LoadCursor(NULL, idc));
}

void window_get_mouse_pos(window_t *w, int *x, int *y) {
    if (!w) return;
    if (x) *x = w->mouse_x;
    if (y) *y = w->mouse_y;
}

bool window_get_mouse_button(window_t *w, mouse_button_t button) {
    if (!w || button > MOUSE_RIGHT) return false;
    return w->mouse_buttons[button];
}

void window_set_mouse_pos(window_t *w, int x, int y) {
    if (!w || !w->hwnd) return;
    POINT pt = {x, y};
    ClientToScreen(w->hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

/* ── Clipboard ─────────────────────────────────────────────────────────── */

const char *window_clipboard_get(window_t *w) {
    if (!w || !w->hwnd) return "";
    if (!OpenClipboard(w->hwnd)) return w->clipboard_text;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) {
        const char *src = (const char *)GlobalLock(h);
        if (src) {
            strncpy(w->clipboard_text, src, WINDOW_MAX_TEXT_INPUT - 1);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return w->clipboard_text;
}

void window_clipboard_set(window_t *w, const char *text) {
    if (!w || !w->hwnd || !text) return;
    strncpy(w->clipboard_text, text, WINDOW_MAX_TEXT_INPUT - 1);
    if (!OpenClipboard(w->hwnd)) return;
    EmptyClipboard();
    size_t len = strlen(text) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
    if (h) {
        memcpy(GlobalLock(h), text, len);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
}

/* ── Drag and drop ─────────────────────────────────────────────────────── */

void window_enable_dnd(window_t *w, bool enable) {
    if (!w || !w->hwnd) return;
    w->dnd_enabled = enable;
    DragAcceptFiles(w->hwnd, enable ? TRUE : FALSE);
}

/* ── Window state management ───────────────────────────────────────────── */

/* PoP: window_minimize @ apps/desktop/src/app/window/index.tsx */
void window_minimize(window_t *w) {
    if (!w || !w->hwnd) return;
    ShowWindow(w->hwnd, SW_MINIMIZE);
}

/* PoP: window_maximize @ apps/desktop/src/app/window/index.tsx */
void window_maximize(window_t *w) {
    if (!w || !w->hwnd) return;
    ShowWindow(w->hwnd, SW_MAXIMIZE);
}

/* PoP: window_restore @ apps/desktop/src/app/window/index.tsx */
void window_restore(window_t *w) {
    if (!w || !w->hwnd) return;
    ShowWindow(w->hwnd, SW_RESTORE);
}

/* ── Transparency / Always-on-Top ──────────────────────────────────────── */

/* PoP: window_transparency @ apps/desktop/src/app/window/index.tsx */
void window_set_opacity(window_t *w, float opacity) {
    if (!w || !w->hwnd) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    /* Set layered window attribute for per-window alpha */
    LONG ex = GetWindowLong(w->hwnd, GWL_EXSTYLE);
    SetWindowLong(w->hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(w->hwnd, 0, (BYTE)(opacity * 255), LWA_ALPHA);
}

float window_get_opacity(window_t *w) {
    if (!w || !w->hwnd) return 1.0f;
    BYTE alpha = 0;
    DWORD flags = 0;
    if (GetLayeredWindowAttributes(w->hwnd, NULL, &alpha, &flags)) {
        if (flags & LWA_ALPHA) return (float)alpha / 255.0f;
    }
    return 1.0f;
}

/* PoP: window_always_on_top @ apps/desktop/src/app/window/index.tsx */
void window_set_always_on_top(window_t *w, bool enabled) {
    if (!w || !w->hwnd) return;
    SetWindowPos(w->hwnd,
        enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool window_is_always_on_top(window_t *w) {
    if (!w || !w->hwnd) return false;
    LONG ex = GetWindowLong(w->hwnd, GWL_EXSTYLE);
    return (ex & WS_EX_TOPMOST) != 0;
}

/* ── Tray icon ──────────────────────────────────────────────────────────── */

/* PoP: window_tray @ apps/desktop/src/app/window/index.tsx */
bool window_set_tray_icon(window_t *w, const window_tray_config_t *config) {
    if (!w || !w->hwnd || !config) return false;
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = w->hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    /* Load icon from file or use default application icon */
    if (config->icon_path && config->icon_path[0]) {
        nid.hIcon = (HICON)LoadImage(NULL, config->icon_path,
            IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    }
    if (!nid.hIcon) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    if (config->tooltip) {
        strncpy(nid.szTip, config->tooltip, sizeof(nid.szTip) - 1);
    }
    if (config->visible) {
        Shell_NotifyIcon(NIM_ADD, &nid);
    } else {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
    if (nid.hIcon && nid.hIcon != LoadIcon(NULL, IDI_APPLICATION)) {
        DestroyIcon(nid.hIcon);
    }
    return true;
}

bool window_remove_tray(window_t *w) {
    if (!w || !w->hwnd) return false;
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = w->hwnd;
    nid.uID = 1;
    return Shell_NotifyIcon(NIM_DELETE, &nid) ? true : false;
}

/* ── Global shortcuts / hotkeys ─────────────────────────────────────────── */

/* PoP: window_hotkey @ apps/desktop/src/app/window/index.tsx */
#define WIN32_MAX_HOTKEYS 64

typedef struct {
    char id[64];
    uint32_t key;
    uint32_t mods;
    int win32_id;
} win32_hotkey_entry_t;

static win32_hotkey_entry_t s_hotkeys[WIN32_MAX_HOTKEYS];
static int s_hotkey_count = 0;
static int s_next_hotkey_id = 1;

static uint32_t map_mods_to_win32(uint32_t mods) {
    uint32_t w32 = 0;
    if (mods & MOD_ALT)   w32 |= MOD_ALT;
    if (mods & MOD_CTRL)  w32 |= MOD_CONTROL;
    if (mods & MOD_SHIFT) w32 |= MOD_SHIFT;
    if (mods & MOD_SUPER) w32 |= MOD_WIN;
    return w32;
}

bool window_register_hotkey(window_t *w, const window_hotkey_t *hotkey) {
    if (!w || !hotkey || !hotkey->id) return false;
    if (s_hotkey_count >= WIN32_MAX_HOTKEYS) return false;
    /* Check for duplicate ID */
    for (int i = 0; i < s_hotkey_count; i++) {
        if (strcmp(s_hotkeys[i].id, hotkey->id) == 0) return false;
    }
    int id = s_next_hotkey_id++;
    uint32_t w32_mods = map_mods_to_win32(hotkey->mods);
    /* Map window_key_t to VK code — simplified for common keys */
    UINT vk = (UINT)hotkey->key;
    if (hotkey->key >= KEY_F1 && hotkey->key <= KEY_F12) {
        vk = VK_F1 + (hotkey->key - KEY_F1);
    }
    if (RegisterHotKey(w->hwnd, id, w32_mods, vk)) {
        win32_hotkey_entry_t *e = &s_hotkeys[s_hotkey_count++];
        strncpy(e->id, hotkey->id, sizeof(e->id) - 1);
        e->key = hotkey->key;
        e->mods = hotkey->mods;
        e->win32_id = id;
        return true;
    }
    return false;
}

bool window_unregister_hotkey(window_t *w, const char *id) {
    if (!w || !id) return false;
    for (int i = 0; i < s_hotkey_count; i++) {
        if (strcmp(s_hotkeys[i].id, id) == 0) {
            UnregisterHotKey(w->hwnd, s_hotkeys[i].win32_id);
            /* Remove from array */
            for (int j = i; j < s_hotkey_count - 1; j++) {
                s_hotkeys[j] = s_hotkeys[j + 1];
            }
            s_hotkey_count--;
            return true;
        }
    }
    return false;
}

int window_list_hotkeys(window_t *w, window_hotkey_t *out, int max_count) {
    (void)w;
    if (!out || max_count <= 0) return 0;
    int n = s_hotkey_count < max_count ? s_hotkey_count : max_count;
    for (int i = 0; i < n; i++) {
        strncpy((char *)out[i].id, s_hotkeys[i].id, sizeof(out[i].id) - 1);
        out[i].key = s_hotkeys[i].key;
        out[i].mods = s_hotkeys[i].mods;
        out[i].description = "";
    }
    return n;
}

/* ── Deep linking ───────────────────────────────────────────────────────── */

/* PoP: window_deep_link @ apps/desktop/src/app/window/index.tsx */
static deep_link_cb g_deep_link_cb = NULL;

void window_set_deep_link_callback(window_t *w, deep_link_cb cb) {
    (void)w;
    g_deep_link_cb = cb;
}

bool window_handle_deep_link(window_t *w, const char *url) {
    (void)w;
    if (!url || strncmp(url, "hermes://", 9) != 0) return false;
    const char *action = url + 9;
    const char *params = strchr(action, '?');
    char action_buf[256];
    if (params) {
        size_t len = (size_t)(params - action);
        if (len >= sizeof(action_buf)) len = sizeof(action_buf) - 1;
        strncpy(action_buf, action, len);
        action_buf[len] = '\0';
        params++;
    } else {
        strncpy(action_buf, action, sizeof(action_buf) - 1);
        params = "";
    }
    if (g_deep_link_cb) {
        g_deep_link_cb(url, action_buf, params);
    }
    return true;
}

/* ── Terminal hyperlinks ────────────────────────────────────────────────── */

/* PoP: terminal_web_links @ apps/desktop/src/app/terminal/index.tsx */
void window_terminal_enable_hyperlinks(window_t *w, bool enable) {
    (void)w;
    /* Enable Windows 10+ virtual terminal processing for hyperlink support */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return;
    if (enable) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    } else {
        mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    }
    SetConsoleMode(hOut, mode);
}

bool window_terminal_has_hyperlinks(window_t *w) {
    (void)w;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

/* ── Platform info ─────────────────────────────────────────────────────── */

const char *window_platform_name(void) { return "win32"; }
bool window_platform_has_gpu(void) { return true; }
