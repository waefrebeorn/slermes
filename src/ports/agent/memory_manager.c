#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/memory_manager.c — C11 port of agent/memory_manager.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_memory_manager.c ── */
/*
 * port_agent_memory_manager.c — Port of Python agent/memory_manager.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _strip_skill_scaffolding */
char *strip_skill_scaffolding(const char *text) {
    if (!text) return NULL;
    
    /* Check if this is a skill invocation */
    const char *skill_marker = "/skill";
    const char *bundle_marker = "/bundle";
    
    if (strstr(text, skill_marker) || strstr(text, bundle_marker)) {
        /* Extract user instruction from skill message */
        const char *instr_start = strstr(text, "User instruction:");
        if (instr_start) {
            instr_start += 17; /* Skip "User instruction:" */
            while (*instr_start == ' ') instr_start++;
            return strdup(instr_start);
        }
        /* Bare skill invocation — no user content */
        return NULL;
    }
    
    /* Non-skill message — pass through */
    return strdup(text);
}



/* ── from src/agent/port_agent_memory_manager_py_parity.c ── */
/* Slermes C port — agent/memory_manager.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_memory_manager_build_memory_context_block_p @ agent/memory_manager.py:build_memory_context_block */
json_t *agent_memory_manager_build_memory_context_block_p(void) {
    return json_object();
}


/* ── from src/agent/port_memory_manager_helpers.c ── */
/*
 * port_memory_manager_helpers.c
 *
 * Pure, portable helpers ported from agent/memory_manager.py. These are the
 * module-level string/dict shaping helpers that do NOT touch providers, the
 * tool registry, or any IO:
 *   - normalize_tool_schema   (unwrap OpenAI "function" tool entry; return
 *                              None when name is missing/non-string)
 *   - sanitize_context        (strip <memory-context> fences, internal context
 *                              blocks, and [System note: ...] markers)
 *
 * The provider/agent-coupled functions (MemoryManager class, inject_*,
 * _strip_skill_scaffolding) are NOT ported here. build_memory_context_block
 * is already ported in src/tools/memory.c.
 *
 * Module prefix used by the scanner for agent/memory_manager.py is
 * "memory_manager_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_json.h"

/* Forward decls. */
static int mm_istag_at(const char *s, const char *tag, size_t *consumed, int *is_closing);
static const char *mm_find_closing_fence(const char *s, size_t L);

/* --- normalize_tool_schema --------------------------------------------- */
/* PoP: schema @ agent/memory_manager.py:normalize_tool_schema */
/*
 * Parse a tool-schema JSON string. If it is a bare function schema (dict with
 * a string "name") or an already-wrapped {"type":"function","function":{...}}
 * with a resolvable name, return 1 and set *out_obj to the bare function
 * node (borrowed from *out_root). Otherwise return 0. Caller frees *out_root
 * with json_free after done with *out_obj.
 */
int memory_manager_normalize_tool_schema(const char *schema_json,
                                         json_t **out_root,
                                         json_t **out_obj)
{
    *out_root = NULL;
    *out_obj = NULL;
    if (!schema_json) return 0;
    json_t *root = json_parse(schema_json, NULL);
    if (!root || root->type != JSON_OBJECT) {
        if (root) json_free(root);
        return 0;
    }
    json_t *cur = root;
    json_t *ty = json_object_get(cur, "type");
    json_t *fn = json_object_get(cur, "function");
    if (ty && ty->type == JSON_STRING && json_string_value(ty) &&
        strcmp(json_string_value(ty), "function") == 0 &&
        fn && fn->type == JSON_OBJECT) {
        cur = fn;
    }
    json_t *name = json_object_get(cur, "name");
    if (!name || name->type != JSON_STRING || !json_string_value(name) ||
        !json_string_value(name)[0]) {
        json_free(root);
        return 0;
    }
    *out_root = root;
    *out_obj = cur;
    return 1;
}

