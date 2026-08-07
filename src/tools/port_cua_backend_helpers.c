/*
 * port_cua_backend_helpers.c — pure, self-contained helpers ported from
 * tools/computer_use/cua_backend.py. These are the IO-free parsing/formatting
 * helpers; they do NOT touch the driver session, subprocess, or asyncio loop:
 *   - _image_dimensions_from_bytes -> cua_image_dimensions_from_bytes
 *   - _split_tree_text             -> cua_split_tree_text
 *   - _parse_key_combo             -> cua_parse_key_combo
 * No god header; opaque, minimal includes.
 */

#include "cua_backend_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Best-effort PNG/JPEG dimension sniffing (mirrors _image_dimensions_from_bytes).
 * Sets *out_w/*out_h; returns 1 if a dimension was found, else 0 with 0,0. */
int cua_image_dimensions_from_bytes(const unsigned char *raw, size_t len,
                                    int *out_w, int *out_h)
{
    *out_w = 0; *out_h = 0;

    /* PNG: signature then IHDR width/height at fixed offsets. */
    static const unsigned char PNG_SIG[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    if (len >= 24 && memcmp(raw, PNG_SIG, 8) == 0) {
        int w = (int)((raw[16] << 24) | (raw[17] << 16) | (raw[18] << 8) | raw[19]);
        int h = (int)((raw[20] << 24) | (raw[21] << 16) | (raw[22] << 8) | raw[23]);
        if (w > 0 && h > 0) { *out_w = w; *out_h = h; return 1; }
        return 0;
    }

    /* JPEG: walk markers for a Start-Of-Frame (SOF0/SOF1/... excluding
     * SOF2 progressive and the differential/extended variants the driver
     * doesn't emit). */
    if (len >= 3 && raw[0] == 0xFF && raw[1] == 0xD8) {
        size_t i = 2;
        while (i + 9 < len) {
            if (raw[i] != 0xFF) { i++; continue; }
            unsigned char marker = raw[i + 1];
            i += 2;
            if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7))
                continue;
            if (i + 2 > len) break;
            size_t seg = ((size_t)raw[i] << 8) | raw[i + 1];
            if (seg < 2 || i + seg > len) break;
            switch (marker) {
                case 0xC0: case 0xC1: case 0xC2: case 0xC3:
                case 0xC5: case 0xC6: case 0xC7: case 0xC9:
                case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
                    if (seg >= 7) {
                        int h = (int)((raw[i + 3] << 8) | raw[i + 4]);
                        int w = (int)((raw[i + 5] << 8) | raw[i + 6]);
                        if (w > 0 && h > 0) { *out_w = w; *out_h = h; return 1; }
                    }
                    return 0;
                default:
                    break;
            }
            i += seg;
        }
    }
    return 0;
}

/* Split get_window_state text into (summary_line, tree_markdown).
 * summary is malloc'd; tree is malloc'd ("" when absent). Caller frees both. */
void cua_split_tree_text(const char *full_text, char **out_summary, char **out_tree)
{
    if (!full_text) full_text = "";
    const char *nl = strchr(full_text, '\n');
    if (!nl) {
        *out_summary = strdup(full_text);
        *out_tree = strdup("");
        return;
    }
    size_t slen = (size_t)(nl - full_text);
    char *sum = malloc(slen + 1);
    memcpy(sum, full_text, slen);
    sum[slen] = '\0';
    const char *tree = nl + 1;
    *out_summary = sum;
    *out_tree = strdup(tree);
}

/* Parse a key string like 'cmd+s' into (key, modifiers[]). key is malloc'd
 * (NULL when none); modifiers is malloc'd array of malloc'd strings, count in
 * *out_nmods. Caller frees key, each modifier, and the array. */
static const char *cua_alias_of(const char *p)
{
    if (strcmp(p, "command") == 0) return "cmd";
    if (strcmp(p, "alt") == 0) return "option";
    if (strcmp(p, "control") == 0) return "ctrl";
    return p;
}

