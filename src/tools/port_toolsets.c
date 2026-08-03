/* port_toolsets.c — faithful C11 port of toolsets.py.
 * Static TOOLSETS table + recursive composition resolution.
 */
#define _GNU_SOURCE
#include "toolsets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* registry bridge (src/tools/registry.c) */
extern char **registry_get_tool_names_for_toolset(const char *toolset, size_t *out_n);
extern char **registry_get_registered_toolset_names(size_t *out_n);

/* PoP: hermes_core_tools @ toolsets.py:_HERMES_CORE_TOOLS */
static const char *const HERMES_CORE_TOOLS[] = {
    "web_search", "web_extract",
    "terminal", "process",
    "read_terminal", "close_terminal", "open_preview", "focus_pane",
    "react_to_message",
    "read_file", "write_file", "patch", "search_files",
    "vision_analyze", "image_generate",
    "bfl_flux3_text_to_video", "bfl_flux3_image_to_video",
    "bfl_flux3_keyframes_to_video", "bfl_flux3_video_continuation",
    "bfl_flux3_get_result", "bfl_flux3_prompting_guide",
    "skills_list", "skill_view", "skill_manage",
    "browser_navigate", "browser_snapshot", "browser_click",
    "browser_type", "browser_scroll", "browser_back",
    "browser_press", "browser_get_images",
    "browser_vision", "browser_console", "browser_cdp", "browser_dialog",
    "text_to_speech",
    "todo", "memory",
    "session_search",
    "clarify",
    "execute_code", "delegate_task",
    "cronjob",
    "ha_list_entities", "ha_get_state", "ha_list_services", "ha_call_service",
    "kanban_show", "kanban_list",
    "kanban_complete", "kanban_block", "kanban_heartbeat",
    "kanban_comment", "kanban_create", "kanban_link",
    "kanban_unblock",
    "kanban_attach", "kanban_attach_url", "kanban_attachments",
    "computer_use",
    NULL
};

static const char *const WEBHOOK_SAFE_TOOLS[] = {
    "web_search", "web_extract", "vision_analyze", "clarify", NULL
};

#define TS(...) (const char *const[]){ __VA_ARGS__, NULL }
#define TS_NONE (const char *const[]){ NULL }

