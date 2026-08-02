/*
 * port_clipboard_helpers.c — C port of hermes_cli/clipboard.py
 *
 * Cross-platform clipboard access: macOS (osascript), Windows (PowerShell),
 * Linux (xclip), WSL (powershell.exe bridging), Wayland (wl-paste).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* ── Shell helper ────────────────────────────────────────────── */
static char *run_cmd_capture(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[8192] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    if (n == 0) return NULL;
    /* strip trailing newlines */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return strdup(buf);
}

static int run_cmd_silent(const char *cmd) {
    return system(cmd);
}

/* ── macOS ───────────────────────────────────────────────────── */
#if defined(__APPLE__)
/* PoP: _macos_save @ hermes_cli/clipboard.py:_macos_save */
/* PoP: clipboard_macos_save @ hermes_cli/clipboard.py:_macos_save */
int clipboard_macos_save(const char *text) {
    if (!text) return -1;
    FILE *fp = popen("pbcopy", "w");
    if (!fp) return -1;
    fputs(text, fp);
    return pclose(fp);
}
/* PoP: _macos_has_image @ hermes_cli/clipboard.py:_macos_has_image */
bool clipboard_macos_has_image(void) {
    char *r = run_cmd_capture("osascript -e 'tell application \"System Events\" to get the clipboard' 2>/dev/null | grep -q 'TIFF\\|PNG\\|JPEG'");
    return r != NULL;
}
/* PoP: _macos_pngpaste @ hermes_cli/clipboard.py:_macos_pngpaste */
char *clipboard_macos_pngpaste(const char *out_path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "pngpaste %s 2>/dev/null", out_path ? out_path : "/tmp/clip.png");
    run_cmd_silent(cmd);
    return strdup(out_path ? out_path : "/tmp/clip.png");
}
/* PoP: _macos_osascript @ hermes_cli/clipboard.py:_macos_osascript */
char *clipboard_macos_osascript(const char *script) {
    if (!script) return NULL;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "osascript -e '%s' 2>/dev/null", script);
    return run_cmd_capture(cmd);
}
#else
/* PoP: _macos_save @ hermes_cli/clipboard.py:_macos_save */
int clipboard_macos_save(const char *text) { (void)text; return -1; }
/* PoP: _macos_has_image @ hermes_cli/clipboard.py:_macos_has_image */
bool clipboard_macos_has_image(void) { return false; }
/* PoP: _macos_pngpaste @ hermes_cli/clipboard.py:_macos_pngpaste */
char *clipboard_macos_pngpaste(const char *out_path) { (void)out_path; return NULL; }
/* PoP: _macos_osascript @ hermes_cli/clipboard.py:_macos_osascript */
char *clipboard_macos_osascript(const char *script) { (void)script; return NULL; }
#endif

/* ── Windows / PowerShell ──────────────────────────────────────── */
/* PoP: _run_powershell @ hermes_cli/clipboard.py:_run_powershell */
char *clipboard_run_powershell(const char *script) {
    if (!script) return NULL;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "powershell.exe -NoProfile -Command \"%s\" 2>/dev/null", script);
    return run_cmd_capture(cmd);
}

/* PoP: _write_base64_image @ hermes_cli/clipboard.py:_write_base64_image */
bool clipboard_write_base64_image(const char *b64, const char *out_path) {
    if (!b64 || !out_path) return false;
    char cmd[16384];
    snprintf(cmd, sizeof(cmd), "echo '%s' | base64 -d > %s 2>/dev/null", b64, out_path);
    return run_cmd_silent(cmd) == 0;
}

/* PoP: _powershell_has_image @ hermes_cli/clipboard.py:_powershell_has_image */
bool clipboard_powershell_has_image(void) {
    char *r = clipboard_run_powershell("Get-Clipboard -Format Image 2>$null | Select-Object -First 1");
    bool found = r && r[0];
    free(r);
    return found;
}

