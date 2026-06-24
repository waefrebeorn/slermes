/*
 * clipboard.c — Platform clipboard read/write for C11 desktop app
 *
 * Linux (Wayland): wl-copy / wl-paste
 * Linux (X11):     xclip / xsel
 * macOS:           pbcopy / pbpaste
 * Windows:         Win32 Clipboard API
 *
 * PoP: clipboard_read  @ electron/main.cjs:readClipboard
 * PoP: clipboard_write @ electron/main.cjs:writeClipboard
 */

#include "clipboard.h"
#include "hermes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

/* ── Internal: run a command and capture output ─────────────────────────── */

static char *run_cmd_capture(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) { pclose(fp); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, fp)) > 0) {
        len += n;
        if (len >= cap - 1) {
            cap *= 2;
            if (cap > CLIPBOARD_MAX_TEXT) break;
            char *new_buf = realloc(buf, cap);
            if (!new_buf) break;
            buf = new_buf;
        }
    }
    buf[len] = '\0';
    pclose(fp);
    return buf;
}

/* ── Linux/Wayland ──────────────────────────────────────────────────────── */

#if defined(__linux__) && !defined(_WIN32)

static const char *detect_clipboard_tool(void) {
    /* Prefer Wayland tools */
    if (getenv("WAYLAND_DISPLAY")) {
        if (system("which wl-paste > /dev/null 2>&1") == 0)
            return "wayland";
    }
    /* Fall back to xclip */
    if (system("which xclip > /dev/null 2>&1") == 0)
        return "xclip";
    /* Fall back to xsel */
    if (system("which xsel > /dev/null 2>&1") == 0)
        return "xsel";
    return NULL;
}

/* PoP: clipboard_read @ electron/main.cjs:readClipboard */
char *clipboard_read_text(void) {
    const char *tool = detect_clipboard_tool();
    if (!tool) {
        fprintf(stderr, "clipboard_read: no clipboard tool found");
        return NULL;
    }

    char cmd[256];
    if (strcmp(tool, "wayland") == 0) {
        snprintf(cmd, sizeof(cmd), "wl-paste --no-newline 2>/dev/null");
    } else if (strcmp(tool, "xclip") == 0) {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard -o 2>/dev/null");
    } else {
        snprintf(cmd, sizeof(cmd), "xsel --clipboard --output 2>/dev/null");
    }

    return run_cmd_capture(cmd);
}

/* PoP: clipboard_write @ electron/main.cjs:writeClipboard */
bool clipboard_write_text(const char *text) {
    if (!text) return false;

    const char *tool = detect_clipboard_tool();
    if (!tool) {
        fprintf(stderr, "clipboard_write: no clipboard tool found");
        return false;
    }

    char cmd[512];
    if (strcmp(tool, "wayland") == 0) {
        snprintf(cmd, sizeof(cmd), "wl-copy 2>/dev/null");
    } else if (strcmp(tool, "xclip") == 0) {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard -i 2>/dev/null");
    } else {
        snprintf(cmd, sizeof(cmd), "xsel --clipboard --input 2>/dev/null");
    }

    FILE *fp = popen(cmd, "w");
    if (!fp) return false;

    fputs(text, fp);
    int ret = pclose(fp);
    return ret == 0;
}

bool clipboard_available(void) {
    return detect_clipboard_tool() != NULL;
}

bool clipboard_has_text(void) {
    char *text = clipboard_read_text();
    if (!text) return false;
    bool has = (text[0] != '\0');
    free(text);
    return has;
}

#endif /* __linux__ */

/* ── macOS ──────────────────────────────────────────────────────────────── */

#if defined(__APPLE__) && !defined(_WIN32)

/* PoP: clipboard_read @ electron/main.cjs:readClipboard */
char *clipboard_read_text(void) {
    return run_cmd_capture("pbpaste 2>/dev/null");
}

/* PoP: clipboard_write @ electron/main.cjs:writeClipboard */
bool clipboard_write_text(const char *text) {
    if (!text) return false;
    FILE *fp = popen("pbcopy", "w");
    if (!fp) return false;
    fputs(text, fp);
    return pclose(fp) == 0;
}

bool clipboard_available(void) {
    return system("which pbpaste > /dev/null 2>&1") == 0;
}

bool clipboard_has_text(void) {
    char *text = clipboard_read_text();
    if (!text) return false;
    bool has = (text[0] != '\0');
    free(text);
    return has;
}

#endif /* __APPLE__ */

/* ── Windows ────────────────────────────────────────────────────────────── */

#if defined(_WIN32)

/* PoP: clipboard_read @ electron/main.cjs:readClipboard */
char *clipboard_read_text(void) {
    if (!OpenClipboard(NULL)) return NULL;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) { CloseClipboard(); return NULL; }

    wchar_t *wstr = (wchar_t *)GlobalLock(hData);
    if (!wstr) { CloseClipboard(); return NULL; }

    /* Convert wide to UTF-8 */
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char *buf = malloc(len > 0 ? len : 1);
    if (buf) {
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, len, NULL, NULL);
    }

    GlobalUnlock(hData);
    CloseClipboard();
    return buf;
}

/* PoP: clipboard_write @ electron/main.cjs:writeClipboard */
bool clipboard_write_text(const char *text) {
    if (!text) return false;

    /* Convert UTF-8 to wide */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wlen <= 0) return false;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (size_t)wlen * sizeof(wchar_t));
    if (!hMem) return false;

    wchar_t *wbuf = (wchar_t *)GlobalLock(hMem);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf, wlen);
    GlobalUnlock(hMem);

    if (!OpenClipboard(NULL)) { GlobalFree(hMem); return false; }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

bool clipboard_available(void) {
    return OpenClipboard(NULL) != 0;
}

bool clipboard_has_text(void) {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
}

#endif /* _WIN32 */

/* ── Image clipboard (stub) ─────────────────────────────────────────────── */

void *clipboard_read_image(size_t *out_len) {
    (void)out_len;
    return NULL;
}

bool clipboard_write_image(const void *png_data, size_t len) {
    (void)png_data;
    (void)len;
    return false;
}
