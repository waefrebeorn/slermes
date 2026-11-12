/*
 * computer_use_backend.c - computer-use backend helpers.
 *
 * Split from computer_use.c (soul.md: self-contained modules). The
 * blocked-key/type predicates with their tables, the action/capture
 * destructors and the backend factories. No file-scope state.
 *
 * NOT here (all anchored in computer_use.c):
 *   find_backend_by_name / cu_list_backends   -> g_backend_registry
 *   cu_reset_global_backend                   -> g_cu_backend
 *   computer_use_select_backend               -> cu_auto_register
 *   json_get_str / json_get_int               -> inside #ifdef __APPLE__
 *                                                and they shadow the
 *                                                library json_get_str
 */
#include "computer_use_backend.h"
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <windows.h>
#include "computer_use_windows.h"   /* make_windows_backend */
#include "computer_use_noop.h"     /* make_noop_backend */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static const char *BLOCKED_COMBOS[] = {
    "cmd+shift+backspace",
    "cmd+option+backspace",
    "cmd+ctrl+q",
    "cmd+shift+q",
    "cmd+option+shift+q",
    NULL
};

static const char *BLOCKED_TYPE_PATTERNS[] = {
    "curl | bash",
    "curl | sh",
    "wget | bash",
    "sudo rm -rf",
    "rm -rf /",
    NULL
};



cu_backend_t *computer_use_new_noop_backend(void) {
    return make_noop_backend();
}


/* ======================================================================
 *  Free helpers
 * ====================================================================== */

/* PoP: cu_capture_free @ tools/computer_use/cua_backend.py:CuaDriverBackend.get_accessibility_tree */
void cu_capture_free(cu_capture_t *cap) {
    if (!cap) return;
    free(cap->png_b64);
    free(cap->elements);
    free(cap);
}


/* PoP: cu_action_free @ tools/computer_use/cua_backend_parse.py:_parse_elements_from_tree */
void cu_action_free(cu_action_t *act) {
    if (!act) return;
    if (act->capture) cu_capture_free(act->capture);
    free(act);
}
 bool is_blocked_key_combo(const char *keys) {
    if (!keys) return false;
    char buf[128];
    size_t blen = strlen(keys);
    if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
    for (size_t i = 0; i < blen; i++)
        buf[i] = (char)tolower((unsigned char)keys[i]);
    buf[blen] = '\0';

    for (int i = 0; BLOCKED_COMBOS[i]; i++) {
        if (strstr(buf, BLOCKED_COMBOS[i])) return true;
    }
    return false;
}


/* Port of Python tools/computer_use/tool.py:_is_blocked_type(). */ bool is_blocked_type(const char *text) {
    if (!text) return false;
    for (int i = 0; BLOCKED_TYPE_PATTERNS[i]; i++) {
        if (strstr(text, BLOCKED_TYPE_PATTERNS[i])) return true;
    }
    return false;
}
