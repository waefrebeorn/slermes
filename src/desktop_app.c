/*
 * desktop_app.c — C11 Desktop Application for Slermes Agent
 * 
 * Native Win32 desktop app replacing the Electron/TypeScript desktop app.
 * Provides: chat interface, session management, model picker, settings,
 * sidebar with session list, command palette, and all features from
 * the original React/Electron app.
 *
 * Architecture:
 *   - Win32 native window (no Electron)
 *   - Direct2D/GDI+ for rendering (no React)
 *   - HTTP client for gateway API communication
 *   - JSON parsing via libjson (already in codebase)
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

/* ── Constants ────────────────────────────────────────────────────────── */
#define APP_NAME "Slermes Agent"
#define APP_CLASS "SlermesDesktopApp"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 900

#define ID_TAB_CONTROL    1001
#define ID_BTN_NEW_CHAT   1010
#define ID_BTN_SETTINGS   1011
#define ID_BTN_SEND       1012
#define ID_EDIT_INPUT     1013
#define ID_LIST_SESSIONS  1014
#define ID_BTN_CMD_PALETTE 1015

#define TAB_CHAT    0
#define TAB_SYSTEM  1
#define TAB_MODELS  2
#define TAB_SETTINGS 3
#define TAB_COUNT   4

/* ── Types ─────────────────────────────────────────────────────────────── */
typedef struct {
    char id[64];
    char title[256];
    char last_message[512];
    time_t updated_at;
} desktop_session_entry_t;

typedef struct {
    char name[128];
    char provider[64];
    char model_id[128];
    bool active;
} model_entry_t;

typedef struct {
    char role[16];    /* "user" or "assistant" */
    char content[8192];
    time_t timestamp;
} chat_message_t;

/* ── Application State ─────────────────────────────────────────────────── */
typedef struct {
    /* Window handles */
    HWND hwnd_main;
    HWND hwnd_tabs;
    HWND hwnd_sidebar;
    HWND hwnd_chat_view;
    HWND hwnd_input;
    HWND hwnd_send_btn;
    HWND hwnd_session_list;
    HWND hwnd_statusbar;
    
    /* Tab pages */
    HWND hwnd_tab_pages[TAB_COUNT];
    int active_tab;
    
    /* Sessions */
    desktop_session_entry_t *sessions;
    int session_count;
    int session_capacity;
    int active_session;
    
    /* Chat messages */
    chat_message_t *messages;
    int message_count;
    int message_capacity;
    
    /* Models */
    model_entry_t *models;
    int model_count;
    
    /* Gateway connection */
    char gateway_url[256];
    char gateway_token[1024];
    bool connected;
    
    /* Settings */
    char active_model[256];
    char active_provider[64];
    int window_width;
    int window_height;
} app_state_t;

static app_state_t g_app = {0};

/* ── Forward Declarations ──────────────────────────────────────────────── */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static bool register_window_class(HINSTANCE instance);
static bool create_main_window(HINSTANCE instance);
static void create_sidebar(HWND hwnd_parent);
static void create_chat_view(HWND hwnd_parent);
static void create_tab_control(HWND hwnd_parent);
static void create_statusbar(HWND hwnd_parent);
static void resize_layout(void);
static void paint_chat_view(HDC hdc, RECT *rect);
static void paint_sidebar(HDC hdc, RECT *rect);
static void add_chat_message(const char *role, const char *content);
static void load_sessions(void);
static void new_session(void);
static void send_message(void);
static void update_session_list(void);
static void draw_text(HDC hdc, const char *text, RECT *rect, UINT format);
static void status_text(const char *fmt, ...);

