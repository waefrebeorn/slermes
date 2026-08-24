/*
 * hermes_cli_agent_b2.c — HermesCLI batch 2 methods (cli.py class HermesCLI).
 * Retry wave: config MCP watch, destructive-skip splitter, tool edit
 * snapshots, voice STT config accessors, beeps + voice-mode disable.
 */
#include "cli_hermes_cli.h"
#include "hermes_json.h"
#include "hermes_display.h"
#include "port_config_py_helpers.h"
#include "truthy.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Local env-expand used by _preserve_env_ref_templates (config_py_pure.c). */
char *config_expand_env_vars_local(const char *input);

static double b2_now_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ── hcli_check_config_mcp_changes ──────────────────────────────────────── */

/* Recursively expand ${VAR} templates in string values, in place. */
static void b2_expand_env_strings(json_t *node)
{
    if (!node) return;
    switch (node->type) {
    case JSON_STRING: {
        char *exp = config_expand_env_vars_local(node->str_val ? node->str_val : "");
        if (exp) {
            free(node->str_val);
            node->str_val = exp;
        }
        break;
    }
    case JSON_ARRAY:
        for (size_t i = 0; i < node->c.count; i++)
            b2_expand_env_strings(node->c.items[i]);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < node->c.count; i++)
            b2_expand_env_strings(node->c.items[i]);
        break;
    default:
        break;
    }
}

/* PoP: hcli_check_config_mcp_changes @ cli.py:_check_config_mcp_changes */
void hcli_check_config_mcp_changes(hcli_t *self)
{
    if (!self) return;

    const double CONFIG_WATCH_INTERVAL = 5.0; /* seconds between stat() calls */

    double now = b2_now_monotonic();
    if (now - self->last_config_check < CONFIG_WATCH_INTERVAL)
        return;
    self->last_config_check = now;

    char cfg_path[4096];
    config_py_get_config_path(cfg_path, sizeof(cfg_path));
    if (!cfg_path[0]) return;

    struct stat st;
    if (stat(cfg_path, &st) != 0) return;   /* cfg_path missing */

    double mtime = (double)st.st_mtim.tv_sec + (double)st.st_mtim.tv_nsec / 1e9;
    if (mtime == self->config_mtime)
        return;                              /* File unchanged — fast path */

    /* File changed — check whether mcp_servers section changed */
    self->config_mtime = mtime;

    char *raw = NULL;
    long len = 0;
    FILE *f = fopen(cfg_path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return; }
    raw = malloc((size_t)len + 1);
    if (!raw) { fclose(f); return; }
    size_t got = fread(raw, 1, (size_t)len, f);
    raw[got] = '\0';
    fclose(f);

    json_t *new_cfg = json_parse_yaml_real(raw);
    free(raw);
    if (!new_cfg) return;                    /* yaml parse failure → return */

    json_t *new_mcp = json_obj_get(new_cfg, "mcp_servers");
    if (!new_mcp || new_mcp->type == JSON_NULL)
        new_mcp = NULL;
    else
        new_mcp = json_copy(new_mcp);        /* detach before mutating */
    if (new_mcp) {
        /* Expand ${VAR} templates so the comparison matches the init
         * snapshot (_config_mcp_servers), which was loader-expanded.
         * Without this any save_config_value() rewrite triggers a
         * false-positive reload on still-templated raw yaml. */
        b2_expand_env_strings(new_mcp);
    }

    char *new_mcp_json = new_mcp ? json_serialize(new_mcp) : NULL;
    const char *snap = self->config_mcp_servers_json;
    bool unchanged = ((!new_mcp_json || !new_mcp_json[0]) && (!snap || !snap[0])) ||
                     (new_mcp_json && snap && !strcmp(new_mcp_json, snap));
    if (unchanged) {
        free(new_mcp_json);
        if (new_mcp) json_free(new_mcp);
        json_free(new_cfg);
        return;   /* mcp_servers unchanged (some other section was edited) */
    }

    /* auto_reload_on_config_change toggle — read from the just-parsed config
     * so the user can flip it in the same edit that changes mcp_servers.
     * Missing key means default-on. */
    json_t *mcp_cfg = json_obj_get(new_cfg, "mcp");
    bool auto_reload = true;
    if (mcp_cfg && mcp_cfg->type == JSON_OBJECT) {
        json_t *flag = json_obj_get(mcp_cfg, "auto_reload_on_config_change");
        if (flag) {
            if (flag->type == JSON_BOOL) auto_reload = flag->bool_val;
            else if (flag->type == JSON_STRING && flag->str_val)
                auto_reload = is_truthy_value(flag->str_val, true);
            else if (flag->type == JSON_NUMBER) auto_reload = flag->num_val != 0.0;
        }
    }

    /* Snapshot the new section. */
    free(self->config_mcp_servers_json);
    self->config_mcp_servers_json = new_mcp_json;   /* may be NULL */
    if (new_mcp) json_free(new_mcp);

    if (!auto_reload) {
        /* Notify but do NOT auto-reload — user applies via /reload-mcp,
         * which we warn invalidates the provider prompt cache. */
        printf("\n");
        printf("🔄 MCP server config changed — reload skipped (auto-reload disabled).\n");
        printf("   New settings are NOT applied yet. To apply them now, run:\n");
        printf("     /reload-mcp\n");
        printf("   ⚠️  Note: /reload-mcp rebuilds the tool set and invalidates the\n");
        printf("   provider prompt cache (next message re-sends full input tokens).\n");
        fflush(stdout);
    } else if (self->reload_mcp) {
        printf("\n");
        printf("🔄 MCP server config changed — reloading connections...\n");
        fflush(stdout);
        /* Python runs _reload_mcp on a daemon thread with no join so a hung
         * MCP server cannot freeze process_loop; C side has no thread seam,
         * so invoke through the callback (best-effort by contract). */
        self->reload_mcp(self);
    }
    json_free(new_cfg);
}