/* PoP: toolsets_static_table @ toolsets.py:TOOLSETS */
static const toolset_def_t STATIC_TOOLSETS[] = {
    { "web", "Web research and content extraction tools",
      TS("web_search", "web_extract"), TS_NONE, false },
    { "search", "Web search only (no content extraction/scraping)",
      TS("web_search"), TS_NONE, false },
    { "x_search", "Search X (Twitter) posts and threads via xAI",
      TS("x_search"), TS_NONE, false },
    { "vision", "Image analysis and vision tools",
      TS("vision_analyze"), TS_NONE, false },
    { "video", "Video analysis and understanding tools",
      TS("video_analyze"), TS_NONE, false },
    { "image_gen", "Creative generation tools (images)",
      TS("image_generate"), TS_NONE, false },
    { "video_gen", "Video generation tools",
      TS("video_generate", "xai_video_edit", "xai_video_extend"), TS_NONE, false },
    { "bfl", "FLUX 3 video generation (Black Forest Labs)",
      TS("bfl_flux3_text_to_video", "bfl_flux3_image_to_video",
         "bfl_flux3_keyframes_to_video", "bfl_flux3_video_continuation",
         "bfl_flux3_get_result", "bfl_flux3_prompting_guide"), TS_NONE, false },
    { "computer_use", "Background desktop control via cua-driver",
      TS("computer_use"), TS_NONE, false },
    { "terminal", "Terminal/command execution and process management tools",
      TS("terminal", "process"), TS_NONE, false },
    { "skills", "Access, create, edit, and manage skill documents",
      TS("skills_list", "skill_view", "skill_manage"), TS_NONE, false },
    { "browser", "Browser automation for web interaction",
      TS("browser_navigate", "browser_snapshot", "browser_click",
         "browser_type", "browser_scroll", "browser_back",
         "browser_press", "browser_get_images",
         "browser_vision", "browser_console", "browser_cdp",
         "browser_dialog", "web_search"), TS_NONE, false },
    { "cronjob", "Cronjob management tool",
      TS("cronjob"), TS_NONE, false },
    { "file", "File manipulation tools",
      TS("read_file", "write_file", "patch", "search_files"), TS_NONE, false },
    { "tts", "Text-to-speech",
      TS("text_to_speech"), TS_NONE, false },
    { "todo", "Task planning and tracking for multi-step work",
      TS("todo"), TS_NONE, false },
    { "memory", "Persistent memory across sessions",
      TS("memory"), TS_NONE, false },
    { "context_engine", "Runtime tools exposed by the active context engine",
      TS_NONE, TS_NONE, false },
    { "session_search", "Search and recall past conversations",
      TS("session_search"), TS_NONE, false },
    { "project", "Desktop Projects (GUI sessions only)",
      TS("project_list", "project_create", "project_switch"), TS_NONE, false },
    { "clarify", "Ask the user clarifying questions",
      TS("clarify"), TS_NONE, false },
    { "code_execution", "Run Python scripts that call tools programmatically",
      TS("execute_code"), TS_NONE, false },
    { "delegation", "Spawn subagents with isolated context",
      TS("delegate_task"), TS_NONE, false },
    { "homeassistant", "Home Assistant smart home control and monitoring",
      TS("ha_list_entities", "ha_get_state", "ha_list_services", "ha_call_service"),
      TS_NONE, false },
    { "kanban", "Kanban multi-agent coordination",
      TS("kanban_show", "kanban_list", "kanban_complete", "kanban_block",
         "kanban_heartbeat", "kanban_comment", "kanban_create", "kanban_link",
         "kanban_unblock", "kanban_attach", "kanban_attach_url",
         "kanban_attachments"), TS_NONE, false },
    { "discord", "Discord read and participate tools",
      TS("discord"), TS_NONE, false },
    { "discord_admin", "Discord server management",
      TS("discord_admin"), TS_NONE, false },
    { "yuanbao", "Yuanbao platform tools",
      TS("yb_query_group_info", "yb_query_group_members", "yb_send_dm",
         "yb_search_sticker", "yb_send_sticker"), TS_NONE, false },
    { "feishu_doc", "Read Feishu/Lark document content",
      TS("feishu_doc_read"), TS_NONE, false },
    { "feishu_drive", "Feishu/Lark document comment operations",
      TS("feishu_drive_list_comments", "feishu_drive_list_comment_replies",
         "feishu_drive_reply_comment", "feishu_drive_add_comment"), TS_NONE, false },
    { "spotify", "Native Spotify playback, search, playlist, album, and library tools",
      TS("spotify_playback", "spotify_devices", "spotify_queue", "spotify_search",
         "spotify_playlists", "spotify_albums", "spotify_library"), TS_NONE, false },
    { "debugging", "Debugging and troubleshooting toolkit",
      TS("terminal", "process"), TS("web", "file"), false },
    { "safe", "Safe toolkit without terminal access",
      TS_NONE, TS("web", "vision", "image_gen"), false },
    { "coding", "Coding-focused toolset",
      TS("web_search", "web_extract",
         "terminal", "process", "read_terminal", "close_terminal",
         "read_file", "write_file", "patch", "search_files",
         "vision_analyze",
         "skills_list", "skill_view", "skill_manage",
         "browser_navigate", "browser_snapshot", "browser_click",
         "browser_type", "browser_scroll", "browser_back",
         "browser_press", "browser_get_images",
         "browser_vision", "browser_console", "browser_cdp", "browser_dialog",
         "todo", "memory",
         "session_search", "clarify",
         "execute_code", "delegate_task"), TS_NONE, true },
    { "hermes-acp", "Editor integration",
      TS("web_search", "web_extract",
         "terminal", "process",
         "read_file", "write_file", "patch", "search_files",
         "vision_analyze",
         "skills_list", "skill_view", "skill_manage",
         "browser_navigate", "browser_snapshot", "browser_click",
         "browser_type", "browser_scroll", "browser_back",
         "browser_press", "browser_get_images",
         "browser_vision", "browser_console", "browser_cdp", "browser_dialog",
         "todo", "memory",
         "session_search",
         "execute_code", "delegate_task"), TS_NONE, false },
    { "hermes-api-server", "OpenAI-compatible API server",
      TS("web_search", "web_extract",
         "terminal", "process",
         "read_file", "write_file", "patch", "search_files",
         "vision_analyze", "image_generate",
         "skills_list", "skill_view", "skill_manage",
         "browser_navigate", "browser_snapshot", "browser_click",
         "browser_type", "browser_scroll", "browser_back",
         "browser_press", "browser_get_images",
         "browser_vision", "browser_console", "browser_cdp", "browser_dialog",
         "todo", "memory",
         "session_search",
         "execute_code", "delegate_task",
         "cronjob",
         "ha_list_entities", "ha_get_state", "ha_list_services",
         "ha_call_service"), TS_NONE, false },
    /* CORE-BUNDLE marker: entries below use HERMES_CORE_TOOLS at init */
    { "hermes-cli", "Full interactive CLI toolset", NULL, TS_NONE, false },
    { "hermes-cron", "Default cron toolset", NULL, TS_NONE, false },
    { "hermes-telegram", "Telegram bot toolset", NULL, TS_NONE, false },
    { "hermes-whatsapp", "WhatsApp bot toolset", NULL, TS_NONE, false },
    { "hermes-slack", "Slack bot toolset", NULL, TS_NONE, false },
    { "hermes-signal", "Signal bot toolset", NULL, TS_NONE, false },
    { "hermes-bluebubbles", "BlueBubbles iMessage bot toolset", NULL, TS_NONE, false },
    { "hermes-homeassistant", "Home Assistant bot toolset", NULL, TS_NONE, false },
    { "hermes-email", "Email bot toolset", NULL, TS_NONE, false },
    { "hermes-mattermost", "Mattermost bot toolset", NULL, TS_NONE, false },
    { "hermes-matrix", "Matrix bot toolset", NULL, TS_NONE, false },
    { "hermes-dingtalk", "DingTalk bot toolset", NULL, TS_NONE, false },
    { "hermes-weixin", "Weixin bot toolset", NULL, TS_NONE, false },
    { "hermes-qqbot", "QQBot toolset", NULL, TS_NONE, false },
    { "hermes-wecom", "WeCom bot toolset", NULL, TS_NONE, false },
    { "hermes-wecom-callback", "WeCom callback toolset", NULL, TS_NONE, false },
    { "hermes-sms", "SMS bot toolset", NULL, TS_NONE, false },
    { "hermes-discord", "Discord bot toolset", NULL /* core + extras */, TS_NONE, false },
    { "hermes-feishu", "Feishu/Lark bot toolset", NULL, TS_NONE, false },
    { "hermes-yuanbao", "Yuanbao Bot toolset", NULL, TS_NONE, false },
    { "hermes-webhook", "Webhook toolset", WEBHOOK_SAFE_TOOLS, TS_NONE, false },
    { "hermes-gateway", "Gateway toolset - union of all messaging platform tools",
      TS_NONE,
      TS("hermes-telegram", "hermes-discord", "hermes-whatsapp", "hermes-slack",
         "hermes-signal", "hermes-bluebubbles", "hermes-homeassistant",
         "hermes-email", "hermes-sms", "hermes-mattermost", "hermes-matrix",
         "hermes-dingtalk", "hermes-feishu", "hermes-wecom",
         "hermes-wecom-callback", "hermes-weixin", "hermes-qqbot",
         "hermes-webhook", "hermes-yuanbao"), false },
};
#define N_STATIC (sizeof(STATIC_TOOLSETS) / sizeof(STATIC_TOOLSETS[0]))