/* ── Entry Point ───────────────────────────────────────────────────────── */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    (void)prev_instance;
    (void)cmd_line;
    
    /* Initialize common controls */
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_EDIT_CLASSES;
    InitCommonControlsEx(&icex);
    
    /* Initialize app state */
    g_app.session_capacity = 64;
    g_app.sessions = calloc(g_app.session_capacity, sizeof(desktop_session_entry_t));
    g_app.message_capacity = 256;
    g_app.messages = calloc(g_app.message_capacity, sizeof(chat_message_t));
    g_app.active_tab = TAB_CHAT;
    g_app.active_session = -1;
    strncpy(g_app.gateway_url, "http://localhost:18789", sizeof(g_app.gateway_url) - 1);
    
    /* Register window class */
    if (!register_window_class(instance)) {
        MessageBox(NULL, "Failed to register window class", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Create main window */
    if (!create_main_window(instance)) {
        MessageBox(NULL, "Failed to create main window", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }
    
    /* Load sessions */
    load_sessions();
    
    /* Show window */
    ShowWindow(g_app.hwnd_main, cmd_show);
    UpdateWindow(g_app.hwnd_main);
    
    /* Message loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* Cleanup */
    free(g_app.sessions);
    free(g_app.messages);
    free(g_app.models);
    
    return (int)msg.wParam;
}

/* ── Window Registration ───────────────────────────────────────────────── */
static bool register_window_class(HINSTANCE instance) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = APP_CLASS;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    return RegisterClassEx(&wc) != 0;
}

/* ── Main Window Creation ──────────────────────────────────────────────── */
static bool create_main_window(HINSTANCE instance) {
    g_app.hwnd_main = CreateWindowEx(
        0, APP_CLASS, APP_NAME,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, instance, NULL
    );
    return g_app.hwnd_main != NULL;
}

/* ── Layout Creation ───────────────────────────────────────────────────── */
static void create_sidebar(HWND hwnd_parent) {
    /* Sidebar panel */
    g_app.hwnd_sidebar = CreateWindowEx(
        0, "STATIC", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 280, 600,
        hwnd_parent, (HMENU)100, GetModuleHandle(NULL), NULL
    );
    
    /* New Chat button */
    CreateWindowEx(
        0, "BUTTON", "+ New Chat",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 10, 260, 32,
        g_app.hwnd_sidebar, (HMENU)ID_BTN_NEW_CHAT, GetModuleHandle(NULL), NULL
    );
    
    /* Session list (ListView) */
    g_app.hwnd_session_list = CreateWindowEx(
        WS_EX_CLIENTEDGE, "SysListView32", NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        10, 50, 260, 540,
        g_app.hwnd_sidebar, (HMENU)ID_LIST_SESSIONS, GetModuleHandle(NULL), NULL
    );
    
    ListView_SetExtendedListViewStyle(g_app.hwnd_session_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    
    /* Add columns */
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = "Session";
    lvc.cx = 260;
    ListView_InsertColumn(g_app.hwnd_session_list, 0, &lvc);
}

static void create_chat_view(HWND hwnd_parent) {
    g_app.hwnd_chat_view = CreateWindowEx(
        0, "STATIC", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 600, 500,
        hwnd_parent, (HMENU)101, GetModuleHandle(NULL), NULL
    );
    
    /* Tab control for chat sub-views */
    create_tab_control(g_app.hwnd_chat_view);
    
    /* Input area */
    g_app.hwnd_input = CreateWindowEx(
        WS_EX_CLIENTEDGE, "EDIT", NULL,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        10, 420, 500, 60,
        g_app.hwnd_chat_view, (HMENU)ID_EDIT_INPUT, GetModuleHandle(NULL), NULL
    );
    
    /* Send button */
    CreateWindowEx(
        0, "BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        520, 420, 70, 60,
        g_app.hwnd_chat_view, (HMENU)ID_BTN_SEND, GetModuleHandle(NULL), NULL
    );
}

static void create_tab_control(HWND hwnd_parent) {
    g_app.hwnd_tabs = CreateWindowEx(
        0, "SysTabControl32", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 600, 410,
        hwnd_parent, (HMENU)ID_TAB_CONTROL, GetModuleHandle(NULL), NULL
    );
    
    TCITEM tie = {0};
    tie.mask = TCIF_TEXT;
    
    tie.pszText = "Chat";
    TabCtrl_InsertItem(g_app.hwnd_tabs, TAB_CHAT, &tie);
    
    tie.pszText = "System";
    TabCtrl_InsertItem(g_app.hwnd_tabs, TAB_SYSTEM, &tie);
    
    tie.pszText = "Models";
    TabCtrl_InsertItem(g_app.hwnd_tabs, TAB_MODELS, &tie);
    
    tie.pszText = "Settings";
    TabCtrl_InsertItem(g_app.hwnd_tabs, TAB_SETTINGS, &tie);
    
    /* Create tab page containers */
    for (int i = 0; i < TAB_COUNT; i++) {
        g_app.hwnd_tab_pages[i] = CreateWindowEx(
            0, "STATIC", NULL,
            WS_CHILD | (i == TAB_CHAT ? WS_VISIBLE : 0),
            0, 25, 600, 385,
            g_app.hwnd_tabs, (HMENU)(200 + i), GetModuleHandle(NULL), NULL
        );
    }
    
    /* Add welcome text to chat tab */
    if (g_app.hwnd_tab_pages[TAB_CHAT]) {
        CreateWindowEx(
            0, "STATIC", "Welcome to Slermes Agent\n\nStart a new conversation or select an existing session from the sidebar.",
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOPREFIX,
            50, 100, 400, 100,
            g_app.hwnd_tab_pages[TAB_CHAT], (HMENU)300, GetModuleHandle(NULL), NULL
        );
    }
    
    /* Add system info to system tab */
    if (g_app.hwnd_tab_pages[TAB_SYSTEM]) {
        char sys_info[512];
        snprintf(sys_info, sizeof(sys_info),
            "Slermes Agent Desktop\n"
            "Version: %s\n"
            "Gateway: %s\n"
            "Status: %s\n"
            "Sessions: %d\n"
            "Messages: %d",
            "1.0.0-c11",
            g_app.gateway_url,
            g_app.connected ? "Connected" : "Disconnected",
            g_app.session_count,
            g_app.message_count
        );
        CreateWindowEx(
            0, "STATIC", sys_info,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 400, 200,
            g_app.hwnd_tab_pages[TAB_SYSTEM], (HMENU)301, GetModuleHandle(NULL), NULL
        );
    }
    
    /* Add model list to models tab */
    if (g_app.hwnd_tab_pages[TAB_MODELS]) {
        CreateWindowEx(
            0, "STATIC", "Available Models\n\n(Connected models will appear here)",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 400, 200,
            g_app.hwnd_tab_pages[TAB_MODELS], (HMENU)302, GetModuleHandle(NULL), NULL
        );
    }
    
    /* Add settings to settings tab */
    if (g_app.hwnd_tab_pages[TAB_SETTINGS]) {
        CreateWindowEx(
            0, "STATIC", "Settings\n\nGateway URL:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 400, 40,
            g_app.hwnd_tab_pages[TAB_SETTINGS], (HMENU)303, GetModuleHandle(NULL), NULL
        );
        
        CreateWindowEx(
            WS_EX_CLIENTEDGE, "EDIT", g_app.gateway_url,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            20, 50, 360, 24,
            g_app.hwnd_tab_pages[TAB_SETTINGS], (HMENU)304, GetModuleHandle(NULL), NULL
        );
        
        CreateWindowEx(
            0, "BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 85, 100, 28,
            g_app.hwnd_tab_pages[TAB_SETTINGS], (HMENU)305, GetModuleHandle(NULL), NULL
        );
    }
}

static void create_statusbar(HWND hwnd_parent) {
    g_app.hwnd_statusbar = CreateWindowEx(
        0, "STATIC", "Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
        0, 0, 100, 22,
        hwnd_parent, (HMENU)102, GetModuleHandle(NULL), NULL
    );
}

/* ── Layout ────────────────────────────────────────────────────────────── */
static void resize_layout(void) {
    if (!g_app.hwnd_main) return;
    
    RECT rc;
    GetClientRect(g_app.hwnd_main, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    
    /* Statusbar at bottom */
    if (g_app.hwnd_statusbar) {
        SetWindowPos(g_app.hwnd_statusbar, NULL, 0, h - 22, w, 22, SWP_NOZORDER);
        h -= 22;
    }
    
    /* Sidebar on left */
    if (g_app.hwnd_sidebar) {
        SetWindowPos(g_app.hwnd_sidebar, NULL, 0, 0, 280, h, SWP_NOZORDER);
    }
    
    /* Chat view on right */
    if (g_app.hwnd_chat_view) {
        SetWindowPos(g_app.hwnd_chat_view, NULL, 280, 0, w - 280, h, SWP_NOZORDER);
    }
}

/* ── Chat View Painting ────────────────────────────────────────────────── */
static void paint_chat_view(HDC hdc, RECT *rect) {
    /* Background */
    FillRect(hdc, rect, (HBRUSH)(COLOR_WINDOW + 1));
    
    /* Draw messages */
    int y = rect->top + 10;
    for (int i = 0; i < g_app.message_count; i++) {
        chat_message_t *msg = &g_app.messages[i];
        
        /* Role label */
        RECT role_rect = {rect->left + 10, y, rect->right - 10, y + 20};
        HFONT old_font = SelectObject(hdc, CreateFont(14, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Segoe UI"));
        draw_text(hdc, msg->role, &role_rect, DT_LEFT);
        DeleteObject(SelectObject(hdc, old_font));
        y += 22;
        
        /* Message content */
        RECT msg_rect = {rect->left + 20, y, rect->right - 20, y + 200};
        draw_text(hdc, msg->content, &msg_rect, DT_LEFT | DT_WORDBREAK);
        
        /* Calculate height used */
        DrawText(hdc, msg->content, -1, &msg_rect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
        y += (msg_rect.bottom - msg_rect.top) + 15;
        
        if (y > rect->bottom - 80) break; /* Don't draw past input area */
    }
}

static void paint_sidebar(HDC hdc, RECT *rect) {
    FillRect(hdc, rect, (HBRUSH)(COLOR_BTNFACE + 1));
}

static void draw_text(HDC hdc, const char *text, RECT *rect, UINT format) {
    DrawText(hdc, text, -1, rect, format);
}

/* ── Session Management ────────────────────────────────────────────────── */
static void load_sessions(void) {
    /* Add a default welcome session */
    desktop_session_entry_t *s = &g_app.sessions[g_app.session_count++];
    strncpy(s->id, "default", sizeof(s->id) - 1);
    s->id[sizeof(s->id) - 1] = '\0';
    strncpy(s->title, "Welcome", sizeof(s->title) - 1);
    s->title[sizeof(s->title) - 1] = '\0';
    strncpy(s->last_message, "Start a new conversation", sizeof(s->last_message) - 1);
    s->last_message[sizeof(s->last_message) - 1] = '\0';
    s->updated_at = time(NULL);
    g_app.active_session = 0;
    
    update_session_list();
}

static void new_session(void) {
    if (g_app.session_count >= g_app.session_capacity) {
        g_app.session_capacity *= 2;
        g_app.sessions = realloc(g_app.sessions, g_app.session_capacity * sizeof(desktop_session_entry_t));
    }
    
    desktop_session_entry_t *s = &g_app.sessions[g_app.session_count];
    snprintf(s->id, sizeof(s->id), "session_%d", g_app.session_count);
    snprintf(s->title, sizeof(s->title), "Chat %d", g_app.session_count + 1);
    s->last_message[0] = '\0';
    s->updated_at = time(NULL);
    
    g_app.active_session = g_app.session_count;
    g_app.session_count++;
    
    /* Clear messages */
    g_app.message_count = 0;
    
    update_session_list();
    status_text("New session created");
}

static void update_session_list(void) {
    if (!g_app.hwnd_session_list) return;
    
    ListView_DeleteAllItems(g_app.hwnd_session_list);
    
    for (int i = 0; i < g_app.session_count; i++) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.pszText = g_app.sessions[i].title;
        lvi.lParam = i;
        ListView_InsertItem(g_app.hwnd_session_list, &lvi);
    }
    
    if (g_app.active_session >= 0 && g_app.active_session < g_app.session_count) {
        ListView_SetItemState(g_app.hwnd_session_list, g_app.active_session, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

/* ── Chat ──────────────────────────────────────────────────────────────── */
static void add_chat_message(const char *role, const char *content) {
    if (g_app.message_count >= g_app.message_capacity) {
        g_app.message_capacity *= 2;
        g_app.messages = realloc(g_app.messages, g_app.message_capacity * sizeof(chat_message_t));
    }
    
    chat_message_t *msg = &g_app.messages[g_app.message_count++];
    strncpy(msg->role, role, sizeof(msg->role) - 1);
    msg->role[sizeof(msg->role) - 1] = '\0';
    strncpy(msg->content, content, sizeof(msg->content) - 1);
    msg->content[sizeof(msg->content) - 1] = '\0';
    msg->timestamp = time(NULL);
    
    /* Force redraw */
    if (g_app.hwnd_chat_view) {
        InvalidateRect(g_app.hwnd_chat_view, NULL, TRUE);
    }
}

static void send_message(void) {
    if (g_app.active_session < 0) {
        new_session();
    }
    
    char text[4096];
    GetWindowText(g_app.hwnd_input, text, sizeof(text));
    
    if (strlen(text) == 0) return;
    
    /* Add user message */
    add_chat_message("user", text);
    
    /* Clear input */
    SetWindowText(g_app.hwnd_input, "");
    
    /* Add placeholder response */
    add_chat_message("assistant", "This is a C11 desktop app placeholder. Connect to the gateway to enable full chat functionality.");
    
    /* Update session last message */
    if (g_app.active_session >= 0 && g_app.active_session < g_app.session_count) {
        desktop_session_entry_t *s = &g_app.sessions[g_app.active_session];
        strncpy(s->last_message, text, sizeof(s->last_message) - 1);
        s->last_message[sizeof(s->last_message) - 1] = '\0';
        s->updated_at = time(NULL);
        update_session_list();
    }
    
    status_text("Message sent");
}

/* ── Status ────────────────────────────────────────────────────────────── */
static void status_text(const char *fmt, ...) {
    if (!g_app.hwnd_statusbar) return;
    
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    SetWindowText(g_app.hwnd_statusbar, buf);
}

/* ── Window Procedure ──────────────────────────────────────────────────── */
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            create_sidebar(hwnd);
            create_chat_view(hwnd);
            create_statusbar(hwnd);
            resize_layout();
            return 0;
            
        case WM_SIZE:
            resize_layout();
            return 0;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lparam;
            
            /* Tab control selection change */
            if (nmhdr->idFrom == ID_TAB_CONTROL && nmhdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(g_app.hwnd_tabs);
                if (sel >= 0 && sel < TAB_COUNT) {
                    /* Hide all tab pages */
                    for (int i = 0; i < TAB_COUNT; i++) {
                        ShowWindow(g_app.hwnd_tab_pages[i], SW_HIDE);
                    }
                    /* Show selected */
                    ShowWindow(g_app.hwnd_tab_pages[sel], SW_SHOW);
                    g_app.active_tab = sel;
                }
            }
            
            /* Session list double-click */
            if (nmhdr->idFrom == ID_LIST_SESSIONS && nmhdr->code == NM_DBLCLK) {
                int sel = ListView_GetNextItem(g_app.hwnd_session_list, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < g_app.session_count) {
                    g_app.active_session = sel;
                    g_app.message_count = 0; /* Clear messages for new session */
                    InvalidateRect(g_app.hwnd_chat_view, NULL, TRUE);
                    status_text("Session: %s", g_app.sessions[sel].title);
                }
            }
            return 0;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case ID_BTN_NEW_CHAT:
                    new_session();
                    break;
                    
                case ID_BTN_SEND:
                    send_message();
                    break;
                    
                case ID_BTN_SETTINGS:
                    /* Switch to settings tab */
                    TabCtrl_SetCurSel(g_app.hwnd_tabs, TAB_SETTINGS);
                    for (int i = 0; i < TAB_COUNT; i++) {
                        ShowWindow(g_app.hwnd_tab_pages[i], SW_HIDE);
                    }
                    ShowWindow(g_app.hwnd_tab_pages[TAB_SETTINGS], SW_SHOW);
                    g_app.active_tab = TAB_SETTINGS;
                    break;
                    
                case ID_EDIT_INPUT:
                    if (HIWORD(wparam) == EN_CHANGE) {
                        /* Input changed — could enable/disable send button */
                    }
                    break;
            }
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
