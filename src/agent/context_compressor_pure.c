/*
 * context_compressor_pure.c — Faithful C11 port of the pure, self-contained
 * helper surface of agent/context_compressor.py.
 *
 * See include/context_compressor_pure.h for the full function list and the
 * Python provenance of each. No agent handle, no async/IO — just json_t
 * message-array + string transforms, plus the redaction and error-classifier
 * libraries already ported elsewhere in the tree.
 */

#include "context_compressor_pure.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "hermes_redact.h"
#include "hermes_json.h"
#include "libregex/hermes_regex.h"

/* ── Shared constants (mirror the Python module-level literals) ─────────── */

static const char *SUMMARY_MISSING_CREDENTIAL_MARKERS[] = {
    "no api key was found",
    "no api key found",
    NULL,
};

static const char *SUMMARY_PERMANENT_QUOTA_MARKERS[] = {
    "insufficient_quota",
    "quota exceeded",
    "quota_exceeded",
    "out of funds",
    "out of credits",
    "out of credit",
    "out of extra usage",
    NULL,
};

#define CC_SKILL_PRUNE_RECENT_WINDOW 10
#define CC_SKILL_VIEW_PRUNE_MIN_CHARS 5000

/* ── _is_summary_access_or_quota_error ─────────────────────────────────── */

/* PoP: cc_is_summary_access_or_quota_error @ agent/context_compressor.py:_is_summary_access_or_quota_error */
bool cc_is_summary_access_or_quota_error(int status_code,
                                         const char *error_text,
                                         int classified_reason) {
    /* classified_reason mirrors liberrorclassifier failover_reason_t values. */
    if (classified_reason == 40 /* FAILOVER_RATE_LIMIT */)
        return false;
    if (classified_reason == 35 /* FAILOVER_AUTH */ ||
        classified_reason == 36 /* FAILOVER_AUTH_PERMANENT */)
        return true;

    const char *err = error_text ? error_text : "";
    size_t elen = strlen(err);
    char *low = malloc(elen + 1);
    if (!low) return false;
    for (size_t i = 0; i < elen; i++) low[i] = (char)tolower((unsigned char)err[i]);
    low[elen] = '\0';

    bool result = false;
    for (int i = 0; SUMMARY_MISSING_CREDENTIAL_MARKERS[i]; i++) {
        if (strstr(low, SUMMARY_MISSING_CREDENTIAL_MARKERS[i])) {
            result = true;
            goto done;
        }
    }

    if (status_code == 401 || status_code == 402 || status_code == 403) {
        result = true;
        goto done;
    }

    for (int i = 0; SUMMARY_PERMANENT_QUOTA_MARKERS[i]; i++) {
        if (strstr(low, SUMMARY_PERMANENT_QUOTA_MARKERS[i])) {
            result = true;
            goto done;
        }
    }

done:
    free(low);
    return result;
}

/* ── _skill_view_call_sites ───────────────────────────────────────────── */

/* PoP: cc_skill_view_call_sites @ agent/context_compressor.py:_skill_view_call_sites */
int cc_skill_view_call_sites(const json_t *messages,
                             cc_skill_view_site_t *sites, int limit) {
    if (!messages || !sites || limit <= 0) return 0;
    int count = 0;
    int n = (int)json_len(messages);
    for (int i = 0; i < n && count < limit; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg) continue;
        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "assistant") != 0) continue;
        json_t *tool_calls = json_obj_get(msg, "tool_calls");
        if (!tool_calls) continue;
        int tn = (int)json_len(tool_calls);
        for (int j = 0; j < tn && count < limit; j++) {
            json_t *tc = json_get(tool_calls, (size_t)j);
            if (!tc) continue;
            json_t *fn = json_obj_get(tc, "function");
            if (!fn) continue;
            const char *name = json_get_str(fn, "name", "");
            const char *args_str = json_get_str(fn, "arguments", "");
            if (strcmp(name, "skill_view") != 0 || !args_str || !*args_str)
                continue;
            json_t *args = json_parse(args_str, NULL);
            if (!args) continue;
            const char *skill = json_get_str(args, "name", "");
            if (skill && *skill) {
                sites[count].index = i;
                sites[count].name = strdup(skill);
                count++;
            }
            json_free(args);
        }
    }
    return count;
}

/* ── _collect_ghosted_skill_names ─────────────────────────────────────── */