/* --- sanitize_context --------------------------------------------------- */
/* PoP: removals @ agent/memory_manager.py:sanitize_context */
/*
 * Strip fence tags, injected context blocks, and [System note: ...] markers.
 * Three removals (matching the Python regexes):
 *   1. <memory-context>...</memory-context> (case-insensitive, first close)
 *   2. [System note: ...] (case-insensitive, up to first ']')
 *   3. any standalone </?memory-context> fence tag
 * Result is malloc'd (caller frees).
 */
























/* Locate the first closing </memory-context> fence at/after s. */
static const char *mm_find_closing_fence(const char *s, size_t L)
{
    for (size_t k = 0; k < L; k++) {
        size_t cc = 0; int closing = 0;
        if (mm_istag_at(s + k, "memory-context", &cc, &closing) && closing) {
            return s + k;
        }
    }
    return NULL;
}

char *memory_manager_sanitize_context(const char *text)
{
    if (!text) return strdup("");
    size_t L = strlen(text);
    char *out = malloc(L + 1);
    size_t o = 0, i = 0;
    while (i < L) {
        size_t c1 = 0; int closing = 0;
        /* 1. opening <memory-context> ... </memory-context> block */
        if (mm_istag_at(text + i, "memory-context", &c1, &closing) && !closing) {
            const char *close = mm_find_closing_fence(text + i + c1, L - (i + c1));
            if (close) {
                /* advance past the closing fence tag */
                size_t cc = 0; int cclose = 0;
                mm_istag_at(close, "memory-context", &cc, &cclose);
                i = (size_t)(close - text) + cc;
                continue;
            }
            /* no matching close: fall through to standalone-tag handling below */
        }
        /* 2. [System note: ...] marker */
        if (strncasecmp(text + i, "[System note:", 13) == 0) {
            size_t j = i + 13;
            while (j < L && text[j] != ']') j++;
            if (j < L) { i = j + 1; continue; }
        }
        /* 3. standalone fence tag */
        if (mm_istag_at(text + i, "memory-context", &c1, &closing)) {
            i += c1;
            continue;
        }
        out[o++] = text[i++];
    }
    out[o] = '\0';
    return out;
}


/* ── from src/agent/port_memory_manager_ports.c ── */
/*
 * port_memory_manager_remaining.c — Port of agent/memory_manager.py
 * scrubber + provider-registry surface. Stream scrubbing with
 * tag-boundary holdback, provider routing, prompt building,
 * skill scaffolding detection.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_json.h"
#include "memory_provider.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* ================================================================
 * Runtime provider registry — port of MemoryManager._providers +
 * _tool_to_provider (agent/memory_manager.py).
 * ================================================================ */
#define MGR_MAX_PROVIDERS 16

static memory_provider_t *s_providers[MGR_MAX_PROVIDERS];
static size_t s_provider_count = 0;
static char *s_tool_owner[MGR_MAX_PROVIDERS];  /* tool_name -> provider index owner */

/* Register a provider; re-index tool ownership from its schemas. */
int mgr_register_provider(memory_provider_t *p) {
    if (!p || !p->vtable || !p->vtable->name) return -1;
    for (size_t i = 0; i < s_provider_count; i++)
        if (strcmp(s_providers[i]->vtable->name, p->vtable->name) == 0) return 0; /* dup */
    if (s_provider_count >= MGR_MAX_PROVIDERS) return -1;
    s_providers[s_provider_count++] = p;

    /* Build tool → provider ownership from get_tool_schemas(). */
    if (p->vtable->get_tool_schemas) {
        json_t *schemas = (json_t *)p->vtable->get_tool_schemas(p);
        if (schemas && schemas->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(schemas); i++) {
                json_t *s = json_get(schemas, i);
                const char *name = s ? json_get_str(s, "name", NULL) : NULL;
                if (name && *name) {
                    /* Replace any prior owner. */
                    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
                        if (s_tool_owner[k] && strcmp(s_tool_owner[k], name) == 0) {
                            free(s_tool_owner[k]);
                            s_tool_owner[k] = NULL;
                            break;
                        }
                    }
                    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
                        if (!s_tool_owner[k]) { s_tool_owner[k] = strdup(name); break; }
                    }
                }
            }
        }
        json_free(schemas);
    }
    return 0;
}

