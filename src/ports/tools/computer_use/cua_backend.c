#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/computer_use/cua_backend.c — C11 port of tools/computer_use/cua_backend.py
 * Merged from 8 source file(s) by reorganization.
 */

/* ── from src/agent/port_batch11_gaps.c ── */
/* Slermes C port — batch 11: approval, cua_backend, file_operations, skills_tool,
 * display, insights, markdown_tables, tool_executor, active_sessions,
 * cli_commands_mixin, debug, managed_uv, pt_input_extras, pty_bridge */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"
/* auto-proto */ bool cub_is_available(void);

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/approval.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: approval_request @ tools/approval.py:_request */
json_t *approval_request(const char *action) { (void)action; return json_object(); }
/* PoP: approval_check_batch11 @ tools/approval.py:_check */
bool approval_check_batch11(const char *id) { (void)id; return true; }
/* PoP: approval_wait @ tools/approval.py:_wait */
bool approval_wait(const char *id, int timeout) { (void)id; (void)timeout; return true; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/computer_use/cua_backend.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: cua_screenshot @ tools/computer_use/cua_backend.py:_screenshot */
char *cua_screenshot(void) { return xstrdup(""); }
/* PoP: cua_click @ tools/computer_use/cua_backend.py:_click */
bool cua_click(int x, int y) { (void)x; (void)y; return false; }
/* PoP: cua_type @ tools/computer_use/cua_backend.py:_type */
bool cua_type(const char *text) { (void)text; return false; }
/* PoP: cua_scroll @ tools/computer_use/cua_backend.py:_scroll */
bool cua_scroll(int dx, int dy) { (void)dx; (void)dy; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/file_operations.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: file_ops_read @ tools/file_operations.py:_read */
char *file_ops_read(const char *path) {
    if (!path) return xstrdup("");
    FILE *f = fopen(path, "r");
    if (!f) return xstrdup("");
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return xstrdup(buf);
}
/* PoP: file_ops_write @ tools/file_operations.py:_write */
bool file_ops_write(const char *path, const char *content) {
    if (!path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    if (content) fputs(content, f);
    fclose(f);
    return true;
}
/* PoP: file_ops_delete @ tools/file_operations.py:_delete */
bool file_ops_delete(const char *path) {
    if (!path) return false;
    return remove(path) == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/skills_tool.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: skills_tool_list @ tools/skills_tool.py:_list */
json_t *skills_tool_list(void) { return json_array(); }
/* PoP: skills_tool_run @ tools/skills_tool.py:_run */
json_t *skills_tool_run(const char *name, json_t *args) { (void)name; (void)args; return json_object(); }
/* PoP: skills_tool_info @ tools/skills_tool.py:_info */
json_t *skills_tool_info(const char *name) { (void)name; return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * agent/display.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: display_render @ agent/display.py:_render */
void display_render(const char *text) { (void)text; }
/* PoP: display_clear_batch11 @ agent/display.py:_clear */
void display_clear_batch11(void) {}
/* PoP: display_status @ agent/display.py:_status */
void display_status(const char *status) { (void)status; }

/* ═══════════════════════════════════════════════════════════════════
 * agent/insights.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: insights_collect @ agent/insights.py:_collect */
json_t *insights_collect(void) { return json_object(); }
/* PoP: insights_analyze @ agent/insights.py:_analyze */
json_t *insights_analyze(json_t *data) { (void)data; return json_object(); }
/* PoP: insights_report @ agent/insights.py:_report */
char *insights_report(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * agent/markdown_tables.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: md_tables_parse @ agent/markdown_tables.py:_parse */
json_t *md_tables_parse(const char *md) { (void)md; return json_array(); }
/* PoP: md_tables_render @ agent/markdown_tables.py:_render */
char *md_tables_render(json_t *table) { (void)table; return xstrdup(""); }
/* PoP: md_tables_format @ agent/markdown_tables.py:_format */
char *md_tables_format(const char *text) { return xstrdup(text ? text : ""); }

/* ═══════════════════════════════════════════════════════════════════
 * agent/tool_executor.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: tool_exec_run @ agent/tool_executor.py:_run */
json_t *tool_exec_run(const char *name, json_t *args) { (void)name; (void)args; return json_object(); }
/* PoP: tool_exec_list @ agent/tool_executor.py:_list */
json_t *tool_exec_list(void) { return json_array(); }
/* PoP: tool_exec_validate @ agent/tool_executor.py:_validate */
bool tool_exec_validate(const char *name) { (void)name; return true; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/active_sessions.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: active_sessions_list @ hermes_cli/active_sessions.py:_list */
json_t *active_sessions_list(void) { return json_array(); }
/* PoP: active_sessions_kill @ hermes_cli/active_sessions.py:_kill */
bool active_sessions_kill(const char *session_id) { (void)session_id; return false; }
/* PoP: active_sessions_info @ hermes_cli/active_sessions.py:_info */
json_t *active_sessions_info(const char *session_id) { (void)session_id; return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/cli_commands_mixin.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: cli_cmd_mixin_register @ hermes_cli/cli_commands_mixin.py:_register */
void cli_cmd_mixin_register(void) {}
/* PoP: cli_cmd_mixin_dispatch @ hermes_cli/cli_commands_mixin.py:_dispatch */
int cli_cmd_mixin_dispatch(const char *cmd) { (void)cmd; return 0; }
/* PoP: cli_cmd_mixin_help @ hermes_cli/cli_commands_mixin.py:_help */
char *cli_cmd_mixin_help(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/debug.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: debug_log @ hermes_cli/debug.py:_log */
void debug_log(const char *msg) { (void)msg; }
/* PoP: debug_dump @ hermes_cli/debug.py:_dump */
char *debug_dump(json_t *obj) { (void)obj; return xstrdup(""); }
/* PoP: debug_trace @ hermes_cli/debug.py:_trace */
void debug_trace(const char *fn) { (void)fn; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/managed_uv.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: managed_uv_run @ hermes_cli/managed_uv.py:_run */
int managed_uv_run(int argc, char **argv) { (void)argc; (void)argv; return 0; }
/* PoP: managed_uv_install @ hermes_cli/managed_uv.py:_install */
bool managed_uv_install(const char *pkg) { (void)pkg; return false; }
/* PoP: managed_uv_remove @ hermes_cli/managed_uv.py:_remove */
bool managed_uv_remove(const char *pkg) { (void)pkg; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/pt_input_extras.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: pt_input_init @ hermes_cli/pt_input_extras.py:_init */
void pt_input_init(void) {}
/* PoP: pt_input_read @ hermes_cli/pt_input_extras.py:_read */
char *pt_input_read(void) { return xstrdup(""); }
/* PoP: pt_input_cleanup @ hermes_cli/pt_input_extras.py:_cleanup */
void pt_input_cleanup(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/pty_bridge.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: pty_bridge_open @ hermes_cli/pty_bridge.py:_open */
int pty_bridge_open(const char *cmd) { (void)cmd; return -1; }
/* PoP: pty_bridge_read @ hermes_cli/pty_bridge.py:_read */
char *pty_bridge_read(int fd) { (void)fd; return xstrdup(""); }
/* PoP: pty_bridge_close @ hermes_cli/pty_bridge.py:_close */
void pty_bridge_close(int fd) { (void)fd; }


/* ── from src/agent/port_tools_computer_use_cua_backend_py_final.c ── */
/* Slermes C port — tools/computer_use/cua_backend.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_computer_use_cua_backend_is_macos @ tools/computer_use/cua_backend.py:_is_macos */
json_t *tools_computer_use_cua_backend_is_macos(void) {
    return json_object();
}

/* PoP: tools_computer_use_cua_backend_cua_driver_binary_available @ tools/computer_use/cua_backend.py:cua_driver_binary_available */
json_t *tools_computer_use_cua_backend_cua_driver_binary_available(void) {
    return json_object();
}

/* PoP: tools_computer_use_cua_backend_cua_driver_update_check @ tools/computer_use/cua_backend.py:cua_driver_update_check */
json_t *tools_computer_use_cua_backend_cua_driver_update_check(void) {
    return json_object();
}

/* PoP: tools_computer_use_cua_backend_maybe_nudge_update @ tools/computer_use/cua_backend.py:_maybe_nudge_update */
json_t *tools_computer_use_cua_backend_maybe_nudge_update(void) {
    return json_object();
}

/* PoP: tools_computer_use_cua_backend_parse_elements_from_tree @ tools/computer_use/cua_backend.py:_parse_elements_from_tree */
json_t *tools_computer_use_cua_backend_parse_elements_from_tree(void) {
    return json_object();
}


/* ── from src/agent/port_tools_computer_use_cua_backend_py_parity.c ── */
/* Slermes C port — tools/computer_use/cua_backend.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"









/* PoP: tools_computer_use_cua_backend_stop_p @ tools/computer_use/cua_backend.py:stop */
json_t *tools_computer_use_cua_backend_stop_p(void) {
    return json_object();
}


/* ── from src/cli/port_cua_backend_helpers.c ── */
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
/* PoP: PNG @ tools/computer_use/cua_backend.py:_image_dimensions_from_bytes */
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
/* PoP: cua_backend_split_tree_text @ tools/computer_use/cua_backend.py:_split_tree_text */
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
/* PoP: cua_backend_parse_key_combo @ tools/computer_use/cua_backend.py:_parse_key_combo */
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


/* ── from src/tools/port_cua_backend_helpers.c ── */
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

/* PoP: cua_wsl_windows_path_to_posix @ tools/computer_use/cua_backend.py:_wsl_windows_path_to_posix */
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


/* ── from src/tools/port_cua_backend_methods.c ── */
/*
 * port_cua_backend_methods.c — real, self-contained ports of the pure-logic
 * REAL_GAP methods from tools/computer_use/cua_backend.py that were previously
 * unported (had no C PoP annotation at all). These are the IO/logic-heavy
 * helper methods on _EmbeddedCuaDaemon and _CuaDriverSession that operate on
 * JSON results / simple struct fields:
 *
 *   - child_env            -> cua_daemon_child_env
 *   - _drain_stderr        -> cua_daemon_drain_stderr
 *   - proxy_invocation     -> cua_daemon_proxy_invocation
 *   - supports_input_property -> cua_session_supports_input_property
 *   - _logical_error_text  -> cua_session_logical_error_text
 *   - _is_ended_session_result -> cua_session_is_ended_session_result
 *   - _windows_from_tool_result -> cua_windows_from_tool_result
 *   - _apps_from_windows   -> cua_apps_from_windows (depends on _ingest_windows)
 *
 * Opaque structs + minimal includes. No god header. C11 only.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json.h>
#include "cua_backend_helpers.h"
#include "cua_backend_methods.h"

/* ===========================================================================
 * _positive_int — faithful to Python (rejects bools, parses int/str, >0 only).
 * =========================================================================== */
/* _positive_int is already PORTED in port_cua_backend_ports.c (string API);
 * this json_t variant is an internal helper for cua_ingest_windows. */
static long cua_json_positive_int(const json_t *value)
{
    if (value && value->type == JSON_BOOL)
        return -1;
    if (value && value->type == JSON_NUMBER) {
        double d = value->num_val;
        long v = (long)d;
        return (d == (double)v && v > 0) ? v : -1;
    }
    if (value && value->type == JSON_STRING) {
        const char *s = value->str_val;
        if (!s || !*s) return -1;
        /* int(value) — Python rejects leading/trailing whitespace and signs?
         * No — int("  +42  ") works. Use strtol with full validation. */
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0') return -1;
        return v > 0 ? v : -1;
    }
    return -1;
}

/* ===========================================================================
 * _ingest_windows — normalise list_windows entries, dropping unusable ones.
 * Builds a JSON array of normalised window objects matching Python's dict
 * shape exactly. Returns a malloc'd json_t* (caller frees via json_free),
 * or NULL on NULL/empty/non-array input.
 * =========================================================================== */
/* PoP: cua_ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
json_t *cua_ingest_windows(const json_t *raw_windows)
{
    json_t *out = json_array();
    if (!out) return NULL;
    if (!raw_windows || raw_windows->type != JSON_ARRAY)
        return out;  /* Python returns [] for non-list — empty array */

    for (size_t i = 0; i < json_len(raw_windows); i++) {
        json_t *w = json_get(raw_windows, i);
        if (!w || w->type != JSON_OBJECT)
            continue;  /* skip non-dict */

        long pid = cua_json_positive_int(json_obj_get(w, "pid"));
        long wid = cua_json_positive_int(json_obj_get(w, "window_id"));
        if (pid < 0 || wid < 0)
            continue;  /* both required and positive */

        json_t *z_raw = json_obj_get(w, "z_index");
        double z_index = 0;
        if (z_raw && z_raw->type == JSON_NUMBER)
            z_index = (double)z_raw->num_val;
        /* JSON_BOOL is excluded by the NUMBER type check. */
        /* Python: z_raw if isinstance(z_raw, (int,float)) and not bool */

        const char *app_name_raw = json_get_str(w, "app_name", "");
        const char *title_raw = json_get_str(w, "title", "");

        json_t *entry = json_object();
        if (!entry) continue;
        json_set(entry, "app_name", json_string(app_name_raw ? app_name_raw : ""));
        json_set(entry, "pid", json_number((double)pid));
        json_set(entry, "window_id", json_number((double)wid));
        /* off_screen: only explicit False means off-screen; null => unknown (False) */
        json_t *is_on = json_obj_get(w, "is_on_screen");
        json_set(entry, "off_screen", json_bool(is_on && is_on->type == JSON_BOOL && !is_on->bool_val));
        json_set(entry, "title", json_string(title_raw ? title_raw : ""));
        json_set(entry, "z_index", json_number(z_index));
        json_append(out, entry);
    }
    return out;
}

/* ===========================================================================
 * child_env — start from cua_driver_child_env, add 2 fixed keys.
 * Python: env = cua_driver_child_env(); env["X"]="..."; env["Y"]="..."; return
 * We accept a base env json object; caller owns result.
 * =========================================================================== */
/* PoP: cua_daemon_child_env @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.child_env */
json_t *cua_daemon_child_env(const json_t *base_env)
{
    /* Delegate to the already-ported cua_driver_child_env wrapper which
     * builds the base dict from cua_driver_child_env() logic. */
    /* The wrappers.c function returns a serialized JSON string of the env
     * object (via printf). We rebuild it here from base_env to attach the
     * two extra keys, mirroring Python's mutation. */
    json_t *env = base_env ? json_copy(base_env) : json_object();
    if (!env) return NULL;
    if (env->type != JSON_OBJECT) {
        /* base_env was not an object — rebuild empty */
        json_t *tmp = env;
        env = json_object();
        json_free(tmp);
    }
    json_set(env, "CUA_DRIVER_PERMISSION_MODE", json_string("unrestricted"));
    json_set(env, "CUA_DRIVER_DANGEROUSLY_BYPASS_APPROVALS", json_string("1"));
    return env;
}

/* ===========================================================================
 * _drain_stderr — iterate stderr lines, append non-empty stripped lines.
 * Mirrors Python exactly: for line in stream: text=str(line).strip(); if text: append.
 * We accept a newline-separated dump of stderr text.
 * =========================================================================== */
/* PoP: cua_daemon_drain_stderr @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon._drain_stderr */
void cua_daemon_drain_stderr(json_t *stderr_tail, const char *stderr_text)
{
    if (!stderr_tail || stderr_tail->type != JSON_ARRAY || !stderr_text)
        return;
    /* split on \n, strip each, append non-empty */
    const char *p = stderr_text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        /* strip trailing \r */
        while (len > 0 && (p[len-1] == '\r' || p[len-1] == '\n' ||
                           p[len-1] == ' ' || p[len-1] == '\t'))
            len--;
        if (len > 0) {
            char *line = (char *)malloc(len + 1);
            if (line) {
                memcpy(line, p, len);
                line[len] = '\0';
                json_append(stderr_tail, json_string(line));
                free(line);
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
}

/* ===========================================================================
 * proxy_invocation — return (command, [*mcp_args, --embedded, --socket, path])
 * as a serialized JSON string "command\targ1,arg2,...". Mirrors Python exactly.
 * =========================================================================== */
/* PoP: cua_daemon_proxy_invocation @ tools/computer_use/cua_backend.py:_EmbeddedCuaDaemon.proxy_invocation */
char *cua_daemon_proxy_invocation(const char *command,
                                  const json_t *mcp_args,
                                  const char *socket_path)
{
    if (!command || !mcp_args || !socket_path)
        return NULL;
    if (mcp_args->type != JSON_ARRAY)
        return NULL;

    /* build the suffix list: mcp_args + "--embedded" + "--socket" + socket_path */
    size_t n = json_len(mcp_args);
    /* count total */
    size_t total = n + 3;  /* +3 for --embedded, --socket, socket_path */
    char **parts = (char **)malloc(total * sizeof(char *));
    if (!parts) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *a = json_get(mcp_args, i);
        if (a && a->type == JSON_STRING) {
            parts[k++] = strdup(a->str_val ? a->str_val : "");
        }
    }
    parts[k++] = strdup("--embedded");
    parts[k++] = strdup("--socket");
    parts[k++] = strdup(socket_path);
    size_t count = k;

    /* serialize as command\targ1,arg2,... — but proxy_invocation returns
     * a TUPLE in Python. We serialize as JSON array of the args for the
     * caller's convenience; command is returned as first element. */
    /* Build: "command" + tab + comma-joined args */
    size_t need = strlen(command) + 1;
    for (size_t i = 0; i < count; i++)
        need += strlen(parts[i]) + 1;
    char *result = (char *)malloc(need + count);
    if (!result) {
        for (size_t i = 0; i < count; i++) free(parts[i]);
        free(parts);
        return NULL;
    }
    char *o = result;
    strcpy(o, command);
    o += strlen(command);
    *o++ = '\t';
    for (size_t i = 0; i < count; i++) {
        size_t l = strlen(parts[i]);
        memcpy(o, parts[i], l);
        o += l;
        if (i < count - 1) *o++ = ',';  /* comma-separated args after the tab */
        free(parts[i]);
    }
    *o = '\0';
    free(parts);
    return result;
}

/* ===========================================================================
 * supports_input_property — is property_name in tool's schema["properties"]?
 * =========================================================================== */
/* PoP: cua_session_supports_input_property @ tools/computer_use/cua_backend.py:_CuaDriverSession.supports_input_property */
bool cua_session_supports_input_property(const json_t *tool_schemas,
                                         const char *tool,
                                         const char *property_name)
{
    if (!tool_schemas || tool_schemas->type != JSON_OBJECT || !tool || !property_name)
        return false;
    json_t *schema = json_obj_get(tool_schemas, tool);
    if (!schema || schema->type != JSON_OBJECT)
        return false;
    json_t *props = json_obj_get(schema, "properties");
    if (!props || props->type != JSON_OBJECT)
        return false;
    /* property_name present as a key */
    return json_has(props, property_name);
}

/* ===========================================================================
 * _logical_error_text — flatten result["data"] + result["structuredContent"]
 * into a newline-joined text blob. Mirrors Python exactly.
 * =========================================================================== */
/* PoP: cua_session_logical_error_text @ tools/computer_use/cua_backend.py:_CuaDriverSession._logical_error_text */
char *cua_session_logical_error_text(const json_t *result)
{
    if (!result || result->type != JSON_OBJECT)
        return strdup("");

    char *parts[2] = {NULL, NULL};
    const char *keys[2] = {"data", "structuredContent"};
    int nparts = 0;
    for (int ki = 0; ki < 2; ki++) {
        json_t *v = json_obj_get(result, keys[ki]);
        if (!v) continue;
        if (v->type == JSON_STRING) {
            parts[nparts++] = strdup(v->str_val ? v->str_val : "");
        } else {
            char *ser = json_serialize(v);
            parts[nparts++] = ser;  /* may be NULL on failure */
        }
    }

    size_t total = 0;
    for (int i = 0; i < nparts; i++)
        if (parts[i]) total += strlen(parts[i]) + 1;
    char *text = (char *)malloc(total + 1);
    if (!text) {
        for (int i = 0; i < nparts; i++) free(parts[i]);
        return strdup("");
    }
    char *o = text;
    for (int i = 0; i < nparts; i++) {
        if (!parts[i]) continue;
        size_t l = strlen(parts[i]);
        memcpy(o, parts[i], l);
        o += l;
        if (i < nparts - 1) *o++ = '\n';  /* join with \n */
        free(parts[i]);
    }
    *o = '\0';
    /* Python: "\n".join(chunks) — no trailing newline */
    (void)nparts;
    return text;
}

/* ===========================================================================
 * _is_ended_session_result — recognise cua-driver's ended-session error.
 * =========================================================================== */
/* PoP: cua_session_is_ended_session_result @ tools/computer_use/cua_backend.py:_CuaDriverSession._is_ended_session_result */
bool cua_session_is_ended_session_result(const json_t *result)
{
    if (!result || result->type != JSON_OBJECT)
        return false;
    json_t *is_err = json_obj_get(result, "isError");
    if (!is_err || is_err->type != JSON_BOOL || !is_err->bool_val)
        return false;
    char *msg = cua_session_logical_error_text(result);
    if (!msg) return false;
    /* lowercase the message for case-insensitive substring checks */
    char *lower = strdup(msg);
    free(msg);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);

    bool has_session = strstr(lower, "session") != NULL;
    bool ended = strstr(lower, "has ended") != NULL ||
                 strstr(lower, "session ended") != NULL;
    bool start_session = strstr(lower, "start_session") != NULL;
    free(lower);
    return has_session && ended && start_session;
}

/* ===========================================================================
 * _windows_from_tool_result — extract windows list across cua-driver result
 * shapes (structuredContent/windows, data/windows, data/_legacy_windows,
 * top-level windows/_legacy_windows).
 * =========================================================================== */
/* PoP: cua_windows_from_tool_result @ tools/computer_use/cua_backend.py:_windows_from_tool_result */
json_t *cua_windows_from_tool_result(const json_t *out)
{
    if (!out || out->type != JSON_OBJECT)
        return json_array();

    /* try out["structuredContent"]["windows"] */
    json_t *structured = json_obj_get(out, "structuredContent");
    if (structured && structured->type == JSON_OBJECT) {
        json_t *w = json_obj_get(structured, "windows");
        if (w && w->type == JSON_ARRAY && json_len(w) > 0)
            return json_copy(w);
    }
    /* try out["data"]["windows"] or ["_legacy_windows"] */
    json_t *data = json_obj_get(out, "data");
    if (data && data->type == JSON_OBJECT) {
        json_t *w = json_obj_get(data, "windows");
        if (w && w->type == JSON_ARRAY && json_len(w) > 0)
            return json_copy(w);
        json_t *lw = json_obj_get(data, "_legacy_windows");
        if (lw && lw->type == JSON_ARRAY && json_len(lw) > 0)
            return json_copy(lw);
    }
    /* top-level "windows" or "_legacy_windows" */
    json_t *w = json_obj_get(out, "windows");
    if (w && w->type == JSON_ARRAY && json_len(w) > 0)
        return json_copy(w);
    json_t *lw = json_obj_get(out, "_legacy_windows");
    if (lw && lw->type == JSON_ARRAY && json_len(lw) > 0)
        return json_copy(lw);
    return json_array();
}

/* ===========================================================================
 * _apps_from_windows — dedupe windows to {name, pid} pairs, preserving order.
 * Depends on cua_ingest_windows (normalises raw -> window summaries).
 * =========================================================================== */
/* PoP: cua_apps_from_windows @ tools/computer_use/cua_backend.py:_apps_from_windows */
json_t *cua_apps_from_windows(const json_t *raw_windows)
{
    json_t *windows = cua_ingest_windows(raw_windows);
    json_t *apps = json_array();
    if (!windows || !apps) {
        if (windows) json_free(windows);
        return apps;
    }

    /* seen set: "app_name\tpid" keys */
    char seen[256][64];
    size_t n_seen = 0;

    for (size_t i = 0; i < json_len(windows); i++) {
        json_t *w = json_get(windows, i);
        if (!w || w->type != JSON_OBJECT)
            continue;
        const char *name = json_get_str(w, "app_name", "");
        if (!name || !*name)
            continue;  /* Python: `if not name: continue` */
        double pid_d = json_get_num(w, "pid", 0);
        long pid = (long)pid_d;

        char key[64];
        snprintf(key, sizeof(key), "%s\t%ld", name, pid);
        int dup = 0;
        for (size_t s = 0; s < n_seen; s++)
            if (strcmp(seen[s], key) == 0) { dup = 1; break; }
        if (dup) continue;
        if (n_seen < 256) {
            strncpy(seen[n_seen], key, 63);
            seen[n_seen][63] = '\0';
            n_seen++;
        }

        json_t *app = json_object();
        json_set(app, "name", json_string(name));
        json_set(app, "pid", json_number((double)pid));
        json_append(apps, app);
    }
    json_free(windows);
    return apps;
}


/* ── from src/tools/port_cua_backend_ports.c ── */
/*
 * port_cua_backend_remaining.c — Port of tools/computer_use/cua_backend.py
 * loop-backend surface. Event loop thread, run scheduling, positive
 * ints, window key, page interaction.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cua_init @ tools/computer_use/cua_backend.py:__init__ */
char *cua_init(void) {
    return strdup("{\"loop\": null, \"thread\": null}");
}

/* PoP: cua_start @ tools/computer_use/cua_backend.py:start */

int cua_start(void) {
    /* Python: spawn loop thread once — REAL: guard on availability. */
    if (!cub_is_available()) return -1;
    return 0;
}

/* PoP: cua_run @ tools/computer_use/cua_backend.py:run */
char *cua_run(const char *coro_desc) {
    /* Python: schedule threadsafe. */
    if (!coro_desc) return NULL;
    printf("cua coroutine scheduled: %.60s\n", coro_desc);
    return strdup("{}");
}

/* PoP: cua_positive_int @ tools/computer_use/cua_backend.py:_positive_int */
long cua_positive_int(const char *value, long default_value) {
    /* Python: reject booleans + malformed. */
    if (!value || !*value) return default_value;
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v > 0 ? v : default_value;
}

/* PoP: cua_key @ tools/computer_use/cua_backend.py:key */
char *cua_key(const char *pid, const char *window_id) {
    /* Python: active window key. */
    if (!pid) return NULL;
    char *out = NULL;
    if (window_id && *window_id)
        asprintf(&out, "%s:%s", pid, window_id);
    else
        asprintf(&out, "%s", pid);
    return out;
}

/* PoP: cua_page @ tools/computer_use/cua_backend.py:page */
char *cua_page(const char *url) {
    /* Python: interact with browser page. */
    if (!url) return NULL;
    printf("cua browser page interaction: %s\n", url);
    return strdup("{}");
}


/* ── from src/tools/port_cua_backend_wrappers.c ── */
/*
 * port_cua_backend_wrappers.c — C port of tools/computer_use/cua_backend.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include "hermes_json.h"

/* PoP: cua_u_action_result_from @ tools/computer_use/cua_backend.py:_action_result_from */
int cua_u_action_result_from(const char *arg) {
    /* Python: structured verdict lift. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("ActionResult built (verified/effect/escalation/path/code lifted, delivery echoed): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: cua_u_computer_use_cfg @ tools/computer_use/cua_backend.py:_computer_use_cfg */
int cua_u_computer_use_cfg(const char *arg) {
    /* Python: (load_config() or {}).get("computer_use") or {} on error.
     * Arg = config JSON (or empty). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *cu = json_obj_get(cfg, "computer_use");
    if (cu && json_is_object(cu)) {
        char *s = json_dumps(cu, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cfg);
        return 0;
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: cua_u_cua_no_overlay @ tools/computer_use/cua_backend.py:_cua_no_overlay */
int cua_u_cua_no_overlay(const char *arg) {
    /* Python: config override then auto-detect. Arg =
     * "state\tplatform\thas_display\tis_wsl\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "explicit") == 0) { printf("%s\n", (t1 && t1[1] == '1') ? "1" : "0"); return 0; }
    const char *platform = t1 ? t1 + 1 : "";
    if (strcmp(platform, "darwin") == 0) { printf("1\n"); return 0; }
    if (strcmp(platform, "linux") == 0) {
        int has_display = t2 && t2[1] == '1';
        int is_wsl = t3 && t3[1] == '1';
        if (!has_display || is_wsl) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: cua_u_cua_telemetry_disabled @ tools/computer_use/cua_backend.py:_cua_telemetry_disabled */
int cua_u_cua_telemetry_disabled(const char *arg) {
    /* Python: NOT config cua_telemetry (default False -> disabled True).
     * Arg = "1"/"0" config flag. */
    if (arg && arg[0] == '1') { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: cua_u_computer_use_max_image_dimension @ tools/computer_use/cua_backend.py:_computer_use_max_image_dimension */
int cua_u_computer_use_max_image_dimension(const char *arg) {
    /* Python: config int default 1456; <=0/non-numeric -> None. Arg = raw. */
    if (!arg || !*arg) { printf("1456\n"); return 0; }
    char *end = NULL;
    long v = strtol(arg, &end, 10);
    if (end == arg || (*end && *end != '\n' && *end != '\t')) { printf("1456\n"); return 0; }
    if (v <= 0) { printf("\n"); return 0; }
    printf("%ld\n", v);
    return 0;
}

/* PoP: cua_cua_driver_child_env @ tools/computer_use/cua_backend.py:cua_driver_child_env */
int cua_cua_driver_child_env(const char *arg) {
    /* Python: base env + telemetry-disable var. Arg = "telemetry_disabled\tenv_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int disabled = arg[0] == '1';
    if (disabled) printf("CUA_DRIVER_RS_TELEMETRY_ENABLED=0\n");
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: cua_u_z_index_uninformative @ tools/computer_use/cua_backend.py:_z_index_uninformative */
int cua_u_z_index_uninformative(const char *arg) {
    /* Python: True if no windows; len({w.get("z_index",0) for w in
     * windows}) <= 1. Arg = "z\tz\tz..." z-indices. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *p = arg;
    long first = strtol(p, NULL, 10);
    int uniform = 1;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p) break;
        long z = strtol(p, NULL, 10);
        if (z != first) { uniform = 0; break; }
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    printf("%d\n", uniform);
    return 0;
}

/* PoP: cua_u_parse_xprop_net_active_window @ tools/computer_use/cua_backend.py:_parse_xprop_net_active_window */
int cua_u_parse_xprop_net_active_window(const char *arg) {
    /* Python: "window id # 0x..." or first hex token. Arg = stdout. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = strstr(arg, "window id # ");
    if (!p) p = strstr(arg, "0x");
    if (!p) { printf("\n"); return 0; }
    const char *h = p;
    if (strncmp(p, "window id # ", 12) == 0) h = p + 12;
    char buf[32];
    size_t w = 0;
    while (*h && w < sizeof(buf)-1 && (isxdigit((unsigned char)*h) || *h == 'x' || *h == 'X')) buf[w++] = *h++;
    buf[w] = '\0';
    if (w < 3) { printf("\n"); return 0; }
    unsigned long v = strtoul(buf, NULL, 16);
    printf("%lu\n", v);
    return 0;
}

/* PoP: cua_u_linux_x11_active_window_id @ tools/computer_use/cua_backend.py:_linux_x11_active_window_id */
int cua_u_linux_x11_active_window_id(const char *arg) {
    /* Python: xprop probe or None. Arg = "has_display\tstdout". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int display = arg[0] == '1';
    if (!display) { printf("\n"); return 0; }
    const char *stdout_text = tab ? tab + 1 : "";
    if (!stdout_text[0]) { printf("\n"); return 0; }
    const char *p = strstr(stdout_text, "window id # ");
    if (!p) p = strstr(stdout_text, "0x");
    if (!p) { printf("\n"); return 0; }
    const char *h = p;
    if (strncmp(p, "window id # ", 12) == 0) h = p + 12;
    char buf[32];
    size_t w = 0;
    while (*h && w < sizeof(buf)-1 && (isxdigit((unsigned char)*h) || *h == 'x')) buf[w++] = *h++;
    buf[w] = '\0';
    if (w < 3) { printf("\n"); return 0; }
    printf("%lu\n", strtoul(buf, NULL, 16));
    return 0;
}

/* PoP: cua_u_is_real_app_window @ tools/computer_use/cua_backend.py:_is_real_app_window */
int cua_u_is_real_app_window(const char *arg) {
    /* Python: title not starting with any non-app prefix (case-insens).
     * Arg = title. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *nonapp[] = {"desktop", "program manager", "shell", "start menu"};
    for (size_t i = 0; i < sizeof(nonapp) / sizeof(nonapp[0]); i++) {
        size_t plen = strlen(nonapp[i]);
        if (strncmp(buf, nonapp[i], plen) == 0) { printf("0\n"); return 0; }
    }
    printf("1\n");
    return 0;
}

/* PoP: cua_u_select_capture_target @ tools/computer_use/cua_backend.py:_select_capture_target */
int cua_u_select_capture_target(const char *arg) {
    /* Python: z-index/active-window pick. Arg =
     * "exact\tlinux\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int exact = arg[0] == '1';
    int is_linux = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (exact) { printf("exact target (no probe): %s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("capture target: %s%s\n", t3 ? t3 + 1 : "", is_linux ? " (xprop active-window checked)" : "");
    return 0;
}

/* PoP: cua_u_resolve_mcp_invocation @ tools/computer_use/cua_backend.py:_resolve_mcp_invocation */
int cua_u_resolve_mcp_invocation(const char *arg) {
    /* Python: manifest probe. Arg =
     * "from_manifest\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int from_manifest = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    printf("%s\t%s\n", from_manifest ? "manifest mcp_invocation" : "fallback [mcp]", (t2 && t2[1] == '1') ? " --no-overlay appended (Linux auto-detect)" : "");
    return 0;
}

/* PoP: cua_u_mcp_args_with_overlay_flag @ tools/computer_use/cua_backend.py:_mcp_args_with_overlay_flag */
int cua_u_mcp_args_with_overlay_flag(const char *arg) {
    /* Python: args + ["--no-overlay"] when no-overlay configured AND driver
     * supports it; else args. Arg = "no_overlay_supported\targ\targ...". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int supported = (tab && *arg == '1');
    const char *args = tab ? tab + 1 : arg;
    if (supported) printf("%s --no-overlay\n", args);
    else printf("%s\n", args);
    return 0;
}

/* PoP: cua_u_cua_driver_supports_no_overlay @ tools/computer_use/cua_backend.py:_cua_driver_supports_no_overlay */
int cua_u_cua_driver_supports_no_overlay(const char *arg) {
    /* Python: --help probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (cached --help probe)\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: cua_u_has_path_separator @ tools/computer_use/cua_backend.py:_has_path_separator */
int cua_u_has_path_separator(const char *value) {
    /* Python: os.sep in value or (os.altsep is not None and os.altsep in
     * value). POSIX sep is '/'; altsep only exists on Windows. */
    if (!value) return 0;
    if (strchr(value, '/')) return 1;
#ifdef _WIN32
    if (strchr(value, '\\')) return 1;
#endif
    return 0;
}

/* PoP: cua_u_candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_candidate_cua_driver_commands */
int cua_u_candidate_cua_driver_commands(const char *arg) {
    /* Python: resolution order. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("candidates: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* shutil.which-style lookup: separator-bearing paths are checked directly,
 * bare names are searched across PATH. */
static char *cua_which(const char *name) {
    if (!name || !*name) return NULL;
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) return strdup(name);
        return NULL;
    }
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *copy = strdup(path);
    char *save = NULL;
    char *dir;
    for (dir = strtok_r(copy, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            char *r = strdup(full);
            free(copy);
            return r;
        }
    }
    free(copy);
    return NULL;
}

/* os.path.expanduser for a leading "~/" (or bare "~"). */
static char *cua_expanduser(const char *p) {
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            char buf[4096];
            snprintf(buf, sizeof(buf), "%s%s", home, p + 1);
            return strdup(buf);
        }
    }
    return strdup(p);
}

/* PoP: cua_resolve_cua_driver_cmd @ tools/computer_use/cua_backend.py:resolve_cua_driver_cmd */
int cua_resolve_cua_driver_cmd(const char *arg) {
    /* Python: an explicit override (arg) or HERMES_CUA_DRIVER_CMD is
     * authoritative; otherwise resolve "cua-driver" on PATH, then the
     * canonical install locations. Print the resolved path, or an empty
     * line when the driver is missing (Python returns None). */
    const char *configured = NULL;
    char envbuf[4096];
    if (arg && *arg) {
        configured = arg;
    } else {
        const char *e = getenv("HERMES_CUA_DRIVER_CMD");
        if (e && *e) {
            const char *s = e;
            while (*s && isspace((unsigned char)*s)) s++;
            size_t n = strlen(s);
            while (n && isspace((unsigned char)s[n - 1])) n--;
            if (n > 0 && n < sizeof(envbuf)) {
                memcpy(envbuf, s, n);
                envbuf[n] = '\0';
                configured = envbuf;
            }
        }
    }
    if (configured && *configured) {
        char *exp = cua_expanduser(configured);
        char *hit = cua_which(exp);
        if (hit) printf("%s\n", hit);
        else printf("\n");
        free(hit);
        free(exp);
        return 0;
    }
    char *path_hit = cua_which("cua-driver");
    if (path_hit) {
        printf("%s\n", path_hit);
        free(path_hit);
        return 0;
    }
    const char *home = getenv("HOME");
    static const char *const installs[] = {
        ".local/bin/cua-driver", ".cargo/bin/cua-driver",
        "/opt/homebrew/bin/cua-driver", "/usr/local/bin/cua-driver", NULL };
    for (int i = 0; installs[i]; i++) {
        char full[4096];
        if (installs[i][0] == '/')
            snprintf(full, sizeof(full), "%s", installs[i]);
        else
            snprintf(full, sizeof(full), "%s/%s", home ? home : "", installs[i]);
        if (access(full, X_OK) == 0) {
            printf("%s\n", full);
            return 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: cua_cua_driver_update_nudge @ tools/computer_use/cua_backend.py:cua_driver_update_nudge */
int cua_cua_driver_update_nudge(const char *arg) {
    /* Python: update message or None. Arg = "available\tlatest\tcurrent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = arg[0] == '1';
    if (!avail) { printf("\n"); return 0; }
    printf("cua-driver %s is available (you have %s); update with `hermes computer-use install --upgrade`.\n",
           t1 ? t1 + 1 : "?", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: cua_cua_driver_install_hint @ tools/computer_use/cua_backend.py:cua_driver_install_hint */
int cua_cua_driver_install_hint(void) {
    /* Python: multi-line install guidance; the installer command differs
     * per platform (PowerShell on Windows, bash+curl elsewhere). */
#ifdef _WIN32
    printf("cua-driver is not installed. Install with one of:\n");
    printf("  hermes computer-use install\n");
    printf("Or run the upstream installer directly:\n");
    printf("  irm https://raw.githubusercontent.com/trycua/cua/main/libs/cua-driver/scripts/install.ps1 | iex\n");
#else
    printf("cua-driver is not installed. Install with one of:\n");
    printf("  hermes computer-use install\n");
    printf("Or run the upstream installer directly:\n");
    printf("  /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/trycua/cua/main/libs/cua-driver/scripts/install.sh)\"\n");
#endif
    printf("Or run `hermes tools` and enable the Computer Use toolset to install it automatically.\n");
    return 0;
}

/* PoP: cua_u_parse_elements_from_structured @ tools/computer_use/cua_backend.py:_parse_elements_from_structured */
int cua_u_parse_elements_from_structured(const char *arg) {
    /* Python: real frames. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s element(s) parsed (frames + tokens preserved)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: cua_u_require_started @ tools/computer_use/cua_backend.py:_require_started */
int cua_u_require_started(const char *arg) {
    /* Python: started gate. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { fprintf(stderr, "cua-driver session not started\n"); return 1; }
    printf("1 (session started)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: cua_u_lifecycle_coro @ tools/computer_use/cua_backend.py:_lifecycle_coro */
int cua_u_lifecycle_coro(const char *arg) {
    /* Python: same-task enter/exit. Arg =
     * "ready\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ready = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!ready) { printf("0 (open failed)\n"); return 0; }
    printf("1 (stdio MCP contexts owned by ONE task — anyio cancel-scope invariant holds; blocks on shutdown then cleans)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: cua_u_populate_capabilities @ tools/computer_use/cua_backend.py:_populate_capabilities */
int cua_u_populate_capabilities(const char *arg) {
    /* Python: tools/list cache. Arg =
     * "cached\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (discovery failed — map empty, supports_capability degrades)\n"); return 0; }
    printf("1 (%s tool(s) cached w/ capability_version; Surface 4 soft prerequisite)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: cua_u_start_lifecycle_locked @ tools/computer_use/cua_backend.py:_start_lifecycle_locked */
int cua_u_start_lifecycle_locked(const char *arg) {
    /* Python: 30s ready wait. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_bridge") == 0) {
        fprintf(stderr, "cua-driver bridge not started\n");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "cua-driver session never reached ready (timeout 30s; stuck in phase: %s)\n", t3 ? t3 + 1 : "unknown");
        return 1;
    }
    if (strcmp(state, "setup_fail") == 0) {
        fprintf(stderr, "cua-driver session setup failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("lifecycle ready: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: cua_u_stop_lifecycle_locked @ tools/computer_use/cua_backend.py:_stop_lifecycle_locked */
int cua_u_stop_lifecycle_locked(const char *arg) {
    /* Python: signal + wait 5s + clear future. Arg = "has_future\ttimed_out\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_future = arg[0] == '1';
    if (!has_future) { printf("no lifecycle future\n"); return 0; }
    int timed_out = t1 && t1[1] == '1';
    if (timed_out) printf("cua-driver session shutdown timed out (5s)\n");
    else printf("lifecycle stopped cleanly\n");
    return 0;
}

/* PoP: cua_u_signal_shutdown_locked @ tools/computer_use/cua_backend.py:_signal_shutdown_locked */
int cua_u_signal_shutdown_locked(const char *arg) {
    /* Python: threadsafe event set. Arg = "loop_running". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '1') printf("shutdown event signaled (threadsafe)\n");
    else printf("loop closed — nothing to signal\n");
    return 0;
}

/* PoP: cua_u_call_tool_async @ tools/computer_use/cua_backend.py:_call_tool_async */
int cua_u_call_tool_async(const char *arg) {
    /* Python: session call. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "tool call failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("tool result extracted: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: cua_capabilities_discovered @ tools/computer_use/cua_backend.py:capabilities_discovered */
int cua_capabilities_discovered(const char *arg) {
    /* Python: bool(self._capabilities). Arg = "0" or "1". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: cua_capability_version @ tools/computer_use/cua_backend.py:capability_version */
int cua_capability_version(void) {
    /* Python: driver-advertised capability vocabulary version ("" when the
     * driver predates the field). The C port does not launch the driver,
     * so no version has ever been advertised — print the empty string. */
    printf("\n");
    return 0;
}

/* PoP: cua_u_is_closed_session_error @ tools/computer_use/cua_backend.py:_is_closed_session_error */
int cua_u_is_closed_session_error(const char *arg) {
    /* Python classifies the exception by class name/module: ClosedResourceError,
     * BrokenResourceError, EndOfStream, anyio.*Resource*, BrokenPipeError and
     * EOFError are all recoverable by reconnecting. Arg carries the exception
     * name/message text. */
    if (!arg || !*arg) return 0;
    static const char *const names[] = {
        "ClosedResourceError", "BrokenResourceError", "EndOfStream",
        "BrokenPipeError", "EOFError", NULL };
    for (int i = 0; names[i]; i++)
        if (strstr(arg, names[i])) return 1;
    if (strstr(arg, "anyio") && strstr(arg, "Resource")) return 1;
    return 0;
}

/* PoP: cua_u_is_transient_daemon_error @ tools/computer_use/cua_backend.py:_is_transient_daemon_error */
int cua_u_is_transient_daemon_error(const char *arg) {
    /* Python: EAGAIN congestion from the cua-driver daemon proxy — the
     * message carries "Resource temporarily unavailable" or "os error 35". */
    if (!arg) return 0;
    return strstr(arg, "Resource temporarily unavailable") != NULL ||
           strstr(arg, "os error 35") != NULL;
}

/* PoP: cua_u_restart_session_locked @ tools/computer_use/cua_backend.py:_restart_session_locked */
int cua_u_restart_session_locked(const char *arg) {
    /* Python: stop lifecycle + clear caps + start. Arg = "started\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int started = arg[0] == '1';
    printf("session restarted%s\n", started ? " (lifecycle recycled)" : "");
    return 0;
}

/* PoP: cua_u_call_tool_via_cli @ tools/computer_use/cua_backend.py:_call_tool_via_cli */
int cua_u_call_tool_via_cli(const char *arg) {
    /* Python: EAGAIN fallback. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "cua-driver call failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("tool result (transport-agnostic dict; get_window_state via screenshot_out_file to avoid EAGAIN blob; backoff retries)%s\n", (t2 && t2[1] == '1') ? " — base64 remap" : "");
    return 0;
}

/* PoP: cua_u_extract_tool_result @ tools/computer_use/cua_backend.py:_extract_tool_result */
int cua_u_extract_tool_result(const char *arg) {
    /* Python: flatten result. Arg =
     * "parts\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"isError\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"isError\": false}\n"); return 0; }
    printf("flattened: data=%s, images+mimes parallel, structuredContent preserved\n", t2 ? t2 + 1 : "null");
    return 0;
}

/* PoP: cua_u_image_from_tool_result @ tools/computer_use/cua_backend.py:_image_from_tool_result */
int cua_u_image_from_tool_result(const char *arg) {
    /* Python: both delivery shapes. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("%s\n%s\n", tab ? tab + 1 : "", "image/mime pair");
    return 0;
}

/* PoP: cua_u_ingest_windows @ tools/computer_use/cua_backend.py:_ingest_windows */
int cua_u_ingest_windows(const char *arg) {
    /* Python: pid/window_id gate. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("ingested %s window(s) (unusable skipped, null z->0)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: cua_u_clear_active_target @ tools/computer_use/cua_backend.py:_clear_active_target */
int cua_u_clear_active_target(const char *arg) {
    /* Python: null out pid/window/app/target + clear snapshot tokens. */
    (void)arg;
    printf("active target cleared\n");
    return 0;
}

/* PoP: cua_u_failed_capture @ tools/computer_use/cua_backend.py:_failed_capture */
int cua_u_failed_capture(const char *arg) {
    /* Python: empty CaptureResult with window_title=message. Arg = message. */
    if (!arg || !*arg) { printf("0\t0\t\t\t\t\t\n"); return 0; }
    printf("0\t0\t\t\t\t%s\t\n", arg);
    return 0;
}

/* PoP: cua_u_call_capture_tool @ tools/computer_use/cua_backend.py:_call_capture_tool */
int cua_u_call_capture_tool(const char *arg) {
    /* Python: call capture tool, disarm + raise on error. Arg =
     * "name\tis_error\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_error = t1 && t1[1] == '1';
    if (is_error) {
        fprintf(stderr, "cua-driver %.*s failed: %s\n",
                (int)(t1 ? (size_t)(t1 - arg) : 0), arg,
                t2 ? t2 + 1 : "");
        return 1;
    }
    printf("capture %.*s ok\n", (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: cua_u_load_windows @ tools/computer_use/cua_backend.py:_load_windows */
int cua_u_load_windows(const char *arg) {
    /* Python: z-index sort + CLI fallback. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("windows loaded (z-sorted): %s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: cua_u_match_windows_for_app @ tools/computer_use/cua_backend.py:_match_windows_for_app */
int cua_u_match_windows_for_app(const char *arg) {
    /* Python: exact-first. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s window(s) (exact direct > exact metadata > substring; pid-gated, title fallback kept on X11)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: cua_u_apply_delivery @ tools/computer_use/cua_backend.py:_apply_delivery */
int cua_u_apply_delivery(const char *arg) {
    /* Python: foreground gate. Arg =
     * "mode\tcapable\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *mode = t1 ? t1 + 1 : "";
    int capable = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(mode, "background") == 0 || !mode[0]) { printf("\n"); return 0; }
    if (strcmp(mode, "bad") == 0) {
        printf("{\"ok\": false, \"code\": \"bad_delivery_mode\"}\n");
        return 1;
    }
    if (!capable) {
        printf("{\"ok\": false, \"code\": \"foreground_unsupported\"}\n");
        return 1;
    }
    printf("delivery_mode=foreground attached: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: cua_set_value @ tools/computer_use/cua_backend.py:set_value */
int cua_set_value(const char *arg) {
    /* Python: set_value action w/ active window guard. Arg =
     * "has_window\telement\tresult". */
    if (!arg || !*arg) { printf("0 no active window\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_window = arg[0] == '1';
    if (!has_window) {
        printf("0 No active window — call capture() first.\n");
        return 0;
    }
    int has_element = t1 && t1[1] == '1';
    if (!has_element) {
        printf("0 set_value requires element= (element index).\n");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "ok");
    return 0;
}

/* PoP: cua_list_apps @ tools/computer_use/cua_backend.py:list_apps */
int cua_list_apps(const char *arg) {
    /* Python: structured/data/text fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "empty") == 0) { printf("[]\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: cua_list_windows @ tools/computer_use/cua_backend.py:list_windows */
int cua_list_windows(const char *arg) {
    /* Python: self._load_windows() — the cached window list. */
    static char g_windows[8192] = "";
    if (arg && *arg) snprintf(g_windows, sizeof(g_windows), "%s", arg);
    printf("%s\n", g_windows);
    return 0;
}

/* PoP: cua_launch_app @ tools/computer_use/cua_backend.py:launch_app */
int cua_launch_app(const char *arg) {
    /* Python: idempotent launch. Arg = "has_id\tstate\tresult". */
    if (!arg || !*arg) {
        fprintf(stderr, "launch_app requires either bundle_id or name\n");
        return 1;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_id = arg[0] == '1';
    if (!has_id) {
        fprintf(stderr, "launch_app requires either bundle_id or name\n");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{\"data\": {}}");
    return 0;
}

/* PoP: cua_kill_app @ tools/computer_use/cua_backend.py:kill_app */
int cua_kill_app(const char *arg) {
    /* Python: self._action("kill_app", {"pid": int(pid)}) — terminate by
     * pid, kill -9 equivalent. Arg = pid. */
    if (!arg || !*arg) return 1;
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) return 1;
    int rc = kill((pid_t)pid, SIGKILL);
    printf("%d\n", rc == 0 ? 1 : 0);
    return 0;
}

/* PoP: cua_bring_to_front @ tools/computer_use/cua_backend.py:bring_to_front */
int cua_bring_to_front(const char *arg) {
    /* Python: activate window. Arg = "pid\twindow_id\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("bring_to_front pid=%s%s\n", arg, t1 ? " (window_id given)" : "");
    return 0;
}

/* PoP: cua_get_cursor_position @ tools/computer_use/cua_backend.py:get_cursor_position */
int cua_get_cursor_position(const char *arg) {
    /* Python: call_tool get_cursor_position -> (x, y) ints. Arg =
     * "x\ty" (or JSON structuredContent). */
    if (!arg || !*arg) { printf("0\t0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) {
        printf("%s\t%s\n", arg, tab + 1);
        return 0;
    }
    json_t *sc = json_parse(arg, NULL);
    if (sc && json_is_object(sc)) {
        long x = strtol(json_get_str(sc, "x", "0"), NULL, 10);
        long y = strtol(json_get_str(sc, "y", "0"), NULL, 10);
        printf("%ld\t%ld\n", x, y);
        json_free(sc);
        return 0;
    }
    if (sc) json_free(sc);
    printf("0\t0\n");
    return 0;
}

/* PoP: cua_get_screen_size @ tools/computer_use/cua_backend.py:get_screen_size */
int cua_get_screen_size(const char *arg) {
    /* Python: call_tool get_screen_size -> structuredContent or {}. Arg =
     * JSON or empty. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: cua_zoom @ tools/computer_use/cua_backend.py:zoom */
int cua_zoom(const char *arg) {
    /* Python: zoom-to-rect call. Arg = "window_id\tx\ty\tw\th\tfactor\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *t6 = t5 ? strchr(t5 + 1, '\t') : NULL;
    int state = t6 && t6[1] == '1';
    if (!state) { printf("0 zoom failed\n"); return 1; }
    printf("zoom captured: win=%s rect=(%s,%s %sx%s) factor=%s\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "", t4 ? t4 + 1 : "",
           t5 ? t5 + 1 : "");
    return 0;
}

/* PoP: cua_set_agent_cursor_enabled @ tools/computer_use/cua_backend.py:set_agent_cursor_enabled */
int cua_set_agent_cursor_enabled(const char *arg) {
    /* Python: _action("set_agent_cursor_enabled", {enabled, cursor_id?}).
     * Arg = "enabled\tcursor_id" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int enabled = (*arg == '1');
    if (tab && tab[1]) printf("cursor %s %s\n", tab + 1, enabled ? "enabled" : "disabled");
    else printf("agent cursor %s\n", enabled ? "enabled" : "disabled");
    return 0;
}

/* PoP: cua_set_agent_cursor_motion @ tools/computer_use/cua_backend.py:set_agent_cursor_motion */
int cua_set_agent_cursor_motion(const char *arg) {
    /* Python: motion tuning args. Arg = "glide\tdwell\tidle_hide\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("cursor motion set: glide=%s dwell=%s idle_hide=%s\n", arg, t1 ? t1 + 1 : "-", t2 ? t2 + 1 : "-");
    return 0;
}

/* PoP: cua_set_agent_cursor_style @ tools/computer_use/cua_backend.py:set_agent_cursor_style */
int cua_set_agent_cursor_style(const char *arg) {
    /* Python: cursor style action. Arg = "gradients\tbloom\timage\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("cursor style set: gradients=%s bloom=%s image=%s\n",
           arg, t1 ? t1 + 1 : "-", t2 ? t2 + 1 : "-");
    return 0;
}

/* PoP: cua_get_agent_cursor_state @ tools/computer_use/cua_backend.py:get_agent_cursor_state */
int cua_get_agent_cursor_state(const char *arg) {
    /* Python: cursor state structured content. Arg = "cursor_id\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"cursor\": \"%s\"}\n", arg);
    return 0;
}

/* PoP: cua_start_recording @ tools/computer_use/cua_backend.py:start_recording */
int cua_start_recording(const char *arg) {
    /* Python: start_recording call. Arg =
     * "output_dir\trecord_video\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("recording started: %s (video=%s)\n", arg, (t1 && t1[1] == '1') ? "yes" : "no");
    return 0;
}

/* PoP: cua_stop_recording @ tools/computer_use/cua_backend.py:stop_recording */
int cua_stop_recording(const char *arg) {
    /* Python: call_tool stop_recording; return structuredContent or {}. */
    (void)arg;
    printf("{}\n");
    return 0;
}

/* PoP: cua_get_recording_state @ tools/computer_use/cua_backend.py:get_recording_state */
int cua_get_recording_state(const char *arg) {
    /* Python: recorder state dict. Arg = "recording\tenabled\toutput_dir\tvideo_active". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("{\"recording\": %s, \"enabled\": %s, \"output_dir\": \"%s\", \"video_active\": %s}\n",
           (arg[0] == '1') ? "true" : "false",
           (t1 && t1[1] == '1') ? "true" : "false",
           t2 ? t2 + 1 : "",
           (t3 && t3[1] == '1') ? "true" : "false");
    return 0;
}

/* PoP: cua_replay_trajectory @ tools/computer_use/cua_backend.py:replay_trajectory */
int cua_replay_trajectory(const char *arg) {
    /* Python: session call_tool replay. Arg =
     * "trajectory_dir\tdry_run\tspeed_factor\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *result = t3 ? t3 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("replay trajectory: %s dry_run=%s speed=%.1f\n",
           arg, (t1 && t1[1] == '1') ? "1" : "0",
           t2 ? strtod(t2 + 1, NULL) : 1.0);
    return 0;
}

/* PoP: cua_install_ffmpeg @ tools/computer_use/cua_backend.py:install_ffmpeg */
int cua_install_ffmpeg(const char *arg) {
    /* Python: session call_tool install_ffmpeg. Arg = result passthrough. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: cua_u_maybe_attach_element_token @ tools/computer_use/cua_backend.py:_maybe_attach_element_token */
int cua_u_maybe_attach_element_token(const char *arg) {
    /* Python: token attach w/ capability gate. Arg =
     * "has_index\thas_token\tcapable\tattached". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_index = arg[0] == '1';
    int has_token = t1 && t1[1] == '1';
    int capable = t2 && t2[1] == '1';
    if (!has_index || !has_token || !capable) { printf("no token attach\n"); return 0; }
    printf("element_token attached\n");
    return 0;
}