/* PoP: cc_collect_ghosted_skill_names @ agent/context_compressor.py:_collect_ghosted_skill_names */
int cc_collect_ghosted_skill_names(const json_t *turns,
                                    char **out_names, int limit) {
    if (!turns || !out_names || limit <= 0) return 0;

    int count = 0;

    /* call_id -> skill map */
    char *call_ids[256];
    char *call_skills[256];
    int nmap = 0;

    cc_skill_view_site_t sites[256];
    int nsites = cc_skill_view_call_sites(turns, sites,
                                          (int)(sizeof(sites) / sizeof(sites[0])));
    for (int i = 0; i < nsites && nmap < 256; i++) {
        json_t *msg = json_get(turns, (size_t)sites[i].index);
        if (!msg) continue;
        json_t *tool_calls = json_obj_get(msg, "tool_calls");
        if (!tool_calls) continue;
        int tn = (int)json_len(tool_calls);
        for (int j = 0; j < tn && nmap < 256; j++) {
            json_t *tc = json_get(tool_calls, (size_t)j);
            if (!tc) continue;
            json_t *fn = json_obj_get(tc, "function");
            if (!fn) continue;
            const char *name = json_get_str(fn, "name", "");
            if (strcmp(name, "skill_view") != 0) continue;
            const char *cid = json_get_str(tc, "id", "");
            if (!cid || !*cid) continue;
            call_ids[nmap] = strdup(cid);
            call_skills[nmap] = strdup(sites[i].name);
            nmap++;
        }
    }

    int nt = (int)json_len(turns);
    for (int i = 0; i < nt; i++) {
        json_t *msg = json_get(turns, (size_t)i);
        if (!msg) continue;
        json_t *content = json_obj_get(msg, "content");
        char *text = NULL;
        if (content && content->type == JSON_STRING) {
            text = strdup(json_get_str(content, "", ""));
        } else {
            text = context_compressor_content_text(content);
        }
        if (text) {
            char **pruned = malloc(sizeof(char *) * 64);
            int pcount = 0;
            context_compressor__extract_pruned_skill_names(text, pruned, &pcount, 64);
            for (int k = 0; k < pcount; k++) {
                bool dup = false;
                for (int o = 0; o < count; o++)
                    if (strcmp(out_names[o], pruned[k]) == 0) { dup = true; break; }
                if (!dup && count < limit) out_names[count++] = strdup(pruned[k]);
                free(pruned[k]);
            }
            free(pruned);
            free(text);
        }

        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "tool") == 0) {
            json_t *c2 = json_obj_get(msg, "content");
            char *ttext = NULL;
            if (c2 && c2->type == JSON_STRING) ttext = strdup(json_get_str(c2, "", ""));
            else ttext = context_compressor_content_text(c2);
            if (ttext && (int)strlen(ttext) > CC_SKILL_VIEW_PRUNE_MIN_CHARS) {
                const char *tcid = json_get_str(msg, "tool_call_id", "");
                for (int m = 0; m < nmap; m++) {
                    if (strcmp(call_ids[m], tcid) == 0) {
                        bool dup = false;
                        for (int o = 0; o < count; o++)
                            if (strcmp(out_names[o], call_skills[m]) == 0) { dup = true; break; }
                        if (!dup && count < limit) out_names[count++] = strdup(call_skills[m]);
                        break;
                    }
                }
            }
            free(ttext);
        }
    }

    for (int i = 0; i < nmap; i++) { free(call_ids[i]); free(call_skills[i]); }
    for (int i = 0; i < nsites; i++) free(sites[i].name);
    return count;
}

/* ── _collect_protected_skill_names ───────────────────────────────────── */