char *cua_parse_key_combo(const char *keys, char ***out_modifiers, int *out_nmods)
{
    static const char *MODIFIER_NAMES[] = {
        "cmd", "command", "shift", "option", "alt", "ctrl", "control", "fn", NULL
    };
    char **mods = NULL;
    int nmods = 0, mcap = 0;
    char *key = NULL;

    /* split on + or - (one or more), trim whitespace, lowercase */
    size_t n = keys ? strlen(keys) : 0;
    char *buf = malloc(n + 1);
    if (keys) memcpy(buf, keys, n);
    buf[n] = '\0';
    /* tokenize */
    char *p = buf;
    while (*p) {
        while (*p == '+' || *p == '-' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != '+' && *p != '-' && *p != ' ' && *p != '\t') p++;
        char saved = *p; *p = '\0';
        /* lowercase in place */
        for (char *q = start; *q; q++)
            if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        char *norm = strdup(cua_alias_of(start));
        int is_mod = 0;
        for (int k = 0; MODIFIER_NAMES[k]; k++)
            if (strcmp(norm, MODIFIER_NAMES[k]) == 0) { is_mod = 1; break; }
        if (is_mod) {
            if (nmods + 1 > mcap) { mcap = mcap ? mcap * 2 : 4; mods = realloc(mods, mcap * sizeof(char *)); }
            mods[nmods++] = norm;
        } else {
            free(key);
            key = norm;  /* last non-modifier wins */
        }
        *p = saved;
    }
    free(buf);

    *out_modifiers = mods;
    *out_nmods = nmods;
    return key;
}

/* PoP: _wsl_windows_path_to_posix @ tools/computer_use/cua_backend.py:_wsl_windows_path_to_posix */
/* Port of Python:
 *   if not re.match(r"^[A-Za-z]:[\\/]", path): return path
 *   from hermes_constants import is_wsl
 *   if not is_wsl(): return path
 *   win = PureWindowsPath(path)
 *   drive = (win.drive or "").rstrip(":").lower()
 *   if not drive: return path
 *   return os.path.join("/mnt", drive, *(str(part) for part in win.parts[1:]))
 * The is_wsl check is the caller's responsibility; we just do the transform. */
char *cua_wsl_windows_path_to_posix(const char *path, int is_wsl) {
    if (!path || !*path) return strdup(path ? path : "");
    /* Check for Windows drive letter: ^[A-Za-z]:[\\/] */
    if (!((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        return strdup(path);
    if (path[1] != ':') return strdup(path);
    if (path[2] != '\\' && path[2] != '/') return strdup(path);
    if (!is_wsl) return strdup(path);

    /* drive letter, lowercased */
    char drive[4] = {0};
    drive[0] = (path[0] >= 'A' && path[0] <= 'Z') ? path[0] + 32 : path[0];

    /* Split remaining path into parts (after drive + separator) */
    const char *rest = path + 3;  /* skip X:\ or X:/ */
    /* Collect non-empty segments split by \ or / */
    char *parts[128];
    int nparts = 0;
    char buf[4096];
    size_t blen = strlen(rest);
    if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
    memcpy(buf, rest, blen);
    buf[blen] = '\0';

    char *tok = buf;
    char *saveptr = NULL;
    char *sep = "/\\";
    char *p = strtok_r(tok, sep, &saveptr);
    while (p && nparts < 127) {
        parts[nparts++] = strdup(p);
        p = strtok_r(NULL, sep, &saveptr);
    }

    /* Build /mnt/<drive>/<parts...> */
    size_t total = 6 + 1 + 2 + 1;  /* "/mnt/" + drive + "/" + trailing null */
    for (int i = 0; i < nparts; i++) total += strlen(parts[i]) + 1;
    char *result = (char *)malloc(total);
    if (!result) {
        for (int i = 0; i < nparts; i++) free(parts[i]);
        return strdup(path);
    }
    char *pos = result;
    *pos++ = '/'; *pos++ = 'm'; *pos++ = 'n'; *pos++ = 't'; *pos++ = '/';
    *pos++ = drive[0]; *pos++ = '/';
    for (int i = 0; i < nparts; i++) {
        size_t slen = strlen(parts[i]);
        memcpy(pos, parts[i], slen);
        pos += slen;
        *pos++ = '/';
    }
    /* Remove trailing slash if we added segments */
    if (nparts > 0 && pos > result + 7 && *(pos - 1) == '/') pos--;
    *pos = '\0';

    for (int i = 0; i < nparts; i++) free(parts[i]);
    return result;
}