/* Platform-extra tools for core bundles whose static tools = CORE + extras. */
static const char *const DISCORD_EXTRAS[] = { "discord", "discord_admin", NULL };
static const char *const FEISHU_EXTRAS[] = {
    "feishu_doc_read", "feishu_drive_list_comments",
    "feishu_drive_list_comment_replies", "feishu_drive_reply_comment",
    "feishu_drive_add_comment", NULL };
static const char *const YUANBAO_EXTRAS[] = {
    "yb_query_group_info", "yb_query_group_members", "yb_send_dm",
    "yb_search_sticker", "yb_send_sticker", NULL };

static const char *const *bundle_extras(const char *name) {
    if (strcmp(name, "hermes-discord") == 0) return DISCORD_EXTRAS;
    if (strcmp(name, "hermes-feishu") == 0) return FEISHU_EXTRAS;
    if (strcmp(name, "hermes-yuanbao") == 0) return YUANBAO_EXTRAS;
    return NULL;
}

/* Bundles whose tools == _HERMES_CORE_TOOLS (+ extras). */
static bool is_core_bundle(const toolset_def_t *d) {
    return d->tools == NULL || d->tools == HERMES_CORE_TOOLS;
}

/* ── Custom toolsets (create_custom_toolset) ─────────────────────────── */
typedef struct custom_ts {
    char *name; char *description;
    char **tools; size_t n_tools;
    char **includes; size_t n_includes;
    struct custom_ts *next;
} custom_ts_t;
static custom_ts_t *g_custom = NULL;
static toolset_def_t g_custom_view; /* scratch view for get_static */
static const char *g_custom_tools_buf[128];
static const char *g_custom_incl_buf[32];