/* PoP: cc_collect_protected_skill_names @ agent/context_compressor.py:_collect_protected_skill_names */
int cc_collect_protected_skill_names(const json_t *messages,
                                     int prune_boundary,
                                     char **out_names, int limit) {
    if (!messages || !out_names || limit <= 0) return 0;
    int total = (int)json_len(messages);
    if (total == 0) return 0;

    int recent_start = (total - CC_SKILL_PRUNE_RECENT_WINDOW > 0)
                           ? total - CC_SKILL_PRUNE_RECENT_WINDOW : 0;
    int tail_start = (prune_boundary > 0) ? prune_boundary : 0;

    char **tail_texts = malloc(sizeof(char *) * (total + 1));
    int ntail = 0;
    for (int i = tail_start; i < total; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg) continue;
        if (strcmp(json_get_str(msg, "role", ""), "user") != 0) continue;
        json_t *c = json_obj_get(msg, "content");
        char *t = NULL;
        if (c && c->type == JSON_STRING) t = strdup(json_get_str(c, "", ""));
        else t = context_compressor_content_text(c);
        if (t && *t) tail_texts[ntail++] = t;
    }

    int count = 0;
    cc_skill_view_site_t sites[256];
    int nsites = cc_skill_view_call_sites(messages, sites,
                                          (int)(sizeof(sites) / sizeof(sites[0])));
    for (int s = 0; s < nsites; s++) {
        int idx = sites[s].index;
        const char *skill = sites[s].name;
        if (!skill) continue;
        bool protect = false;
        if (idx >= recent_start) protect = true;
        if (idx >= tail_start) protect = true;
        if (!protect) {
            for (int t = 0; t < ntail; t++) {
                if (strstr(tail_texts[t], skill)) { protect = true; break; }
            }
        }
        if (protect) {
            char low[256];
            size_t sl = strlen(skill);
            if (sl >= sizeof(low)) sl = sizeof(low) - 1;
            for (size_t k = 0; k < sl; k++) low[k] = (char)tolower((unsigned char)skill[k]);
            low[sl] = '\0';
            bool dup = false;
            for (int o = 0; o < count; o++)
                if (strcmp(out_names[o], low) == 0) { dup = true; break; }
            if (!dup && count < limit) out_names[count++] = strdup(low);
        }
    }

    for (int i = 0; i < ntail; i++) free(tail_texts[i]);
    free(tail_texts);
    for (int i = 0; i < nsites; i++) free(sites[i].name);
    return count;
}

/* ── _redact_compaction_text ──────────────────────────────────────────── */

/* PoP: cc_redact_compaction_text @ agent/context_compressor.py:_redact_compaction_text */
char *cc_redact_compaction_text(const char *text) {
    /* Python force=True overrides security.redact_secrets=false — a summary is
     * a persistence boundary. The C redactor honors the config opt-out, so we
     * use the force variant which always redacts. */
    return hermes_redact_force(text ? text : "");
}

/* ── _serialized_length_for_budget ────────────────────────────────────── */

/* PoP: cc_serialized_length_for_budget @ agent/context_compressor.py:_serialized_length_for_budget */
int cc_serialized_length_for_budget(const json_t *value) {
    if (!value) return 0;
    if (value->type == JSON_STRING) {
        const char *s = json_get_str(value, "", "");
        return (int)strlen(s);
    }
    char *ser = json_serialize(value);
    if (!ser) return 0;
    int len = (int)strlen(ser);
    free(ser);
    return len;
}

/* ── _image_part_label ────────────────────────────────────────────────── */

/* PoP: cc_image_part_label @ agent/context_compressor.py:_image_part_label */
char *cc_image_part_label(const json_t *part) {
    if (!part) return strdup("[image]");
    const char *url = NULL;
    json_t *image_url = json_obj_get(part, "image_url");
    if (image_url && image_url->type == JSON_OBJECT)
        url = json_get_str(image_url, "url", "");
    else if (image_url && image_url->type == JSON_STRING)
        url = json_get_str(part, "image_url", "");
    else
        url = json_get_str(part, "url", "");
    if (url && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)) {
        size_t need = strlen(url) + 16;
        char *out = malloc(need);
        if (out) snprintf(out, need, "[image: %s]", url);
        return out;
    }
    return strdup("[image]");
}

/* ── _str_arg ─────────────────────────────────────────────────────────── */

/* PoP: cc_str_arg @ agent/context_compressor.py:_str_arg */
char *cc_str_arg(const json_t *args, const char *key, const char *def) {
    if (!args || !key) return def ? strdup(def) : strdup("");
    json_t *v = json_obj_get(args, key);
    if (!v) return def ? strdup(def) : strdup("");
    if (v->type == JSON_STRING) {
        return strdup(json_get_str(v, "", def ? def : ""));
    }
    if (v->type == JSON_NULL) {
        return def ? strdup(def) : strdup("");
    }
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_BOOL) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s", v->bool_val ? "true" : "false");
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    char *out = ser ? strdup(ser) : strdup("");
    free(ser);
    return out;
}

/* ── _summarize_tool_result_unguarded ─────────────────────────────────── */

/* PoP: cc_summarize_tool_result_unguarded @ agent/context_compressor.py:_summarize_tool_result_unguarded */
static const char *extract_group(const char *pattern, const char *text) {
    if (!text) return "?";
    hregex_t *re = regex_compile(pattern, 0);
    if (!re) return "?";
    regex_match_t *m = regex_search(re, text);
    const char *res = "?";
    if (m && m->group_count > 1 && m->groups[1])
        res = m->groups[1];
    regex_match_free(m);
    regex_free(re);
    return res;
}