/* ── hcli_split_destructive_skip ────────────────────────────────────────── */

/* PoP: hcli_split_destructive_skip @ cli.py:_split_destructive_skip */
bool hcli_split_destructive_skip(const char *cmd_text, char **out_remainder)
{
    if (out_remainder) *out_remainder = NULL;
    if (!out_remainder) return false;

    char *rem = strdup("");
    if (!rem) return false;
    *out_remainder = rem;
    if (!cmd_text || !cmd_text[0]) return false;

    /* tokenize on whitespace */
    size_t n = strlen(cmd_text);
    char *buf = malloc(n + 1);
    if (!buf) { *out_remainder = NULL; free(rem); return false; }
    memcpy(buf, cmd_text, n + 1);

    char **toks = malloc(sizeof(char *) * (n / 2 + 2));
    if (!toks) { free(buf); *out_remainder = NULL; free(rem); return false; }
    size_t tcount = 0;
    for (char *p = strtok(buf, " \t\r\n"); p; p = strtok(NULL, " \t\r\n"))
        toks[tcount++] = p;

    if (tcount == 0) { free(toks); free(buf); return false; }

    /* Drop leading "/cmd" word — callers pass the full command text. */
    size_t start = (toks[0][0] == '/') ? 1 : 0;

    char *kept = malloc(n + 2);
    if (!kept) { free(toks); free(buf); *out_remainder = NULL; free(rem); return false; }
    kept[0] = '\0';
    bool skip = false;
    for (size_t i = start; i < tcount; i++) {
        char low[64];
        snprintf(low, sizeof(low), "%s", toks[i]);
        for (char *q = low; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (!strcmp(low, "now") || !strcmp(low, "--yes") || !strcmp(low, "-y")) {
            skip = true;
            continue;
        }
        if (kept[0]) strcat(kept, " ");
        strcat(kept, toks[i]);
    }

    free(*out_remainder);
    *out_remainder = kept;   /* transfer ownership */
    free(toks);
    free(buf);
    return skip;
}

/* ── tool edit snapshots ────────────────────────────────────────────────── */

static ssize_t b2_snap_find(hcli_t *self, const char *tool_call_id)
{
    for (size_t i = 0; i < self->pending_edit_snap_count; i++)
        if (self->pending_edit_snaps[i].id &&
            !strcmp(self->pending_edit_snaps[i].id, tool_call_id))
            return (ssize_t)i;
    return -1;
}

/* PoP: hcli_on_tool_start @ cli.py:_on_tool_start */
void hcli_on_tool_start(hcli_t *self, const char *tool_call_id,
                        const char *function_name, const char *function_args_json)
{
    if (!self || !tool_call_id || !function_name) return;

    /* capture_local_edit_snapshot returns None when nothing to track */
    display_local_edit_snapshot_t *snapshot =
        display_capture_local_edit_snapshot(function_name,
                                            function_args_json ? function_args_json : "{}");
    if (!snapshot) return;   /* try/except + None → debug-only */

    /* Replace any stale entry for this id, else append. */
    ssize_t idx = b2_snap_find(self, tool_call_id);
    if (idx >= 0) {
        display_snapshot_free((display_local_edit_snapshot_t *)
                              self->pending_edit_snaps[idx].snap);
        self->pending_edit_snaps[idx].snap = snapshot;
        return;
    }
    hcli_pending_snap_t *grown =
        realloc(self->pending_edit_snaps,
                sizeof(hcli_pending_snap_t) * (self->pending_edit_snap_count + 1));
    if (!grown) { display_snapshot_free(snapshot); return; }   /* OOM → drop */
    self->pending_edit_snaps = grown;
    self->pending_edit_snaps[self->pending_edit_snap_count].id = strdup(tool_call_id);
    self->pending_edit_snaps[self->pending_edit_snap_count].snap = snapshot;
    self->pending_edit_snap_count++;
}

/* PoP: hcli_on_tool_complete @ cli.py:_on_tool_complete */
void hcli_on_tool_complete(hcli_t *self, const char *tool_call_id,
                           const char *function_name, const char *function_args_json,
                           const char *function_result)
{
    if (!self || !function_name) return;
    const char *result = function_result ? function_result : "";

    /* Background delegate_task notice: dispatched+background → say once that
     * work continues off-thread so the idle prompt doesn't look dead. */
    if (!strcmp(function_name, "delegate_task")) {
        json_t *parsed = json_parse(result, NULL);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_t *status = json_obj_get(parsed, "status");
            json_t *mode = json_obj_get(parsed, "mode");
            if (status && status->type == JSON_STRING &&
                !strcmp(status->str_val, "dispatched") &&
                mode && mode->type == JSON_STRING &&
                !strcmp(mode->str_val, "background")) {
                long n = 1;
                json_t *cnt = json_obj_get(parsed, "count");
                if (cnt && cnt->type == JSON_NUMBER && cnt->num_val >= 1)
                    n = (long)cnt->num_val;
                char line[256];
                if (n == 1)
                    snprintf(line, sizeof(line),
                             "\033[2m⧉ Background task running — I'll resume when it finishes. Keep chatting.\033[0m");
                else
                    snprintf(line, sizeof(line),
                             "\033[2m⧉ Background %ld tasks running — I'll resume when they finish. Keep chatting.\033[0m", n);
                printf("%s\n", line);
                fflush(stdout);
            }
        }
        if (parsed) json_free(parsed);
    }

    /* Pop pending snapshot (dict.pop with default None). */
    display_local_edit_snapshot_t *snapshot = NULL;
    ssize_t idx = tool_call_id ? b2_snap_find(self, tool_call_id) : -1;
    if (idx >= 0) {
        snapshot = (display_local_edit_snapshot_t *)self->pending_edit_snaps[idx].snap;
        free(self->pending_edit_snaps[idx].id);
        memmove(&self->pending_edit_snaps[idx], &self->pending_edit_snaps[idx + 1],
                sizeof(hcli_pending_snap_t) *
                    (self->pending_edit_snap_count - (size_t)idx - 1));
        self->pending_edit_snap_count--;
    }

    /* render_edit_diff_with_delta(...) wrapped in try/except → best-effort. */
    if (snapshot) {
        char *diff = display_diff_from_snapshot(snapshot);
        if (diff) {
            display_emit_inline_diff(diff, NULL);
            free(diff);
        }
        display_snapshot_free(snapshot);
    } else {
        /* No snapshot: fall back to result-driven inline diff render. */
        display_render_edit_diff(function_name, result,
                                 function_args_json ? function_args_json : "{}",
                                 NULL, NULL, NULL);
    }
}