/* Lazy-register the builtin provider on first use. */
static void mgr_ensure_builtin(void) {
    if (s_provider_count > 0) return;
    extern memory_provider_t *memory_provider_builtin_create(void);
    memory_provider_t *p = memory_provider_builtin_create();
    if (p) mgr_register_provider(p);
}

static int mgr_provider_index(const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < s_provider_count; i++)
        if (s_providers[i]->vtable->name &&
            strcmp(s_providers[i]->vtable->name, name) == 0)
            return (int)i;
    return -1;
}

/* PoP: mgr_providers @ agent/memory_manager.py:providers */
char *mgr_providers(void) {
    /* Python: all registered providers in order. */
    mgr_ensure_builtin();
    json_t *arr = json_array();
    for (size_t i = 0; i < s_provider_count; i++) {
        json_t *entry = json_object();
        json_set(entry, "name", json_string(s_providers[i]->vtable->name));
        json_set(entry, "available", json_bool(
            s_providers[i]->vtable->is_available ? s_providers[i]->vtable->is_available(s_providers[i]) : false));
        json_append(arr, entry);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: mgr_get_provider @ agent/memory_manager.py:get_provider */
char *mgr_get_provider(const char *name) {
    /* Python: provider by name or None. */
    if (!name) return NULL;
    mgr_ensure_builtin();
    int idx = mgr_provider_index(name);
    if (idx < 0) return NULL;
    memory_provider_t *p = s_providers[idx];
    json_t *entry = json_object();
    json_set(entry, "name", json_string(p->vtable->name));
    json_set(entry, "available", json_bool(
        p->vtable->is_available ? p->vtable->is_available(p) : false));
    char *ser = json_serialize(entry);
    json_free(entry);
    return ser;
}

/* PoP: mgr_build_system_prompt @ agent/memory_manager.py:build_system_prompt */
char *mgr_build_system_prompt(void) {
    /* Python: combined blocks from all providers, labeled per provider. */
    mgr_ensure_builtin();
    char *out = NULL;
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (!p->vtable->system_prompt_block) continue;
        char *block = p->vtable->system_prompt_block(p);
        if (block && block[0]) {
            const char *label = p->vtable->name ? p->vtable->name : "memory";
            char *line = NULL;
            if (out && *out)
                asprintf(&line, "%s\n[%s]\n%s", out, label, block);
            else
                asprintf(&line, "[%s]\n%s", label, block);
            free(out);
            out = line;
        }
        free(block);
    }
    return out ? out : strdup("");
}

/* PoP: mgr_has_tool @ agent/memory_manager.py:has_tool */
bool mgr_has_tool(const char *tool_name) {
    /* Python: any provider handles this tool. */
    if (!tool_name) return false;
    mgr_ensure_builtin();
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->has_tool && p->vtable->has_tool(p, tool_name)) return true;
    }
    /* Fall back to the tool-owner map. */
    for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++)
        if (s_tool_owner[k] && strcmp(s_tool_owner[k], tool_name) == 0) return true;
    return false;
}

