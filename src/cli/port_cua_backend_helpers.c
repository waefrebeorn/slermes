/*
 * port_cua_backend_helpers.c
 *
 * Pure, portable helpers ported from tools/computer_use/cua_backend.py.
 * These are the string/byte parsing helpers that do NOT touch the display
 * server, subprocess, or asyncio loop:
 *   - _image_dimensions_from_bytes  (PNG/JPEG dimension sniffing, byte-only)
 *   - _split_tree_text              (split AX-tree markdown into summary+tree)
 *   - _parse_key_combo              (parse "cmd+s" -> (key, modifiers))
 *
 * The IO/process-coupled drivers (_resolve_mcp_invocation, cua_driver_*,
 * the asyncio _AsyncBridge, the MCP _CuaDriverSession) are NOT ported — they
 * require subprocess/asyncio/the live cua-driver binary.
 *
 * Module prefix used by the scanner for tools/computer_use/cua_backend.py is
 * "cua_backend_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- PNG/JPEG dimension sniffing -------------------------------------- */
/* PoP: _image_dimensions_from_bytes @ tools/computer_use/cua_backend.py:_image_dimensions_from_bytes */
/* Returns width,height via out params, or (0,0) if it can't be determined.
 * Mirrors Python: PNG (IHDR at offset 16), JPEG SOF0..SOF15 markers. */