/* PoP: toolsets_create_custom @ toolsets.py:create_custom_toolset */
void toolsets_create_custom(const char *name, const char *description,
                            const char *const *tools, size_t n_tools,
                            const char *const *includes, size_t n_includes) {
    if (!name) return;
    custom_ts_t *c = calloc(1, sizeof(*c));
    c->name = strdup(name);
    c->description = strdup(description ? description : "");
    c->tools = calloc(n_tools ? n_tools : 1, sizeof(char *));
    for (size_t i = 0; i < n_tools; i++) c->tools[i] = strdup(tools[i]);
    c->n_tools = n_tools;
    c->includes = calloc(n_includes ? n_includes : 1, sizeof(char *));
    for (size_t i = 0; i < n_includes; i++) c->includes[i] = strdup(includes[i]);
    c->n_includes = n_includes;
    c->next = g_custom;
    g_custom = c; /* like Python dict assignment: newest wins on lookup */
}

const char *const *toolsets_core_tools(void) { return HERMES_CORE_TOOLS; }

/* PoP: toolsets_get_static @ toolsets.py:get_toolset */
const toolset_def_t *toolsets_get_static(const char *name) {
    if (!name) return NULL;
    for (custom_ts_t *c = g_custom; c; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            size_t nt = c->n_tools < 127 ? c->n_tools : 127;
            for (size_t i = 0; i < nt; i++) g_custom_tools_buf[i] = c->tools[i];
            g_custom_tools_buf[nt] = NULL;
            size_t ni = c->n_includes < 31 ? c->n_includes : 31;
            for (size_t i = 0; i < ni; i++) g_custom_incl_buf[i] = c->includes[i];
            g_custom_incl_buf[ni] = NULL;
            g_custom_view.name = c->name;
            g_custom_view.description = c->description;
            g_custom_view.tools = g_custom_tools_buf;
            g_custom_view.includes = g_custom_incl_buf;
            g_custom_view.posture = false;
            return &g_custom_view;
        }
    }
    for (size_t i = 0; i < N_STATIC; i++)
        if (strcmp(STATIC_TOOLSETS[i].name, name) == 0)
            return &STATIC_TOOLSETS[i];
    return NULL;
}

/* ── sorted-unique string set helper ─────────────────────────────────── */
typedef struct { char **v; size_t n, cap; } strset_t;
static void ss_add(strset_t *s, const char *x) {
    if (!x) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->v = realloc(s->v, s->cap * sizeof(char *));
    }
    s->v[s->n++] = strdup(x);
}
static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static char **ss_finish(strset_t *s, size_t *out_n) {
    qsort(s->v, s->n, sizeof(char *), cmp_str);
    *out_n = s->n;
    return s->v;
}