/* ── voice STT config accessors ─────────────────────────────────────────── */

/* Shared: fetch stt section from merged config. Caller frees. */
static json_t *b2_stt_section(void)
{
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return NULL;
    }
    json_t *stt = json_obj_get(cfg, "stt");
    json_t *out = (stt && stt->type == JSON_OBJECT) ? json_copy(stt) : NULL;
    json_free(cfg);
    return out;
}

/* PoP: hcli_voice_stt_model @ cli.py:_voice_stt_model */
char *hcli_voice_stt_model(hcli_t *self)
{
    (void)self;
    json_t *stt = b2_stt_section();
    if (!stt) return NULL;                 /* exception / non-dict → None */

    char *result = NULL;
    json_t *prov = json_obj_get(stt, "provider");
    const char *provider =
        (prov && prov->type == JSON_STRING && prov->str_val) ? prov->str_val : "";

    /* strip().lower() */
    while (*provider == ' ' || *provider == '\t') provider++;
    char plow[64] = {0};
    snprintf(plow, sizeof(plow), "%s", provider);
    size_t plen = strlen(plow);
    while (plen > 0 && (plow[plen - 1] == ' ' || plow[plen - 1] == '\t'))
        plow[--plen] = '\0';
    for (char *p = plow; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (!strcmp(plow, "local")) {
        json_t *local = json_obj_get(stt, "local");
        json_t *model = (local && local->type == JSON_OBJECT)
                            ? json_obj_get(local, "model") : NULL;
        if (model && model->type == JSON_STRING && model->str_val && model->str_val[0])
            result = strdup(model->str_val);
        if (!result)
            result = strdup("base");       /* local default */
    } else {
        json_t *model = json_obj_get(stt, "model");
        if (model && model->type == JSON_STRING && model->str_val)
            result = strdup(model->str_val);
        /* else: provider default → NULL */
    }
    json_free(stt);
    return result;
}

/* PoP: hcli_voice_stt_provider @ cli.py:_voice_stt_provider */
char *hcli_voice_stt_provider(hcli_t *self)
{
    (void)self;
    char *out = strdup("");
    json_t *stt = b2_stt_section();
    if (!stt) return out;                  /* exception / non-dict → "" */

    json_t *prov = json_obj_get(stt, "provider");
    if (prov && prov->type == JSON_STRING && prov->str_val) {
        const char *p = prov->str_val;
        while (*p == ' ' || *p == '\t') p++;
        char buf[128];
        snprintf(buf, sizeof(buf), "%s", p);
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'))
            buf[--len] = '\0';
        for (char *q = buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        free(out);
        out = strdup(buf);
    }
    json_free(stt);
    return out;
}

/* PoP: hcli_voice_beeps_enabled @ cli.py:_voice_beeps_enabled */
bool hcli_voice_beeps_enabled(hcli_t *self)
{
    (void)self;
    json_t *cfg = config_py_load_config_readonly();
    bool result = true;                     /* default True */
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *voice = json_obj_get(cfg, "voice");
        if (voice && voice->type == JSON_OBJECT) {
            json_t *beep = json_obj_get(voice, "beep_enabled");
            if (!beep || beep->type == JSON_NULL) {
                result = true;              /* .get(key, True) default */
            } else if (beep->type == JSON_BOOL) {
                result = beep->bool_val;
            } else if (beep->type == JSON_STRING && beep->str_val) {
                result = is_truthy_value(beep->str_val, true);  /* #49883 */
            } else if (beep->type == JSON_NUMBER) {
                result = beep->num_val != 0.0;
            }
        }
    }
    if (cfg) json_free(cfg);
    return result;
}

/* PoP: hcli_disable_voice_mode @ cli.py:_disable_voice_mode */
void hcli_disable_voice_mode(hcli_t *self)
{
    if (!self) return;

    pthread_mutex_t *lock = (pthread_mutex_t *)self->voice_lock;
    if (!lock) {
        lock = malloc(sizeof(pthread_mutex_t));
        if (lock) {
            pthread_mutex_init(lock, NULL);
            self->voice_lock = lock;
        }
    }
    void *recorder = NULL;
    if (lock) pthread_mutex_lock(lock);
    if (self->voice_recording && self->voice_recorder && self->recorder_cancel) {
        self->recorder_cancel(self->voice_recorder);
        self->voice_recording = false;
    }
    recorder = self->voice_recorder;
    self->voice_mode = false;
    self->voice_tts = false;
    self->voice_continuous = false;
    if (lock) pthread_mutex_unlock(lock);

    /* Shut down the persistent audio stream (background in Python; the
     * shutdown callback is best-effort here). */
    if (recorder) {
        if (self->recorder_shutdown)
            self->recorder_shutdown(recorder);
        self->voice_recorder = NULL;
    }

    /* Stop any active TTS playback (file player + streaming pipeline). */
    if (self->voice_tts_stop && self->tts_stop_set)
        self->tts_stop_set(self->voice_tts_stop);
    if (self->stop_playback_fn)
        self->stop_playback_fn();
    if (self->voice_tts_done_ev) {
        /* threading.Event.set() equivalent is driven by tts_stop_set seam;
         * the done-event has no dedicated setter in the C struct contract. */
    }

    printf("\n\033[2mVoice mode disabled.\033[0m\n");
    fflush(stdout);
}