/* PoP: _powershell_save_image @ hermes_cli/clipboard.py:_powershell_save_image */
char *clipboard_powershell_save_image(const char *out_path) {
    char script[1024];
    snprintf(script, sizeof(script), "Get-Clipboard -Format Image | ForEach-Object { $_.Save('%s', 'PNG') }", out_path ? out_path : "/tmp/clip.png");
    clipboard_run_powershell(script);
    return strdup(out_path ? out_path : "/tmp/clip.png");
}

/* PoP: _find_powershell @ hermes_cli/clipboard.py:_find_powershell */
const char *clipboard_find_powershell(void) {
    const char *candidates[] = {"/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe", "powershell.exe"};
    for (int i = 0; i < 2; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return "powershell.exe";
}

/* PoP: _get_ps_exe @ hermes_cli/clipboard.py:_get_ps_exe */
const char *clipboard_get_ps_exe(void) { return clipboard_find_powershell(); }

/* PoP: _windows_has_image @ hermes_cli/clipboard.py:_windows_has_image */
bool clipboard_windows_has_image(void) { return clipboard_powershell_has_image(); }

/* PoP: _windows_save @ hermes_cli/clipboard.py:_windows_save */
char *clipboard_windows_save(const char *out_path) { return clipboard_powershell_save_image(out_path); }

/* ── Linux ───────────────────────────────────────────────────── */
/* PoP: _linux_save @ hermes_cli/clipboard.py:_linux_save */
int clipboard_linux_save(const char *text) {
    if (!text) return -1;
    FILE *fp = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (!fp) return -1;
    fputs(text, fp);
    return pclose(fp);
}

/* PoP: _wsl_has_image @ hermes_cli/clipboard.py:_wsl_has_image */
bool clipboard_wsl_has_image(void) { return clipboard_powershell_has_image(); }

/* PoP: _wsl_save @ hermes_cli/clipboard.py:_wsl_save */
char *clipboard_wsl_save(const char *out_path) { return clipboard_powershell_save_image(out_path); }

/* PoP: _wayland_has_image @ hermes_cli/clipboard.py:_wayland_has_image */
bool clipboard_wayland_has_image(void) {
    char *r = run_cmd_capture("wl-paste --list-types 2>/dev/null | grep -q 'image/png'");
    return r != NULL;
}

/* PoP: _wayland_save @ hermes_cli/clipboard.py:_wayland_save */
char *clipboard_wayland_save(const char *out_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wl-paste --type image/png > %s 2>/dev/null", out_path ? out_path : "/tmp/clip.png");
    run_cmd_silent(cmd);
    return strdup(out_path ? out_path : "/tmp/clip.png");
}

/* PoP: _convert_to_png @ hermes_cli/clipboard.py:_convert_to_png */
bool clipboard_convert_to_png(const char *in_path, const char *out_path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "convert %s %s 2>/dev/null", in_path, out_path);
    return run_cmd_silent(cmd) == 0;
}

/* PoP: _is_png_file @ hermes_cli/clipboard.py:_is_png_file */
bool clipboard_is_png_file(const char *path) {
    if (!path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char header[8];
    size_t rd = fread(header, 1, 8, f);
    fclose(f);
    if (rd < 8) return false;
    return header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47;
}

/* PoP: _xclip_has_image @ hermes_cli/clipboard.py:_xclip_has_image */
bool clipboard_xclip_has_image(void) {
    char *r = run_cmd_capture("xclip -selection clipboard -t TARGETS -o 2>/dev/null | grep -q 'image/png'");
    return r != NULL;
}

/* PoP: _xclip_save @ hermes_cli/clipboard.py:_xclip_save */
char *clipboard_xclip_save(const char *out_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xclip -selection clipboard -t image/png -o > %s 2>/dev/null", out_path ? out_path : "/tmp/clip.png");
    run_cmd_silent(cmd);
    return strdup(out_path ? out_path : "/tmp/clip.png");
}