/* visited-set for cycle detection */
typedef struct { const char *v[256]; size_t n; } visited_t;
static bool visited_has(const visited_t *vs, const char *name) {
    for (size_t i = 0; i < vs->n; i++)
        if (strcmp(vs->v[i], name) == 0) return true;
    return false;
}

static void resolve_into(const char *name, visited_t *vs, strset_t *acc,
                         bool include_registry);

/* PoP: toolsets_get_names @ toolsets.py:get_toolset_names */
char **toolsets_get_names(size_t *out_n) {
    strset_t s = {0};
    for (size_t i = 0; i < N_STATIC; i++) ss_add(&s, STATIC_TOOLSETS[i].name);
    for (custom_ts_t *c = g_custom; c; c = c->next) ss_add(&s, c->name);
    size_t rn = 0;
    char **reg = registry_get_registered_toolset_names(&rn);
    for (size_t i = 0; i < rn; i++) { ss_add(&s, reg[i]); free(reg[i]); }
    free(reg);
    return ss_finish(&s, out_n);
}

static void resolve_into(const char *name, visited_t *vs, strset_t *acc,
                         bool include_registry) {
    if (!name) return;
    /* special aliases: all tools across every toolset */
    if (strcmp(name, "all") == 0 || strcmp(name, "*") == 0) {
        size_t nn = 0;
        char **names = toolsets_get_names(&nn);
        for (size_t i = 0; i < nn; i++) {
            visited_t branch = *vs; /* fresh copy per branch */
            resolve_into(names[i], &branch, acc, include_registry);
        }
        toolsets_free_list(names, nn);
        return;
    }
    if (visited_has(vs, name)) return; /* diamond/cycle */
    if (vs->n < 256) vs->v[vs->n++] = name;

    const toolset_def_t *d = toolsets_get_static(name);
    if (!d) {
        if (include_registry) {
            /* registry/MCP-only toolset: its registered tools */
            size_t rn = 0;
            char **rt = registry_get_tool_names_for_toolset(name, &rn);
            for (size_t i = 0; i < rn; i++) { ss_add(acc, rt[i]); free(rt[i]); }
            free(rt);
            /* plugin platform hermes-<name>: core + registered extras */
            if (rn == 0 && strncmp(name, "hermes-", 7) == 0) {
                size_t pn = 0;
                char **pt = registry_get_tool_names_for_toolset(name + 7, &pn);
                if (pn > 0) {
                    for (size_t i = 0; HERMES_CORE_TOOLS[i]; i++)
                        ss_add(acc, HERMES_CORE_TOOLS[i]);
                    for (size_t i = 0; i < pn; i++) { ss_add(acc, pt[i]); free(pt[i]); }
                }
                free(pt);
            }
        }
        return;
    }

    /* direct tools */
    if (is_core_bundle(d) && strncmp(d->name, "hermes-", 7) == 0 &&
        strcmp(d->name, "hermes-webhook") != 0 &&
        strcmp(d->name, "hermes-gateway") != 0 &&
        strcmp(d->name, "hermes-acp") != 0 &&
        strcmp(d->name, "hermes-api-server") != 0 &&
        d->tools == NULL) {
        for (size_t i = 0; HERMES_CORE_TOOLS[i]; i++)
            ss_add(acc, HERMES_CORE_TOOLS[i]);
        const char *const *ex = bundle_extras(d->name);
        if (ex) for (size_t i = 0; ex[i]; i++) ss_add(acc, ex[i]);
    } else if (d->tools) {
        for (size_t i = 0; d->tools[i]; i++) ss_add(acc, d->tools[i]);
    }
    /* registry-merged tools */
    if (include_registry) {
        size_t rn = 0;
        char **rt = registry_get_tool_names_for_toolset(name, &rn);
        for (size_t i = 0; i < rn; i++) { ss_add(acc, rt[i]); free(rt[i]); }
        free(rt);
    }
    /* includes (shared visited set across siblings) */
    if (d->includes)
        for (size_t i = 0; d->includes[i]; i++)
            resolve_into(d->includes[i], vs, acc, include_registry);
}