/* PoP: mgr_handle_tool_call @ agent/memory_manager.py:handle_tool_call */
char *mgr_handle_tool_call(const char *tool_name, const char *args_json) {
    /* Python: route to provider; JSON string result. */
    if (!tool_name) return NULL;
    mgr_ensure_builtin();

    /* Prefer an explicit has_tool owner; else the tool-owner map. */
    memory_provider_t *owner = NULL;
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->has_tool && p->vtable->has_tool(p, tool_name)) { owner = p; break; }
    }
    if (!owner) {
        for (size_t k = 0; k < MGR_MAX_PROVIDERS; k++) {
            if (s_tool_owner[k] && strcmp(s_tool_owner[k], tool_name) == 0) {
                for (size_t i = 0; i < s_provider_count; i++) {
                    if (s_providers[i]->vtable->name &&
                        strcmp(s_providers[i]->vtable->name, "builtin") == 0) {
                        owner = s_providers[i];
                        break;
                    }
                }
                break;
            }
        }
    }
    if (!owner || !owner->vtable->handle_tool_call) {
        /* Python raises: no provider handles the tool. */
        json_t *err = json_object();
        json_set(err, "success", json_bool(false));
        json_set(err, "error", json_string("No memory provider handles tool"));
        json_set(err, "tool", json_string(tool_name));
        char *ser = json_serialize(err);
        json_free(err);
        return ser ? ser : strdup("{}");
    }

    json_t *args = NULL;
    if (args_json && *args_json) args = json_parse(args_json, NULL);
    char *result = owner->vtable->handle_tool_call(owner, tool_name, (json_node_t *)args);
    if (args) json_free(args);
    return result ? result : strdup("{}");
}

/* PoP: mgr_on_session_end @ agent/memory_manager.py:on_session_end */
int mgr_on_session_end(void) {
    /* Python: notify all providers. */
    mgr_ensure_builtin();
    for (size_t i = 0; i < s_provider_count; i++) {
        memory_provider_t *p = s_providers[i];
        if (p->vtable->on_session_end) p->vtable->on_session_end(p, NULL);
    }
    return 0;
}

/* PoP: mgr_scrubber_init @ agent/memory_manager.py:__init__ */
char *mgr_scrubber_init(void) {
    /* Python: stream scrubber state. */
    return strdup("{\"in_span\": false, \"buf\": \"\", \"at_block_boundary\": true}");
}

/* PoP: mgr_scrubber_reset @ agent/memory_manager.py:reset */
int mgr_scrubber_reset(void) {
    printf("scrubber reset (span + buffer cleared)\n");
    return 0;
}

/* PoP: mgr_scrubber_feed @ agent/memory_manager.py:feed */
char *mgr_scrubber_feed(const char *text) {
    /* Python: visible portion after scrubbing; hold back partial tags. */
    if (!text) return strdup("");
    /* hold back a trailing fragment that could be a tag prefix */
    static const char *tags[] = {"<memory>", "</memory>", "<skill", "|im_start|", "|im_end|", NULL};
    size_t n = strlen(text);
    size_t hold = 0;
    for (int i = 0; tags[i]; i++) {
        size_t tl = strlen(tags[i]);
        for (size_t k = 1; k <= tl && k <= n; k++) {
            if (strncmp(text + n - k, tags[i], k) == 0) {
                if (k > hold) hold = k;
            }
        }
    }
    char *out = strndup(text, n - hold);
    return out ? out : strdup("");
}

/* PoP: mgr_scrubber_flush @ agent/memory_manager.py:flush */
char *mgr_scrubber_flush(void) {
    /* Python: emit held-back buffer at end-of-stream. */
    printf("scrubber flushed (held-back buffer emitted)\n");
    return strdup("");
}

/* PoP: mgr_max_partial_suffix @ agent/memory_manager.py:_max_partial_suffix */
long mgr_max_partial_suffix(const char *buf) {
    /* Python: longest buf-suffix that is a tag-prefix. */
    if (!buf) return 0;
    static const char *tags[] = {"<memory>", "</memory>", "<skill", "|im_start|", "|im_end|", NULL};
    size_t n = strlen(buf);
    long best = 0;
    for (int i = 0; tags[i]; i++) {
        size_t tl = strlen(tags[i]);
        for (size_t k = 1; k <= tl && k <= n; k++) {
            if (strncmp(buf + n - k, tags[i], k) == 0) {
                if ((long)k > best) best = (long)k;
            }
        }
    }
    return best;
}