char *cc_summarize_tool_result_unguarded(const char *tool_name,
                                         const char *tool_args,
                                         const char *tool_content) {
    if (!tool_name) tool_name = "";
    const char *args_str = tool_args ? tool_args : "";
    const char *content = tool_content ? tool_content : "";

    json_t *args = NULL;
    if (*args_str) {
        args = json_parse(args_str, NULL);
        if (args && args->type != JSON_OBJECT) {
            json_free(args);
            args = NULL;
        }
    }

    int content_len = (int)strlen(content);
    int line_count = 0;
    for (int i = 0; content[i]; i++)
        if (content[i] == '\n') line_count++;
    if (content_len > 0) line_count += 1;

    char *out = NULL;

    if (strcmp(tool_name, "terminal") == 0) {
        char *cmd = args ? cc_str_arg(args, "command", "") : strdup("");
        if ((int)strlen(cmd) > 80) { cmd[77] = '.'; cmd[78] = '.'; cmd[79] = '.'; cmd[80] = '\0'; }
        const char *exit_code = extract_group("\"exit_code\"\\s*:\\s*(-?\\d+)", content);
        size_t cap = strlen(cmd) + 64;
        out = malloc(cap);
        snprintf(out, cap, "[terminal] ran `%s` -> exit %s, %d lines output", cmd, exit_code, line_count);
        free(cmd);
    } else if (strcmp(tool_name, "read_file") == 0) {
        const char *path = args ? json_get_str(args, "path", "?") : "?";
        int offset = args ? (int)json_get_num(args, "offset", 1) : 1;
        size_t cap = strlen(path) + 64;
        out = malloc(cap);
        snprintf(out, cap, "[read_file] read %s from line %d (%d chars)", path, offset, content_len);
    } else if (strcmp(tool_name, "write_file") == 0) {
        const char *path = args ? json_get_str(args, "path", "?") : "?";
        const char *wc = args ? json_get_str(args, "content", NULL) : NULL;
        int written = 0;
        if (wc) {
            written = 1;
            for (const char *p = wc; *p; p++) if (*p == '\n') written++;
        }
        char wbuf[32];
        snprintf(wbuf, sizeof(wbuf), "%d", written);
        size_t cap = strlen(path) + 64;
        out = malloc(cap);
        snprintf(out, cap, "[write_file] wrote to %s (%s lines)", path, wc ? wbuf : "?");
    } else if (strcmp(tool_name, "search_files") == 0) {
        const char *pattern = args ? json_get_str(args, "pattern", "?") : "?";
        const char *path = args ? json_get_str(args, "path", ".") : ".";
        const char *target = args ? json_get_str(args, "target", "content") : "content";
        const char *count = extract_group("\"total_count\"\\s*:\\s*(\\d+)", content);
        size_t cap = strlen(pattern) + strlen(path) + strlen(target) + 64;
        out = malloc(cap);
        snprintf(out, cap, "[search_files] %s search for '%s' in %s -> %s matches",
                 target, pattern, path, count);
    } else {
        size_t cap = strlen(tool_name) + 64;
        out = malloc(cap);
        snprintf(out, cap, "[%s] completed (%d chars output)", tool_name, content_len);
    }

    if (args) json_free(args);
    return out ? out : strdup("");
}

/* ── resolve_model_threshold ──────────────────────────────────────────── */

/* PoP: cc_resolve_model_threshold @ agent/context_compressor.py:resolve_model_threshold */
double cc_resolve_model_threshold(const char *model,
                                  const char *const *threshold_keys,
                                  const double *threshold_vals,
                                  int threshold_count,
                                  double default_threshold) {
    if (!model || !*model || !threshold_keys || !threshold_vals || threshold_count <= 0)
        return default_threshold;
    const char *best_key = NULL;
    size_t best_len = 0;
    for (int i = 0; i < threshold_count; i++) {
        const char *key = threshold_keys[i];
        if (!key) continue;
        if (strstr(model, key) && strlen(key) > best_len) {
            best_key = key;
            best_len = strlen(key);
        }
    }
    if (best_key) {
        for (int i = 0; i < threshold_count; i++)
            if (threshold_keys[i] == best_key)
                return threshold_vals[i];
    }
    return default_threshold;
}