/* PoP: toolsets_resolve @ toolsets.py:resolve_toolset */
char **toolsets_resolve(const char *name, bool include_registry,
                        size_t *out_n) {
    strset_t acc = {0};
    visited_t vs = {0};
    resolve_into(name, &vs, &acc, include_registry);
    return ss_finish(&acc, out_n);
}

/* PoP: toolsets_resolve_multiple @ toolsets.py:resolve_multiple_toolsets */
char **toolsets_resolve_multiple(const char *const *names, size_t n_names,
                                 size_t *out_n) {
    strset_t acc = {0};
    for (size_t i = 0; i < n_names; i++) {
        visited_t vs = {0};
        resolve_into(names[i], &vs, &acc, true);
    }
    return ss_finish(&acc, out_n);
}

/* PoP: toolsets_validate @ toolsets.py:validate_toolset */
bool toolsets_validate(const char *name) {
    if (!name) return false;
    if (strcmp(name, "all") == 0 || strcmp(name, "*") == 0) return true;
    if (toolsets_get_static(name)) return true;
    size_t rn = 0;
    char **reg = registry_get_registered_toolset_names(&rn);
    bool found = false;
    for (size_t i = 0; i < rn; i++) {
        if (!found && strcmp(reg[i], name) == 0) found = true;
        free(reg[i]);
    }
    free(reg);
    return found;
}

/* PoP: toolsets_bundle_non_core_tools @ toolsets.py:bundle_non_core_tools */
char **toolsets_bundle_non_core_tools(const char *toolset_name, size_t *out_n) {
    strset_t acc = {0};
    const toolset_def_t *d = toolsets_get_static(toolset_name);
    if (!d) {
        /* unknown: full resolution minus core */
        size_t rn = 0;
        char **rt = toolsets_resolve(toolset_name, true, &rn);
        for (size_t i = 0; i < rn; i++) {
            bool core = false;
            for (size_t j = 0; HERMES_CORE_TOOLS[j]; j++)
                if (strcmp(rt[i], HERMES_CORE_TOOLS[j]) == 0) { core = true; break; }
            if (!core) ss_add(&acc, rt[i]);
        }
        toolsets_free_list(rt, rn);
        return ss_finish(&acc, out_n);
    }
    /* bundle's own non-core tools */
    if (is_core_bundle(d) && d->tools == NULL) {
        const char *const *ex = bundle_extras(d->name);
        if (ex) for (size_t i = 0; ex[i]; i++) ss_add(&acc, ex[i]);
    } else if (d->tools) {
        for (size_t i = 0; d->tools[i]; i++) {
            bool core = false;
            for (size_t j = 0; HERMES_CORE_TOOLS[j]; j++)
                if (strcmp(d->tools[i], HERMES_CORE_TOOLS[j]) == 0) { core = true; break; }
            if (!core) ss_add(&acc, d->tools[i]);
        }
    }
    /* one-level includes */
    if (d->includes) {
        for (size_t i = 0; d->includes[i]; i++) {
            const toolset_def_t *inc = toolsets_get_static(d->includes[i]);
            if (!inc) continue;
            if (is_core_bundle(inc) && inc->tools == NULL) {
                const char *const *ex = bundle_extras(inc->name);
                if (ex) for (size_t j = 0; ex[j]; j++) ss_add(&acc, ex[j]);
            } else if (inc->tools) {
                for (size_t j = 0; inc->tools[j]; j++) {
                    bool core = false;
                    for (size_t k = 0; HERMES_CORE_TOOLS[k]; k++)
                        if (strcmp(inc->tools[j], HERMES_CORE_TOOLS[k]) == 0) { core = true; break; }
                    if (!core) ss_add(&acc, inc->tools[j]);
                }
            }
        }
    }
    return ss_finish(&acc, out_n);
}

void toolsets_free_list(char **list, size_t n) {
    if (!list) return;
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}
/* MARKER-RESOLUTION */