void cua_backend_image_dimensions_from_bytes(const unsigned char *raw, size_t n,
                                             int *out_w, int *out_h)
{
    *out_w = 0;
    *out_h = 0;
    if (!raw || n < 2) return;

    /* PNG: magic \x89PNG\r\n\x1a\n, IHDR width/height big-endian at 16..24 */
    static const unsigned char png_magic[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (n >= 24 && memcmp(raw, png_magic, 8) == 0) {
        int w = ((int)raw[16] << 24) | ((int)raw[17] << 16) | ((int)raw[18] << 8) | (int)raw[19];
        int h = ((int)raw[20] << 24) | ((int)raw[21] << 16) | ((int)raw[22] << 8) | (int)raw[23];
        if (w > 0 && h > 0) { *out_w = w; *out_h = h; }
        return;
    }

    /* JPEG: starts with FFD8. Walk markers. */
    if (n >= 2 && raw[0] == 0xFF && raw[1] == 0xD8) {
        size_t i = 2;
        while (i + 9 < n) {
            if (raw[i] != 0xFF) { i++; continue; }
            unsigned char marker = raw[i + 1];
            i += 2;
            /* standalone markers: no length */
            if (marker == 0xD8 || marker == 0xD9 ||
                (marker >= 0xD0 && marker <= 0xD7)) {
                continue;
            }
            if (i + 2 > n) break;
            int seg_len = ((int)raw[i] << 8) | (int)raw[i + 1];
            if (seg_len < 2 || i + (size_t)seg_len > n) break;
            switch (marker) {
                case 0xC0: case 0xC1: case 0xC2: case 0xC3:
                case 0xC5: case 0xC6: case 0xC7:
                case 0xC9: case 0xCA: case 0xCB:
                case 0xCD: case 0xCE: case 0xCF:
                    if (seg_len >= 7) {
                        int h = ((int)raw[i + 3] << 8) | (int)raw[i + 4];
                        int w = ((int)raw[i + 5] << 8) | (int)raw[i + 6];
                        if (w > 0 && h > 0) { *out_w = w; *out_h = h; }
                    }
                    return; /* SOF reached */
                default:
                    break;
            }
            i += (size_t)seg_len;
        }
    }
}

/* --- AX-tree text split ----------------------------------------------- */
/* PoP: _split_tree_text @ tools/computer_use/cua_backend.py:_split_tree_text */
/* Splits get_window_state text into (summary_line, tree_markdown).
 * Allocates *out_summary and *out_tree with malloc; caller frees. On
 * no-newline input, summary = whole string, tree = "". Returns 0 on success. */
int cua_backend_split_tree_text(const char *full_text,
                                char **out_summary, char **out_tree)
{
    if (!full_text) { *out_summary = strdup(""); *out_tree = strdup(""); return 0; }
    const char *nl = strchr(full_text, '\n');
    if (!nl) {
        *out_summary = strdup(full_text);
        *out_tree = strdup("");
        return 0;
    }
    size_t first_len = (size_t)(nl - full_text);
    *out_summary = malloc(first_len + 1);
    memcpy(*out_summary, full_text, first_len);
    (*out_summary)[first_len] = '\0';
    const char *rest = nl + 1;
    *out_tree = strdup(rest); /* strdup handles "" */
    return 0;
}

/* --- key combo parsing ------------------------------------------------- */
/* PoP: _parse_key_combo @ tools/computer_use/cua_backend.py:_parse_key_combo */
/*
 * Parse a key string like 'cmd+s' into (key, modifiers).
 * Returns key (malloc'd, may be NULL) and fills modifiers[] (malloc'd array of
 * malloc'd strings, NULL-terminated). Caller frees: the key string, each
 * modifier string, and the modifiers array itself.
 *
 * Mirrors Python: split on + or -, strip+lowercase, map aliases
 * (command->cmd, alt->option, control->ctrl); modifier set
 * {cmd, command, shift, option, alt, ctrl, control, fn}. Last non-modifier
 * wins. If no non-modifier present, key is NULL and all tokens are modifiers.
 */
char *cua_backend_parse_key_combo(const char *keys, char ***out_modifiers)
{
    /* modifier set */
    static const char *MODIFIERS[] = {
        "cmd", "command", "shift", "option", "alt", "ctrl", "control", "fn", NULL
    };
    /* alias map */
    static const char *ALIAS_FROM[] = {"command", "alt", "control"};
    static const char *ALIAS_TO[]   = {"cmd",     "option", "ctrl"};
    #define CUA_MAX_MODS 16

    char **mods = malloc(sizeof(char *) * (CUA_MAX_MODS + 1));
    int mod_count = 0;
    char *key = NULL;

    if (!keys || !*keys) {
        mods[0] = NULL;
        *out_modifiers = mods;
        return NULL;
    }

    /* tokenize on + or - */
    size_t L = strlen(keys);
    char *buf = strdup(keys);
    size_t start = 0;
    for (size_t i = 0; i <= L; i++) {
        if (i == L || buf[i] == '+' || buf[i] == '-') {
            if (i > start) {
                /* extract token [start, i) */
                buf[i] = '\0';
                char *tok = buf + start;
                /* strip */
                while (*tok && (*tok == ' ' || *tok == '\t')) tok++;
                size_t tlen = strlen(tok);
                while (tlen > 0 && (tok[tlen-1] == ' ' || tok[tlen-1] == '\t')) {
                    tok[tlen-1] = '\0'; tlen--;
                }
                /* lowercase in place */
                for (size_t k = 0; k < tlen; k++) {
                    char c = tok[k];
                    if (c >= 'A' && c <= 'Z') tok[k] = (char)(c - 'A' + 'a');
                }
                if (tlen > 0) {
                    /* alias resolution */
                    const char *norm = tok;
                    for (int a = 0; a < 3; a++) {
                        if (strcmp(tok, ALIAS_FROM[a]) == 0) { norm = ALIAS_TO[a]; break; }
                    }
                    /* modifier test */
                    int is_mod = 0;
                    for (int m = 0; MODIFIERS[m]; m++) {
                        if (strcmp(norm, MODIFIERS[m]) == 0) { is_mod = 1; break; }
                    }
                    if (is_mod) {
                        if (mod_count < CUA_MAX_MODS) {
                            mods[mod_count++] = strdup(norm);
                        }
                    } else {
                        free(key);
                        key = strdup(norm); /* last non-modifier wins */
                    }
                }
            }
            start = i + 1;
        }
    }
    free(buf);
    mods[mod_count] = NULL;
    *out_modifiers = mods;
    return key;
}