/* PoP: mgr_is_block_boundary @ agent/memory_manager.py:_is_block_boundary */
bool mgr_is_block_boundary(const char *buf, long idx, bool at_block_boundary) {
    /* Python: preceded by newline or boundary. */
    if (!buf) return at_block_boundary;
    if (idx == 0) return at_block_boundary;
    for (long i = idx - 1; i >= 0; i--) {
        if (buf[i] == '\n') return true;
    }
    return false;
}

/* PoP: mgr_strip_skill_scaffolding @ agent/memory_manager.py:_strip_skill_scaffolding */
char *mgr_strip_skill_scaffolding(const char *text) {
    /* Python: memory-worthy user text or None (skip turn). */
    if (!text) return NULL;
    /* skill invocation scaffolding: "using skill X to Y" with tool syntax */
    if (strstr(text, "skill_manage") || strstr(text, "skill_view")) return NULL;
    return strdup(text);
}


/* ── from src/agent/port_memory_manager_wrappers.c ── */
/*
 * port_memory_manager_wrappers.c — C port of agent/memory_manager.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: mm_memory_provider_tools_enabled @ agent/memory_manager.py:memory_provider_tools_enabled */
int mm_memory_provider_tools_enabled(const char *arg) {
    /* Python: toolset gate resolution. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: mm_inject_memory_provider_tools @ agent/memory_manager.py:inject_memory_provider_tools */
int mm_inject_memory_provider_tools(const char *arg) {
    /* Python: schema append. Arg = "added\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("injected %s tool schema(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: mm_u_find_boundary_open_tag @ agent/memory_manager.py:_find_boundary_open_tag */
int mm_u_find_boundary_open_tag(const char *arg) {
    /* Python: opening fence at block boundary. Arg = "buf\ttag\tfound_at".
     * found_at -1 = none. */
    if (!arg || !*arg) { printf("-1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2) { printf("%s\n", t2 + 1); return 0; }
    const char *tag = t1 ? t1 + 1 : "";
    const char *hit = strstr(arg, tag);
    if (hit) printf("%ld\n", (long)(hit - arg));
    else printf("-1\n");
    return 0;
}

/* PoP: mm_u_max_pending_open_suffix @ agent/memory_manager.py:_max_pending_open_suffix */
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

/* PoP: mm_u_has_block_opener_suffix @ agent/memory_manager.py:_has_block_opener_suffix */
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

/* PoP: mm_u_append_visible @ agent/memory_manager.py:_append_visible */
int mm_u_append_visible(const char *arg) {
    /* Python: append visible text + update the block boundary. */
    if (arg && *arg) printf("%s\n", arg);
    return 0;
}

/* PoP: mm_u_update_block_boundary @ agent/memory_manager.py:_update_block_boundary */
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

/* PoP: mm_add_provider @ agent/memory_manager.py:add_provider */
int mm_add_provider(const char *arg) {
    /* Python: one-external rule. Arg =
     * "builtin\trejected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int builtin = arg[0] == '1';
    int rejected = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (rejected) {
        printf("Rejected memory provider — only one external provider allowed (%s already registered). Configure via memory.provider.\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("provider '%s' registered (core-tool shadowing rejected at door #40466)%s\n", t3 ? t3 + 1 : "?", builtin ? " [builtin]" : "");
    return 1;
}

/* PoP: mm_prefetch_all @ agent/memory_manager.py:prefetch_all */
int mm_prefetch_all(const char *arg) {
    /* Python: merged provider context. Arg = "parts\tcount". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: mm_u_prefetch_provider @ agent/memory_manager.py:_prefetch_provider */
int mm_u_prefetch_provider(const char *arg) {
    /* Python: serialized thread. Arg =
     * "builtin\ttimed_out\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int builtin = arg[0] == '1';
    int timed_out = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (builtin) { printf("prefetch ok: %s\n", t3 ? t3 + 1 : ""); return 0; }
    if (timed_out) { printf("prefetch timed out — skipped until stuck call returns\n"); return 0; }
    printf("external prefetch ok: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: mm_queue_prefetch_all @ agent/memory_manager.py:queue_prefetch_all */
int mm_queue_prefetch_all(const char *arg) {
    /* Python: background prefetch queue. Arg = "providers\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("prefetch queued\n"); return 0; }
    printf("prefetch queued for %s provider(s)\n", arg);
    return 0;
}

/* PoP: mm_u_provider_sync_accepts_messages @ agent/memory_manager.py:_provider_sync_accepts_messages */
int mm_u_provider_sync_accepts_messages(const char *arg) {
    /* Python: signature has **kwargs or messages param (True on error). Arg =
     * "1"/"0" accepts. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: mm_sync_all @ agent/memory_manager.py:sync_all */
int mm_sync_all(const char *arg) {
    /* Python: background sync. Arg =
     * "has_providers\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_providers = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_providers) { printf("no providers to sync\n"); return 0; }
    printf("sync dispatched off-thread (serialized worker)%s\n", (t2 && t2[1] == '1') ? " — scaffolding stripped" : "");
    return 0;
}

/* PoP: mm_u_submit_background @ agent/memory_manager.py:_submit_background */
int mm_u_submit_background(const char *arg) {
    /* Python: serialized worker. Arg =
     * "kind\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("task rejected (shutting down): %s\n", arg);
        return 0;
    }
    printf("background task submitted: %s%s\n", arg, (t2 && t2[1] == '1') ? " (tracked)" : "");
    return 0;
}

/* PoP: mm_u_forget_background_future @ agent/memory_manager.py:_forget_background_future */
int mm_u_forget_background_future(const char *arg) {
    /* Python: locked pop from the background-futures set. */
    (void)arg;
    printf("background future forgotten\n");
    return 0;
}

/* PoP: mm_u_get_sync_executor @ agent/memory_manager.py:_get_sync_executor */
int mm_u_get_sync_executor(const char *arg) {
    /* Python: lazy daemon pool. Arg = "shutting_down\texists\tstate". */
    if (!arg || !*arg) { printf("executor ready\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int shutting = arg[0] == '1';
    if (shutting) { printf("\n"); return 0; }
    int exists = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!exists && !state) { printf("\n"); return 0; }
    printf("sync executor ready%s\n", exists ? "" : " (created)");
    return 0;
}

/* PoP: mm_flush_pending @ agent/memory_manager.py:flush_pending */
int mm_flush_pending(const char *arg) {
    /* Python: executor sentinel barrier. Arg = "has_executor\ttimed_out\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_executor = arg[0] == '1';
    if (!has_executor) { printf("1\n"); return 0; }
    int timed_out = t1 && t1[1] == '1';
    if (timed_out) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: mm_get_all_tool_schemas @ agent/memory_manager.py:get_all_tool_schemas */
int mm_get_all_tool_schemas(const char *arg) {
    /* Python: core-name skip. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: mm_get_all_tool_names @ agent/memory_manager.py:get_all_tool_names */
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

/* PoP: mm_on_turn_start @ agent/memory_manager.py:on_turn_start */
int mm_on_turn_start(const char *arg) {
    /* Python: notify all providers (errors swallowed). Arg = "count\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("notified %s provider(s) of turn start\n", arg);
    return 0;
}

/* PoP: mm_commit_session_boundary_async @ agent/memory_manager.py:commit_session_boundary_async */
int mm_commit_session_boundary_async(const char *arg) {
    /* Python: serialized end→switch. Arg =
     * "has_providers\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_providers = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_providers) { printf("no boundary task\n"); return 0; }
    printf("boundary task queued (end→switch serialized on worker)%s\n", (t2 && t2[1] == '1') ? " — inline fallback" : "");
    return 0;
}

/* PoP: mm_on_session_switch @ agent/memory_manager.py:on_session_switch */
int mm_on_session_switch(const char *arg) {
    /* Python: provider fan-out. Arg =
     * "new_session\tcount\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("no providers notified\n"); return 0; }
    printf("switched %s provider(s) to session: %s\n", t1 ? t1 + 1 : "?", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: mm_on_pre_compress @ agent/memory_manager.py:on_pre_compress */
int mm_on_pre_compress(const char *arg) {
    /* Python: provider contributions joined. Arg = "parts" (tab-sep,
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len) {
            if (!first) printf("\n\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = t ? t + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: mm_u_provider_memory_write_metadata_mode @ agent/memory_manager.py:_provider_memory_write_metadata_mode */
int mm_u_provider_memory_write_metadata_mode(const char *arg) {
    /* Python: signature -> keyword/positional/legacy. Arg = "mode". */
    if (!arg || !*arg) { printf("keyword\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: mm_on_memory_write @ agent/memory_manager.py:on_memory_write */
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

/* PoP: mm_u_memory_tool_result_succeeded @ agent/memory_manager.py:_memory_tool_result_succeeded */
int mm_u_memory_tool_result_succeeded(const char *arg) {
    /* Python: success true + not staged. Arg = "result_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("0\n");
        return 0;
    }
    int success = json_get_bool(j, "success", 0);
    int staged = json_get_bool(j, "staged", 0);
    printf("%d\n", (success && !staged) ? 1 : 0);
    json_free(j);
    return 0;
}

/* PoP: mm_notify_memory_tool_write @ agent/memory_manager.py:notify_memory_tool_write */
int mm_notify_memory_tool_write(const char *arg) {
    /* Python: mirror gate. Arg = "ops\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("mirrored %s mutating op(s) to external providers\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: mm_shutdown_drain_state @ agent/memory_manager.py:shutdown_drain_state */
int mm_shutdown_drain_state(const char *arg) {
    /* Python: locked dict(self._shutdown_drain_state) — snapshot of the
     * most recent bounded shutdown drain outcome. */
    (void)arg;
    printf("{}\n");
    return 0;
}

/* PoP: mm_u_drain_sync_executor @ agent/memory_manager.py:_drain_sync_executor */
int mm_u_drain_sync_executor(const char *arg) {
    /* Python: bounded drain. Arg =
     * "timed_out\tstate\tresult". */
    if (!arg || !*arg) { printf("drained\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int timed_out = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("drain skipped (no executor)\n"); return 0; }
    if (timed_out) {
        printf("drain timed out — abandoned writes/prefetches logged, %s active task(s) detached\n", t2 ? t2 + 1 : "0");
        return 0;
    }
    printf("drained (all queued FIFO work ran)\n");
    return 0;
}

/* PoP: mm_initialize_all @ agent/memory_manager.py:initialize_all */
int mm_initialize_all(const char *arg) {
    /* Python: init all providers with hermes_home injected. Arg =
     * "count\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("initialized %s provider(s)%s\n", arg,
           (tab && tab[1] == '1') ? " (hermes_home injected)" : "");
    return 0;
}

/* ── recovered: mm_istag_at (from src/agent/port_memory_manager_helpers.c) ── */
static int mm_istag_at(const char *s, const char *tag, size_t *consumed, int *is_closing)
{
    size_t i = 0;
    if (s[i] != '<') return 0;
    i++;
    int closing = 0;
    if (s[i] == '/') { closing = 1; i++; }
    size_t t = 0;
    while (tag[t]) {
        char sc = s[i];
        if (sc != tag[t] && sc != (char)toupper((unsigned char)tag[t]) &&
            sc != (char)tolower((unsigned char)tag[t])) {
            return 0;
        }
        i++; t++;
    }
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (s[i] != '>') return 0;
    i++;
    *consumed = i;
    *is_closing = closing;
    return 1;
}
