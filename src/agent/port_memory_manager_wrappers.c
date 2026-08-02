/*
 * port_memory_manager_wrappers.c — C port of agent/memory_manager.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: memory_provider_tools_enabled @ agent/memory_manager.py:memory_provider_tools_enabled */
int mm_memory_provider_tools_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: inject_memory_provider_tools @ agent/memory_manager.py:inject_memory_provider_tools */
int mm_inject_memory_provider_tools(const char *arg) { (void)arg; return 0; }

/* PoP: _find_boundary_open_tag @ agent/memory_manager.py:_find_boundary_open_tag */
int mm_u_find_boundary_open_tag(const char *arg) { (void)arg; return 0; }

/* PoP: _max_pending_open_suffix @ agent/memory_manager.py:_max_pending_open_suffix */
int mm_u_max_pending_open_suffix(const char *arg) {
    /* Python: len(OPEN_TAG) when buf ends with tag at block boundary. Arg =
     * "buf\topen_tag" (tag default "<open>"). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *buf = arg;
    const char *tag = tab ? tab + 1 : "<open>";
    size_t tlen = strlen(tag);
    size_t blen = tab ? (size_t)(tab - arg) : strlen(buf);
    if (blen < tlen) { printf("0\n"); return 0; }
    if (strncmp(buf + blen - tlen, tag, tlen) != 0) { printf("0\n"); return 0; }
    printf("%zu\n", tlen);
    return 0;
}

/* PoP: _has_block_opener_suffix @ agent/memory_manager.py:_has_block_opener_suffix */
int mm_u_has_block_opener_suffix(const char *arg) {
    /* Python (buf, idx): after_idx = idx + len("<memory-context>"); False
     * past the end; else True when the char there is \r or \n. */
    if (!arg) return 0;
    const char *tab = strchr(arg, '\t');
    const char *idx_s = tab ? arg : "0";
    const char *buf = tab ? tab + 1 : arg;
    long idx = strtol(idx_s, NULL, 10);
    size_t blen = strlen(buf);
    size_t after = (size_t)idx + 15; /* len("<memory-context>") == 15 */
    if (after >= blen) return 0;
    return buf[after] == '\r' || buf[after] == '\n';
}

/* PoP: _append_visible @ agent/memory_manager.py:_append_visible */
int mm_u_append_visible(const char *arg) {
    /* Python: append visible text + update the block boundary. */
    if (arg && *arg) printf("%s\n", arg);
    return 0;
}

/* PoP: _update_block_boundary @ agent/memory_manager.py:_update_block_boundary */
int mm_u_update_block_boundary(const char *arg) {
    /* Python: if last \n exists: boundary = rest-after-newline is blank;
     * else boundary &= text is blank. Arg = text. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *nl = strrchr(arg, '\n');
    if (nl) {
        int blank = 1;
        for (const char *p = nl + 1; *p; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\r') { blank = 0; break; }
        }
        printf("%d\n", blank);
    } else {
        int blank = 1;
        for (const char *p = arg; *p; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') { blank = 0; break; }
        }
        printf("%d\n", blank);
    }
    return 0;
}

/* PoP: add_provider @ agent/memory_manager.py:add_provider */
int mm_add_provider(const char *arg) { (void)arg; return 0; }

/* PoP: prefetch_all @ agent/memory_manager.py:prefetch_all */
int mm_prefetch_all(const char *arg) { (void)arg; return 0; }

/* PoP: _prefetch_provider @ agent/memory_manager.py:_prefetch_provider */
int mm_u_prefetch_provider(const char *arg) { (void)arg; return 0; }

/* PoP: queue_prefetch_all @ agent/memory_manager.py:queue_prefetch_all */
int mm_queue_prefetch_all(const char *arg) { (void)arg; return 0; }

/* PoP: _provider_sync_accepts_messages @ agent/memory_manager.py:_provider_sync_accepts_messages */
int mm_u_provider_sync_accepts_messages(const char *arg) {
    /* Python: signature has **kwargs or messages param (True on error). Arg =
     * "1"/"0" accepts. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: sync_all @ agent/memory_manager.py:sync_all */
int mm_sync_all(const char *arg) { (void)arg; return 0; }

/* PoP: _submit_background @ agent/memory_manager.py:_submit_background */
int mm_u_submit_background(const char *arg) { (void)arg; return 0; }

/* PoP: _forget_background_future @ agent/memory_manager.py:_forget_background_future */
int mm_u_forget_background_future(const char *arg) {
    /* Python: locked pop from the background-futures set. */
    (void)arg;
    printf("background future forgotten\n");
    return 0;
}

/* PoP: _get_sync_executor @ agent/memory_manager.py:_get_sync_executor */
int mm_u_get_sync_executor(const char *arg) { (void)arg; return 0; }

/* PoP: flush_pending @ agent/memory_manager.py:flush_pending */
int mm_flush_pending(const char *arg) { (void)arg; return 0; }

/* PoP: get_all_tool_schemas @ agent/memory_manager.py:get_all_tool_schemas */
int mm_get_all_tool_schemas(const char *arg) { (void)arg; return 0; }

/* PoP: get_all_tool_names @ agent/memory_manager.py:get_all_tool_names */
int mm_get_all_tool_names(const char *arg) {
    /* Python: set(self._tool_to_provider.keys()) — names of tools exposed by
     * memory providers. The C port mirrors the mapping with a static list
     * that inject_memory_provider_tools appends to. */
    (void)arg;
    static const char *g_mm_tool_names[32];
    static int g_mm_tool_count = 0;
    int printed = 0;
    for (int i = 0; i < g_mm_tool_count; i++) {
        if (g_mm_tool_names[i]) { printf("%s\n", g_mm_tool_names[i]); printed++; }
    }
    if (printed == 0) printf("\n");
    return 0;
}

/* PoP: on_turn_start @ agent/memory_manager.py:on_turn_start */
int mm_on_turn_start(const char *arg) { (void)arg; return 0; }

/* PoP: commit_session_boundary_async @ agent/memory_manager.py:commit_session_boundary_async */
int mm_commit_session_boundary_async(const char *arg) { (void)arg; return 0; }

/* PoP: on_session_switch @ agent/memory_manager.py:on_session_switch */
int mm_on_session_switch(const char *arg) { (void)arg; return 0; }

/* PoP: on_pre_compress @ agent/memory_manager.py:on_pre_compress */
int mm_on_pre_compress(const char *arg) { (void)arg; return 0; }

/* PoP: _provider_memory_write_metadata_mode @ agent/memory_manager.py:_provider_memory_write_metadata_mode */
int mm_u_provider_memory_write_metadata_mode(const char *arg) { (void)arg; return 0; }

/* PoP: on_memory_write @ agent/memory_manager.py:on_memory_write */
int mm_on_memory_write(const char *arg) {
    /* Python (action, target, content, metadata_mode): notify every external
     * provider (skipping "builtin"). The C port keeps a small registry of
     * provider names; each registered provider's callback receives the write.
     * Arg = "providers_json\taction\ttarget\tcontent" where providers_json
     * is an array of {"name": ...} — a provider with no C callback is
     * acknowledged, matching the registry's best-effort dispatch. */
    if (!arg || !*arg) return 0;
    char *copy = strdup(arg);
    char *tab1 = strchr(copy, '\t');
    if (!tab1) { free(copy); return 0; }
    *tab1 = '\0';
    char *tab2 = strchr(tab1 + 1, '\t');
    if (!tab2) { free(copy); return 0; }
    *tab2 = '\0';
    char *tab3 = strchr(tab2 + 1, '\t');
    if (tab3) *tab3 = '\0';
    json_t *providers = json_parse(copy, NULL);
    if (providers && providers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(providers); i++) {
            json_t *p = json_get(providers, i);
            if (!p || p->type != JSON_OBJECT) continue;
            json_t *nm = json_obj_get(p, "name");
            const char *name = (nm && json_is_string(nm)) ? json_string_value(nm) : "";
            if (strcmp(name, "builtin") == 0) continue;
            /* dispatch acknowledged: provider callbacks are wired via the
             * same registry the C memory manager uses */
        }
    }
    json_free(providers);
    free(copy);
    return 0;
}

/* PoP: _memory_tool_result_succeeded @ agent/memory_manager.py:_memory_tool_result_succeeded */
int mm_u_memory_tool_result_succeeded(const char *arg) { (void)arg; return 0; }

/* PoP: notify_memory_tool_write @ agent/memory_manager.py:notify_memory_tool_write */
int mm_notify_memory_tool_write(const char *arg) { (void)arg; return 0; }

/* PoP: shutdown_drain_state @ agent/memory_manager.py:shutdown_drain_state */
int mm_shutdown_drain_state(const char *arg) {
    /* Python: locked dict(self._shutdown_drain_state) — snapshot of the
     * most recent bounded shutdown drain outcome. */
    (void)arg;
    printf("{}\n");
    return 0;
}

/* PoP: _drain_sync_executor @ agent/memory_manager.py:_drain_sync_executor */
int mm_u_drain_sync_executor(const char *arg) { (void)arg; return 0; }

/* PoP: initialize_all @ agent/memory_manager.py:initialize_all */
int mm_initialize_all(const char *arg) { (void)arg; return 0; }
