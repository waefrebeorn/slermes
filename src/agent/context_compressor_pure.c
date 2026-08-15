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
#include "context_compressor_constants.h"
/* _SUMMARY_TOKENS_CEILING / _MIN_SUMMARY_TOKENS not yet exposed by the constants
 * header — mirror Python's literal values inline (see context_compressor.py). */
#define CC_MAX_SUMMARY_TOKENS_CEILING 10000L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include "hermes_redact.h"
#include "hermes_json.h"
#include "libregex/hermes_regex.h"
#include "provider_metadata.h"  /* get_model_context_length() */

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

/* ── Runtime constant arrays (mirror Python module-level literals) ──────── */

/* PoP: (constants) @ agent/context_compressor.py:_HISTORICAL_SUMMARY_PREFIXES */
const char *cc_historical_summary_prefixes[] = {
#if CC_NUM_HISTORICAL_PREFIXES > 0
    CC_HISTORICAL_PREFIX_0,
#endif
#if CC_NUM_HISTORICAL_PREFIXES > 1
    CC_HISTORICAL_PREFIX_1,
#endif
#if CC_NUM_HISTORICAL_PREFIXES > 2
    CC_HISTORICAL_PREFIX_2,
#endif
#if CC_NUM_HISTORICAL_PREFIXES > 3
    CC_HISTORICAL_PREFIX_3,
#endif
};
const size_t cc_num_historical_prefixes = CC_NUM_HISTORICAL_PREFIXES;

/* PoP: (constants) @ agent/context_compressor.py:_IMAGE_PART_TYPES */
const char *cc_image_part_types[] = { "image_url", "input_image", "image" };
const size_t cc_num_image_part_types = 3;

/* ── _safe_int ─────────────────────────────────────────────────────────── */

/* PoP: cc_safe_int @ agent/context_compressor.py:_safe_int */
int cc_safe_int(const json_t *value, int *out) {
    if (!value || value->type == JSON_NULL) { if (out) *out = 0; return 0; }
    if (value->type == JSON_NUMBER) { if (out) *out = (int)value->num_val; return 1; }
    if (value->type == JSON_STRING) {
        const char *s = value->str_val;
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end != s && *end == '\0') { if (out) *out = (int)v; return 1; }
    }
    if (out) *out = 0;
    return 0;
}

/* ── _skill_pruned_marker / _extract_pruned_skill_names ────────────────── */

/* PoP: cc_skill_pruned_marker @ agent/context_compressor.py:_skill_pruned_marker */
char *cc_skill_pruned_marker(const char *skill_name) {
    size_t need = strlen(CC_SKILL_PRUNED_MARKER_PREFIX) + strlen(skill_name) + 64;
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need,
             "%s content lost in compression; reload with skill_view(name='%s')]",
             CC_SKILL_PRUNED_MARKER_PREFIX, skill_name ? skill_name : "");
    return out;
}

/* PoP: cc_extract_pruned_skill_names @ agent/context_compressor.py:_extract_pruned_skill_names */
int cc_extract_pruned_skill_names(const char *text, char **out_names, int *out_count, int limit) {
    if (!text || !out_names || !out_count) return 0;
    *out_count = 0;
    /* marker: "[SKILL_PRUNED: ... reload with skill_view(name='NAME')]" */
    hregex_t *re = regex_compile("\\[SKILL_PRUNED:[^]]*reload with skill_view\\(name='([^']+)'\\)\\]",
                                 0);
    if (!re) return 0;
    int count = 0;
    const char *p = text;
    regex_match_t *m;
    while (count < limit && (m = regex_search(re, p)) != NULL && m->matched) {
        /* group_count is never set by hermes_regex; check groups[1] directly */
        const char *name = (m->groups[1] != NULL) ? m->groups[1] : NULL;
        size_t match_len = m->groups[0] ? strlen(m->groups[0]) : 1;
        if (name) {
            int dup = 0;
            for (int i = 0; i < count; i++)
                if (strcmp(out_names[i], name) == 0) { dup = 1; break; }
            if (!dup) out_names[count++] = strdup(name);
        }
        regex_match_free(m);
        /* advance past this match (must use match_len saved before free) */
        p = p + match_len;
        if (*p == '\0') break;
    }
    regex_free(re);
    *out_count = count;
    return count;
}

/* ── _content_text_for_contains ────────────────────────────────────────── */

/* PoP: cc_content_text_for_contains @ agent/context_compressor.py:_content_text_for_contains */
char *cc_content_text_for_contains(const json_t *content) {
    if (!content) return strdup("");
    if (content->type == JSON_STRING) return strdup(content->str_val);
    if (content->type != JSON_ARRAY) return strdup("");
    /* join text parts + string parts */
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    int n = (int)json_len(content);
    for (int i = 0; i < n; i++) {
        json_t *item = json_get(content, (size_t)i);
        const char *seg = NULL;
        if (item && item->type == JSON_STRING) seg = item->str_val;
        else if (item && item->type == JSON_OBJECT) {
            const char *t = json_get_str(item, "type", "");
            if (strcmp(t, "text") == 0) seg = json_get_str(item, "text", NULL);
        }
        if (!seg) continue;
        size_t need = len + strlen(seg) + 1;
        if (need > cap) { cap = need * 2; char *n2 = realloc(out, cap); if (!n2) { free(out); return strdup(""); } out = n2; }
        memcpy(out + len, seg, strlen(seg));
        len += strlen(seg);
        out[len] = '\0';
    }
    return out;
}

/* ── _content_length_for_budget ────────────────────────────────────────── */

/* PoP: cc_content_length_for_budget @ agent/context_compressor.py:_content_length_for_budget */
int cc_content_length_for_budget(const json_t *raw_content) {
    if (!raw_content) return 0;
    if (raw_content->type == JSON_STRING) return (int)strlen(raw_content->str_val);
    if (raw_content->type != JSON_ARRAY) {
        char *s = json_serialize(raw_content);
        int r = s ? (int)strlen(s) : 0;
        free(s);
        return r;
    }
    int total = 0;
    int n = (int)json_len(raw_content);
    for (int i = 0; i < n; i++) {
        json_t *p = json_get(raw_content, (size_t)i);
        if (!p) continue;
        if (p->type == JSON_STRING) { total += (int)strlen(p->str_val); continue; }
        if (p->type != JSON_OBJECT) { char *s = json_serialize(p); int r = s?(int)strlen(s):0; free(s); total += r; continue; }
        const char *ptype = json_get_str(p, "type", "");
        int is_img = 0;
        for (size_t k = 0; k < cc_num_image_part_types; k++)
            if (strcmp(ptype, cc_image_part_types[k]) == 0) { is_img = 1; break; }
        if (is_img) total += CC_IMAGE_CHAR_EQUIVALENT;
        else total += (int)strlen(json_get_str(p, "text", ""));
    }
    return total;
}

/* ── _is_image_part / _content_has_images / _strip_images_from_content ─── */

/* PoP: cc_is_image_part @ agent/context_compressor.py:_is_image_part */
int cc_is_image_part(const json_t *part) {
    if (!part || part->type != JSON_OBJECT) return 0;
    const char *t = json_get_str(part, "type", "");
    for (size_t k = 0; k < cc_num_image_part_types; k++)
        if (strcmp(t, cc_image_part_types[k]) == 0) return 1;
    return 0;
}

/* PoP: cc_content_has_images @ agent/context_compressor.py:_content_has_images */
int cc_content_has_images(const json_t *content) {
    if (!content || content->type != JSON_ARRAY) return 0;
    int n = (int)json_len(content);
    for (int i = 0; i < n; i++)
        if (cc_is_image_part(json_get(content, (size_t)i))) return 1;
    return 0;
}

/* PoP: cc_strip_images_from_content @ agent/context_compressor.py:_strip_images_from_content */
json_t *cc_strip_images_from_content(const json_t *content) {
    if (!content || content->type != JSON_ARRAY) return (json_t *)content;
    int n = (int)json_len(content);
    int has = 0;
    for (int i = 0; i < n; i++)
        if (cc_is_image_part(json_get(content, (size_t)i))) { has = 1; break; }
    if (!has) return (json_t *)content;
    json_t *out = json_array();
    for (int i = 0; i < n; i++) {
        json_t *p = json_get(content, (size_t)i);
        if (cc_is_image_part(p)) {
            json_t *ph = json_object();
            json_set(ph, "type", json_string("text"));
            json_set(ph, "text", json_string("[Attached image — stripped after compression]"));
            json_append(out, ph);
        } else {
            json_append(out, json_copy(p));
        }
    }
    return out;
}

/* ── _starts_with_summary_prefix / _strip_summary_prefix / _with_summary_prefix ─ */

/* PoP: cc_starts_with_summary_prefix @ agent/context_compressor.py:_starts_with_summary_prefix */
int cc_starts_with_summary_prefix(const char *text) {
    if (!text) return 0;
    if (strncmp(text, CC_SUMMARY_PREFIX, strlen(CC_SUMMARY_PREFIX)) == 0) return 1;
    if (strncmp(text, CC_LEGACY_SUMMARY_PREFIX, strlen(CC_LEGACY_SUMMARY_PREFIX)) == 0) return 1;
    for (size_t i = 0; i < cc_num_historical_prefixes; i++)
        if (strncmp(text, cc_historical_summary_prefixes[i],
                    strlen(cc_historical_summary_prefixes[i])) == 0) return 1;
    return 0;
}

/* PoP: cli_agent_context_compressor__strip_summary_prefix @ agent/context_compressor.py:_strip_summary_prefix */
char *cc_strip_summary_prefix(const char *summary) {
    if (!summary) return strdup("");
    char *text = strdup(summary);
    /* strip leading whitespace */
    char *s = text;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    /* merged-summary delimiter: keep only content after it */
    char *delim = strstr(s, CC_MERGED_SUMMARY_DELIMITER);
    if (delim) {
        s = delim + strlen(CC_MERGED_SUMMARY_DELIMITER);
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    }
    /* strip any known prefix (current/legacy/historical) */
    const char *prefixes[CC_NUM_HISTORICAL_PREFIXES + 2];
    prefixes[0] = CC_SUMMARY_PREFIX;
    prefixes[1] = CC_LEGACY_SUMMARY_PREFIX;
    for (size_t i = 0; i < cc_num_historical_prefixes; i++)
        prefixes[2 + i] = cc_historical_summary_prefixes[i];
    for (int i = 0; i < (int)(cc_num_historical_prefixes + 2); i++) {
        size_t pl = strlen(prefixes[i]);
        if (strncmp(s, prefixes[i], pl) == 0) {
            s += pl;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            break;
        }
    }
    /* strip end marker */
    char *em = strstr(s, CC_SUMMARY_END_MARKER);
    if (em) { *em = '\0'; }
    /* trim trailing whitespace */
    size_t L = strlen(s);
    while (L > 0 && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r')) s[--L]='\0';
    char *res = strdup(s);
    free(text);
    return res;
}

/* PoP: cc_starts_with_summary_prefix @ agent/context_compressor.py:_with_summary_prefix */
char *cc_with_summary_prefix(const char *summary) {
    char *body = cc_strip_summary_prefix(summary);
    if (!body || *body == '\0') {
        free(body);
        return strdup(CC_SUMMARY_PREFIX);
    }
    size_t need = strlen(CC_SUMMARY_PREFIX) + strlen(body) + 2;
    char *out = malloc(need);
    snprintf(out, need, "%s\n%s", CC_SUMMARY_PREFIX, body);
    free(body);
    return out;
}

/* ── _get_tool_call_id ─────────────────────────────────────────────────── */

/* PoP: cc_get_tool_call_id @ agent/context_compressor.py:_extract_tool_call_id */
const char *cc_get_tool_call_id(const json_t *tool_call) {
    if (!tool_call || tool_call->type != JSON_OBJECT) return "";
    return json_get_str(tool_call, "id", "");
}

/* PoP: cc_get_tool_call_id_by_tc @ agent/context_compressor.py:_get_tool_call_id */
const char *cc_get_tool_call_id_by_tc(const json_t *tc) {
    if (!tc || tc->type != JSON_OBJECT) return "";
    const char *v = json_get_str(tc, "call_id", "");
    if (v && *v) return v;
    return json_get_str(tc, "id", "");
}

/* ── summary-content classification family ─────────────────────────────── */

/* PoP: cc_has_compressed_summary_metadata @ agent/context_compressor.py:_has_compressed_summary_metadata */
int cc_has_compressed_summary_metadata(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(message, CC_COMPRESSED_SUMMARY_METADATA_KEY);
    return (v && v->type != JSON_NULL && !(v->type == JSON_BOOL && !v->bool_val)
            && !(v->type == JSON_STRING && strlen(v->str_val) == 0));
}

/* PoP: cc_is_context_summary_content @ agent/context_compressor.py:_is_context_summary_content */
int cc_is_context_summary_content(const json_t *content) {
    return cc_classify_summary_content(content) != NULL;
}

/* PoP: cc_classify_summary_content @ agent/context_compressor.py:classify_summary_content */
/* Returns one of: "standalone", "merged", or NULL. Caller frees when non-NULL. */
char *cc_classify_summary_content(const json_t *content) {
    char *text = cc_content_text_for_contains(content);
    if (!text) return NULL;
    char *rt = NULL;
    char *s = text;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *delim = strstr(s, CC_MERGED_SUMMARY_DELIMITER);
    if (delim) {
        char *after = delim + strlen(CC_MERGED_SUMMARY_DELIMITER);
        while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;
        rt = cc_starts_with_summary_prefix(after) ? strdup("merged") : NULL;
    } else {
        rt = cc_starts_with_summary_prefix(s) ? strdup("standalone") : NULL;
    }
    free(text);
    return rt;
}

/* PoP: cc_is_context_summary_message @ agent/context_compressor.py:_is_context_summary_message */
int cc_is_context_summary_message(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    return cc_has_compressed_summary_metadata(message) ||
           cc_is_context_summary_content(json_obj_get(message, "content"));
}

/* ── user-turn predicates ───────────────────────────────────────────────── */

/* PoP: cc_is_blank_user_turn @ agent/context_compressor.py:_is_blank_user_turn */
int cc_is_blank_user_turn(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    if (strcmp(json_get_str(message, "role", ""), "user") != 0) return 0;
    if (cc_has_compressed_summary_metadata(message)) return 0;
    json_t *content = json_obj_get(message, "content");
    if (cc_is_context_summary_content(content)) return 0;
    if (content == NULL || (content->type == JSON_STRING && strlen(content->str_val) == 0))
        return 1;
    if (content->type != JSON_ARRAY) return 0;
    if (json_len(content) == 0) return 1;
    int n = (int)json_len(content);
    for (int i = 0; i < n; i++) {
        json_t *part = json_get(content, (size_t)i);
        if (part && part->type == JSON_STRING) {
            if (strlen(part->str_val) > 0) return 0;
            continue;
        }
        if (part && part->type == JSON_OBJECT) {
            const char *t = json_get_str(part, "type", "");
            if (strcmp(t, "text") == 0 || strcmp(t, "input_text") == 0) {
                const char *txt = json_get_str(part, "text", "");
                if (strlen(txt) > 0) return 0;
                continue;
            }
        }
        return 0; /* image/audio/unknown structured block = user input */
    }
    return 1;
}

/* PoP: cc_is_synthetic_compression_user_turn @ agent/context_compressor.py:_is_synthetic_compression_user_turn */
int cc_is_synthetic_compression_user_turn(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    if (strcmp(json_get_str(message, "role", ""), "user") != 0) return 0;
    if (cc_has_compressed_summary_metadata(message)) return 1;
    json_t *content = json_obj_get(message, "content");
    if (cc_is_context_summary_content(content)) return 1;
    char *text = cc_content_text_for_contains(content);
    int hit = 0;
    if (text) {
        /* strip */
        char *s = text;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (strcmp(s, CC_COMPRESSION_CONTINUATION_USER_CONTENT) == 0 ||
            strcmp(s, CC_LEGACY_COMPRESSION_CONTINUATION_USER_CONTENT) == 0) {
            hit = 1;
        } else if (strncmp(s, CC_TODO_INJECTION_HEADER, strlen(CC_TODO_INJECTION_HEADER)) == 0
                   && s[strlen(CC_TODO_INJECTION_HEADER)] == '\n') {
            hit = 1;
        }
        free(text);
    }
    return hit;
}

/* PoP: cc_is_actionable_user_turn @ agent/context_compressor.py:_is_actionable_user_turn */
int cc_is_actionable_user_turn(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    if (strcmp(json_get_str(message, "role", ""), "user") != 0) return 0;
    if (cc_has_compressed_summary_metadata(message)) return 0;
    if (cc_is_context_summary_content(json_obj_get(message, "content"))) return 0;
    return !cc_is_blank_user_turn(message);
}

/* PoP: cc_transcript_has_real_user_turn @ agent/context_compressor.py:_transcript_has_real_user_turn */
int cc_transcript_has_real_user_turn(const json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return 0;
    int n = (int)json_len(messages);
    for (int i = 0; i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(msg, "role", ""), "user") != 0) continue;
        if (cc_is_synthetic_compression_user_turn(msg)) continue;
        return 1;
    }
    return 0;
}

/* PoP: cc_blank_echo_indices_after @ agent/context_compressor.py:_blank_echo_indices_after */
/* Returns a malloc'd int array of removable indices; *out_count set. Caller frees.
 * Returns NULL when no removable run. */
int *cc_blank_echo_indices_after(const json_t *messages, int user_idx, int *out_count) {
    static int empty[0];  /* not used; we always malloc */
    (void)empty;
    int *result = NULL;
    *out_count = 0;
    if (!messages || messages->type != JSON_ARRAY) return NULL;
    int n = (int)json_len(messages);
    if (user_idx < 0) return NULL;
    int idx = user_idx + 1;
    int cap = 0;
    while (idx < n && cc_is_blank_user_turn(json_get(messages, (size_t)idx))) {
        if (*out_count >= cap) { cap = cap ? cap*2 : 4; int *r2 = realloc(result, cap*sizeof(int)); if(!r2){free(result);*out_count=0;return NULL;} result = r2; }
        result[(*out_count)++] = idx;
        idx++;
    }
    if (*out_count == 0 || idx >= n) { free(result); *out_count = 0; return NULL; }
    if (strcmp(json_get_str(json_get(messages, (size_t)idx), "role", ""), "assistant") != 0) {
        free(result); *out_count = 0; return NULL;
    }
    return result;
}

/* ── summary discovery / unwrap ─────────────────────────────────────────── */

/* PoP: cc_find_context_summaries @ agent/context_compressor.py:_find_context_summaries */
/* Fills out_idx[] (caller-sized >= end-start) and out_bodies[] (caller frees each).
 * Returns count of summaries found. */
int cc_find_context_summaries(const json_t *messages, int start, int end,
                              int *out_idx, char **out_bodies, int limit) {
    if (!messages || messages->type != JSON_ARRAY) return 0;
    int n = (int)json_len(messages);
    int count = 0;
    for (int idx = start; idx < end && idx < n && count < limit; idx++) {
        json_t *msg = json_get(messages, (size_t)idx);
        if (!cc_is_context_summary_message(msg)) continue;
        char *body = cc_strip_summary_prefix(cc_content_text_for_contains(json_obj_get(msg, "content")));
        out_idx[count] = idx;
        out_bodies[count] = body;
        count++;
    }
    return count;
}

/* PoP: cc_strip_context_summary_handoff_message @ agent/context_compressor.py:_strip_context_summary_handoff_message */
/* Returns a NEW message json_t (caller frees) with stale handoff data dropped,
 * merged prior-tail content preserved. Returns NULL on alloc failure. */
json_t *cc_strip_context_summary_handoff_message(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return json_copy(message);
    json_t *content = json_obj_get(message, "content");
    int is_summary = cc_is_context_summary_content(content) ||
                     cc_has_compressed_summary_metadata(message);
    if (!is_summary) return json_copy(message);

    if (content->type == JSON_STRING) {
        const char *c = content->str_val;
        if (strstr(c, CC_MERGED_SUMMARY_DELIMITER)) {
            char *prior = strdup(c);
            char *d = strstr(prior, CC_MERGED_SUMMARY_DELIMITER);
            *d = '\0';
            char *p = prior;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (strncmp(p, CC_MERGED_PRIOR_CONTEXT_HEADER, strlen(CC_MERGED_PRIOR_CONTEXT_HEADER)) == 0) {
                p += strlen(CC_MERGED_PRIOR_CONTEXT_HEADER);
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            }
            if (*p) {
                json_t *out = json_copy(message);
                json_set(out, "content", json_string(p));
                json_obj_del(out, CC_COMPRESSED_SUMMARY_METADATA_KEY);
                free(prior);
                return out;
            }
            free(prior);
        } else {
            char *mc = strstr(c, CC_SUMMARY_END_MARKER);
            if (mc) {
                char *remainder = strdup(mc + strlen(CC_SUMMARY_END_MARKER));
                char *r = remainder;
                while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') r++;
                if (*r) {
                    json_t *out = json_copy(message);
                    json_set(out, "content", json_string(r));
                    json_obj_del(out, CC_COMPRESSED_SUMMARY_METADATA_KEY);
                    free(remainder);
                    return out;
                }
                free(remainder);
            }
        }
    }
    /* list content / unhandled: fall back to a copy */
    return json_copy(message);
}

/* ── threshold math (pure) ──────────────────────────────────────────────── */

/* PoP: cc_coerce_threshold_tokens_cap @ agent/context_compressor.py:_coerce_threshold_tokens_cap */
long cc_coerce_threshold_tokens_cap(const json_t *value) {
    if (!value || value->type == JSON_NULL) return -1; /* None */
    long iv;
    if (value->type == JSON_NUMBER) iv = (long)value->num_val;
    else if (value->type == JSON_STRING) {
        char *endp = NULL;
        iv = strtol(value->str_val, &endp, 10);
        if (endp == value->str_val || *endp != '\0') return -1;
    } else return -1;
    return iv > 0 ? iv : -1;
}

/* PoP: cc_effective_threshold_percent @ agent/context_compressor.py:_effective_threshold_percent */
double cc_effective_threshold_percent(long context_length, double threshold_percent) {
    if (context_length > 0 && context_length < CC_SMALL_CTX_WINDOW_LIMIT)
        return (threshold_percent > CC_SMALL_CTX_THRESHOLD_PERCENT)
                   ? threshold_percent : CC_SMALL_CTX_THRESHOLD_PERCENT;
    return threshold_percent;
}

/* PoP: cc_compute_threshold_tokens @ agent/context_compressor.py:_compute_threshold_tokens */
long cc_compute_threshold_tokens(long context_length, double threshold_percent,
                                  long max_tokens) {
    long effective_window = context_length - (max_tokens > 0 ? max_tokens : 0);
    if (effective_window <= 0) effective_window = context_length;
    long pct_value = (long)(effective_window * threshold_percent);
    long floored = pct_value > CC_MINIMUM_CONTEXT_LENGTH ? pct_value : CC_MINIMUM_CONTEXT_LENGTH;
    if (effective_window > 0 && floored >= effective_window) {
        long r = (long)(effective_window * CC_MIN_CTX_TRIGGER_RATIO);
        if (r > effective_window - 1) r = effective_window - 1;
        if (r < 1) r = 1;
        return r;
    }
    return floored;
}

/* PoP: cc_apply_threshold_tokens_cap @ agent/context_compressor.py:_apply_threshold_tokens_cap */
/* Pure: returns the clamped threshold_tokens given cap + context_length. */
long cc_apply_threshold_tokens_cap(long threshold_tokens, long threshold_tokens_cap,
                                    long context_length) {
    if (threshold_tokens_cap > 0) {
        long effective_cap = (threshold_tokens_cap < context_length)
                                 ? threshold_tokens_cap : context_length;
        if (effective_cap < threshold_tokens) return effective_cap;
    }
    return threshold_tokens;
}

/* ── restart-handoff probe bounds (pure) ────────────────────────────────── */

/* PoP: cc_restart_handoff_probe_bounds @ agent/context_compressor.py:_restart_handoff_probe_bounds */
void cc_restart_handoff_probe_bounds(const json_t *messages, int protect_first_n,
                                     int *out_start, int *out_end) {
    *out_start = 0; *out_end = 0;
    if (!messages || messages->type != JSON_ARRAY || protect_first_n <= 0) return;
    int n = (int)json_len(messages);
    int first_non_system = (n > 0 && strcmp(json_get_str(json_get(messages,0),"role",""),"system")==0) ? 1 : 0;
    int end = first_non_system + protect_first_n + CC_RESTART_HANDOFF_PROBE_EXTRA_MESSAGES;
    if (end > n) end = n;
    *out_start = first_non_system;
    *out_end = end;
}

/* PoP: cc_effective_protect_first_n @ agent/context_compressor.py:_effective_protect_first_n */
int cc_effective_protect_first_n(int compression_count, int has_previous_summary,
                                 int protect_first_n) {
    if (compression_count >= 1 || has_previous_summary) return 0;
    return protect_first_n > 0 ? protect_first_n : 0;
}

/* ── _template_visible_role ───────────────────────────────────────────── */

static char *cc_str_strip(char *s) {
    if (!s) return NULL;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '\0') { s[0] = '\0'; return s; }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    if (start != s) memmove(s, start, strlen(start) + 1);
    return s;
}

static const char *cc_rfind_substr(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return NULL;
    size_t hlen = strlen(hay), nlen = strlen(needle);
    if (nlen > hlen) return NULL;
    const char *r = NULL;
    for (const char *p = hay; *p; p++) {
        if (strncmp(p, needle, nlen) == 0) r = p;
    }
    return r;
}

static int cc_str_is_blank(const char *s) {
    if (!s) return 1;
    for (const char *p = s; *p; p++) if (!isspace((unsigned char)*p)) return 0;
    return 1;
}

/* PoP: _template_visible_role @ agent/context_compressor.py:_template_visible_role */
const char *_template_visible_role(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return NULL;
    const char *role = json_get_str(message, "role", NULL);
    if (!role) return NULL;
    if (strcmp(role, "tool") == 0) return NULL;
    if (strcmp(role, "assistant") == 0) {
        json_t *tc = json_obj_get(message, "tool_calls");
        if (tc) return NULL;
    }
    return role;
}

/* ── _reasoning_details_text_chars ────────────────────────────────────── */

/* PoP: _reasoning_details_text_chars @ agent/context_compressor.py:_reasoning_details_text_chars */
long _reasoning_details_text_chars(const json_t *value) {
    if (!value) return 0;
    if (value->type == JSON_STRING) return (long)strlen(value->str_val);
    long total = 0;
    /* Python: isinstance(value, dict) -> [value]; list/tuple/set -> iterate.
     * A bare dict is treated as a single part. */
    if (value->type == JSON_OBJECT) {
        const char *text = NULL;
        json_t *t = json_obj_get(value, "thinking");
        if (t && t->type == JSON_STRING) text = t->str_val;
        if (!text) { t = json_obj_get(value, "text"); if (t && t->type == JSON_STRING) text = t->str_val; }
        if (!text) { t = json_obj_get(value, "summary"); if (t && t->type == JSON_STRING) text = t->str_val; }
        if (text) total += (long)strlen(text);
    } else if (value->type == JSON_ARRAY) {
        for (size_t i = 0; i < value->c.count; i++) {
            json_t *part = json_get(value, i);
            if (!part) continue;
            if (part->type == JSON_STRING) {
                total += (long)strlen(part->str_val);
            } else if (part->type == JSON_OBJECT) {
                const char *text = NULL;
                json_t *t = json_obj_get(part, "thinking");
                if (t && t->type == JSON_STRING) text = t->str_val;
                if (!text) { t = json_obj_get(part, "text"); if (t && t->type == JSON_STRING) text = t->str_val; }
                if (!text) { t = json_obj_get(part, "summary"); if (t && t->type == JSON_STRING) text = t->str_val; }
                if (text) total += (long)strlen(text);
            }
        }
    }
    return total;
}

/* ── _rolling_summary_from_marker ─────────────────────────────────────── */

/* PoP: _rolling_summary_from_marker @ agent/context_compressor.py:_rolling_summary_from_marker */
char *_rolling_summary_from_marker(const char *content) {
    /* Python: if not isinstance(content, str) or not content.strip(): return "" */
    if (!content || cc_str_is_blank(content)) return strdup("");
    const char *body = content;
    const char *idx = cc_rfind_substr(body, CC_HISTORICAL_TASK_HEADING);
    if (idx) body = idx + strlen(CC_HISTORICAL_TASK_HEADING);
    const char *end = strstr(body, CC_SUMMARY_END_MARKER);
    char *r;
    if (end) {
        size_t blen = (size_t)(end - body);
        r = (char *)malloc(blen + 1);
        if (!r) return NULL;
        memcpy(r, body, blen); r[blen] = '\0';
    } else {
        r = strdup(body);
    }
    if (!r) return NULL;
    return cc_str_strip(r);
}

/* ── _render_micro_marker_content ─────────────────────────────────────── */

/* PoP: _render_micro_marker_content @ agent/context_compressor.py:_render_micro_marker_content */
char *_render_micro_marker_content(const char *summary_text) {
    char *stripped = cc_str_strip(strdup(summary_text ? summary_text : ""));
    if (!stripped) stripped = strdup("");
    size_t len = strlen(CC_SUMMARY_PREFIX) + 2 + strlen(CC_HISTORICAL_TASK_HEADING) + 1
                 + strlen(stripped) + 2 + 1 + strlen(CC_SUMMARY_END_MARKER) + 1;
    char *r = (char *)malloc(len);
    if (!r) { free(stripped); return NULL; }
    snprintf(r, len, "%s\n\n%s\n%s\n\n%s",
             CC_SUMMARY_PREFIX, CC_HISTORICAL_TASK_HEADING, stripped,
             CC_SUMMARY_END_MARKER);
    free(stripped);
    return r;
}

/* ── _merge_adjacent_user_turns ────────────────────────────────────────── */

/* PoP: _merge_adjacent_user_turns @ agent/context_compressor.py:_merge_adjacent_user_turns */
/* Faithful port: builds a new list merging consecutive plain-text user turns */
/* (\n\n-joined), skipping tool/summary messages. Drops the api_content sidecar */
/* on merged messages (drop_stale_api_content). Returns the new count. The */
/* caller frees the returned array via json_free. */
int _merge_adjacent_user_turns(json_t *result, json_t **out_merged) {
    *out_merged = json_array();
    if (!result || result->type != JSON_ARRAY) return 0;
    json_t *merged = *out_merged;
    long prev_content_len = 0;
    const char *prev_content = NULL;
    int prev_mergeable = 0;
    for (size_t i = 0; i < result->c.count; i++) {
        json_t *msg = json_get(result, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        const char *role = json_get_str(msg, "role", "");
        int is_user = (strcmp(role, "user") == 0);
        int is_summary = json_obj_get(msg, CC_COMPRESSED_SUMMARY_METADATA_KEY) != NULL;
        json_t *mc = json_obj_get(msg, "content");
        int is_plain_text = mc && mc->type == JSON_STRING;
        /* Python merge condition: both user, no summary metadata, both content
         * are str. prev_content_len tracks the prev's content string (may be
         * empty but non-NULL — Python isinstance check, not truthiness). */
        if (is_user && !is_summary && is_plain_text && prev_mergeable) {
            /* merge into previous */
            json_t *last = json_get(merged, merged->c.count - 1);
            /* drop_stale_api_content: pop the api_content sidecar */
            json_obj_del(last, "api_content");
            const char *new_c = mc->str_val;
            char *joined;
            /* Python: prev_content + "\n\n" + new_content if both truthy,
             * else (prev_content or new_content). */
            if (prev_content_len > 0 && new_c && *new_c) {
                joined = malloc(prev_content_len + strlen(new_c) + 3);
                snprintf(joined, prev_content_len + strlen(new_c) + 3,
                         "%s\n\n%s", prev_content, new_c);
            } else {
                joined = strdup(prev_content_len > 0 ? prev_content
                                                     : (new_c ? new_c : ""));
            }
            json_set(last, "content", json_string(joined));
            free(joined);
            prev_content = json_get_str(last, "content", "");
            prev_content_len = prev_content ? strlen(prev_content) : 0;
            /* prev_mergeable stays true (still a user plain-text msg) */
        } else {
            json_append(merged, json_copy(msg));
            prev_mergeable = is_user && !is_summary && is_plain_text;
            if (prev_mergeable) {
                prev_content = json_get_str(msg, "content", "");
                prev_content_len = prev_content ? strlen(prev_content) : 0;
            } else {
                prev_content = NULL;
                prev_content_len = 0;
            }
        }
    }
    return (int)merged->c.count;
}

/* _db_persisted marker (agent/context_compressor.py module literal) is now
 * shared via context_compressor_constants.h (CC_DB_PERSISTED_MARKER). */

/* PoP: _fresh_compaction_message_copy @ agent/context_compressor.py:_fresh_compaction_message_copy */
/* Copy a message for compaction assembly without persistence markers
 * (_db_persisted stripped). Caller frees the returned json_t*. */
json_t *cc_fresh_compaction_message_copy(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return json_copy(msg);
    json_t *fresh = json_copy(msg);
    if (!fresh) return NULL;
    json_obj_del(fresh, CC_DB_PERSISTED_MARKER);
    return fresh;
}

/* PoP: _strip_persistence_markers @ agent/context_compressor.py:_strip_persistence_markers */
/* Enforce the compaction invariant: no assembled message carries a persistence
 * marker. Mutates in place. Returns count of strips performed. */
int cc_strip_persistence_markers(json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return 0;
    int stripped = 0;
    int n = (int)json_len(messages);
    for (int i = 0; i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        if (json_obj_get(msg, CC_DB_PERSISTED_MARKER)) {
            if (json_obj_del(msg, CC_DB_PERSISTED_MARKER))
                stripped++;
        }
    }
    return stripped;
}


/* ── batch 5: standalone handoff + user-turn predicates (pure) ─────────── */

/* PoP: cc_handoff_carries_live_user_content @ agent/context_compressor.py:_handoff_carries_live_user_content */
/* True when a summary-bearing row still carries a live user ask after the handoff is stripped. */
bool cc_handoff_carries_live_user_content(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return false;
    if (!cc_is_context_summary_message(message)) return false;
    return cc_strip_context_summary_handoff_message(message) != NULL;
}

/* PoP: cc_reference_handoff_would_drive_next_model_call @ agent/context_compressor.py:reference_handoff_would_drive_next_model_call */
/* True when the next model call would be driven ONLY by a handoff (no real
 * user/tool continuation after the last driving summary handoff). */
bool cc_reference_handoff_would_drive_next_model_call(const json_t *messages) {
    if (!messages) return false;
    int n = (int)json_len(messages);
    if (n == 0) return false;
    int last_driving_handoff = -1;
    for (int i = 0; i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg) continue;
        if (!cc_is_context_summary_message(msg)) continue;
        if (cc_handoff_carries_live_user_content(msg)) continue;
        last_driving_handoff = i;
    }
    if (last_driving_handoff < 0) return false;
    for (int i = last_driving_handoff + 1; i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg) continue;
        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "tool") == 0) return false;
        if (strcmp(role, "assistant") == 0 && json_obj_get(msg, "tool_calls")) return false;
        if (cc_is_actionable_user_turn(msg) && !cc_is_synthetic_compression_user_turn(msg))
            return false;
        if (cc_is_context_summary_message(msg) && cc_handoff_carries_live_user_content(msg))
            return false;
    }
    return true;
}

/* PoP: cc_is_user_originated_turn @ agent/context_compressor.py:is_user_originated_turn */
/* True for a genuine human user turn: role==user AND not a synthetic
 * compression/continuation marker (those are internal scaffolding, not a
 * real prompt the live model should answer). Delegates to the classmethod
 * equivalents ported in this TU (cc_is_synthetic_compression_user_turn,
 * cc_is_context_summary_message). */
bool cc_is_user_originated_turn(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return false;
    const char *role = json_get_str(message, "role", "");
    if (strcmp(role, "user") != 0) return false;
    if (cc_is_synthetic_compression_user_turn(message)) return false;
    return true;
}

/* PoP: _is_nonempty_user_turn @ agent/context_compressor.py:_is_nonempty_user_turn */
/* A user turn with non-whitespace content that is not a synthetic
 * compression/summary marker. Mirrors the module-level staticmethod:
 *   role == "user" and bool(_content_text_for_contains(content).strip())
 * plus the synthetic-summary exclusion so empty/placeholder user rows
 * (e.g. compression continuation markers) are not counted as real turns. */
bool cc_is_nonempty_user_turn(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return false;
    const char *role = json_get_str(message, "role", "");
    if (strcmp(role, "user") != 0) return false;
    if (cc_is_synthetic_compression_user_turn(message)) return false;
    json_t *content = json_obj_get(message, "content");
    if (!content) return false;
    char *text = context_compressor_content_text(content);
    bool nonempty = false;
    if (text) {
        for (const char *p = text; *p; p++)
            if (!isspace((unsigned char)*p)) { nonempty = true; break; }
        free(text);
    }
    return nonempty;
}

/* PoP: _reasoning_details_text_chars @ agent/context_compressor.py:_reasoning_details_text_chars */
/* Textual thinking chars inside a reasoning_details envelope; opaque envelope
 * blobs (signatures, redacted data, encrypted payloads) are skipped. */
int cc_reasoning_details_text_chars(const json_t *value) {
    if (!value) return 0;
    if (value->type == JSON_STRING) {
        const char *s = json_get_str(value, "", "");
        return (int)strlen(s);
    }
    if (value->type == JSON_NULL) return 0;
    int total = 0;
    if (value->type == JSON_ARRAY) {
        int n = (int)json_len(value);
        for (int i = 0; i < n; i++) {
            json_t *item = json_get(value, (size_t)i);
            if (!item) continue;
            if (item->type == JSON_STRING) {
                total += (int)strlen(json_get_str(item, "", ""));
            } else if (item->type == JSON_OBJECT) {
                size_t cnt = item->c.count;
                for (size_t k = 0; k < cnt; k++) {
                    if (item->c.keys[k] && strcmp(item->c.keys[k], "thinking") == 0 &&
                        item->c.items[k] && item->c.items[k]->type == JSON_STRING)
                        total += (int)strlen(json_get_str(item->c.items[k], "", ""));
                }
                /* if no thinking key found in this dict, count all string fields */
                if (total == 0) {
                    for (size_t k = 0; k < cnt; k++) {
                        if (item->c.items[k] && item->c.items[k]->type == JSON_STRING)
                            total += (int)strlen(json_get_str(item->c.items[k], "", ""));
                    }
                }
            }
        }
    } else if (value->type == JSON_OBJECT) {
        size_t cnt = value->c.count;
        for (size_t k = 0; k < cnt; k++) {
            if (value->c.keys[k] && strcmp(value->c.keys[k], "thinking") == 0 &&
                value->c.items[k] && value->c.items[k]->type == JSON_STRING)
                total += (int)strlen(json_get_str(value->c.items[k], "", ""));
        }
        if (total == 0)
            for (size_t k = 0; k < cnt; k++)
                if (value->c.items[k] && value->c.items[k]->type == JSON_STRING)
                    total += (int)strlen(json_get_str(value->c.items[k], "", ""));
    }
    return total;
}

/* ── batch 6: stateful compression-guard + preflight trackers ───────────── */
/* These mirror the ContextCompressor instance methods. They operate on small
 * value structs so the math is unit-testable without the full agent handle. */

/* Timeout cooldown ladder (seconds) — mirrors _TIMEOUT_COOLDOWN_LADDER. */
static const unsigned int CC_TIMEOUT_COOLDOWN_LADDER[] = { 60, 300, 900 };
#define CC_TIMEOUT_COOLDOWN_LADDER_LEN \
    (int)(sizeof(CC_TIMEOUT_COOLDOWN_LADDER) / sizeof(CC_TIMEOUT_COOLDOWN_LADDER[0]))

static double cc_monotonic_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: record_timeout_failure @ agent/context_compressor.py:record_timeout_failure */
void cc_record_timeout_failure(cc_compression_guards_t *g, const char *error) {
    if (!g) return;
    g->consecutive_timeout_failures++;
    if (g->consecutive_timeout_failures > CC_TIMEOUT_COOLDOWN_LADDER_LEN)
        g->consecutive_timeout_failures = CC_TIMEOUT_COOLDOWN_LADDER_LEN;
    unsigned int cooldown = CC_TIMEOUT_COOLDOWN_LADDER[g->consecutive_timeout_failures - 1];
    g->summary_failure_cooldown_until = cc_monotonic_seconds() + (double)cooldown;
    if (error) {
        free(g->last_summary_error);
        g->last_summary_error = strdup(error);
    }
}

/* PoP: _compression_block_reason @ agent/context_compressor.py:_compression_block_reason */
char *cc_compression_block_reason(const cc_compression_guards_t *g) {
    if (!g) return NULL;
    double remaining = g->summary_failure_cooldown_until - cc_monotonic_seconds();
    if (remaining > 0.0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "cooldown:%d", (int)(remaining + 0.5));
        return strdup(buf);
    }
    if (g->ineffective_compression_count >= 2 || g->fallback_compression_streak >= 2)
        return strdup("ineffective");
    return NULL;
}

/* PoP: _automatic_compression_blocked @ agent/context_compressor.py:_automatic_compression_blocked */
bool cc_automatic_compression_blocked(const cc_compression_guards_t *g) {
    return cc_compression_block_reason(g) != NULL;
}

/* PoP: should_compress_info @ agent/context_compressor.py:should_compress_info */
bool cc_should_compress_info(const cc_compression_guards_t *g, long prompt_tokens,
                             long threshold_tokens, bool *out_should, char **out_reason) {
    if (out_should) *out_should = false;
    if (out_reason) *out_reason = NULL;
    if (prompt_tokens < threshold_tokens) return false;
    char *reason = cc_compression_block_reason(g);
    if (reason) {
        if (out_reason) *out_reason = reason;
        else free(reason);
        return true; /* over threshold but blocked */
    }
    if (out_should) *out_should = true;
    return true;
}

/* note_request_rough_estimate / should_defer_preflight_to_real_usage are
 * intentionally NOT re-declared here: port_context_compressor_ports.c owns
 * the canonical PoP-tagged implementations (cc_should_defer_preflight_to_real_usage
 * is a real gate there). The cc_request_tracker_t math above is available for
 * the canonical version to adopt if it wants the projection-based variant. */

/* ── batch 7: module-level pure helpers (previously REAL_GAP) ───────────── */

/* PoP: _template_visible_role @ agent/context_compressor.py:_template_visible_role */
/* Role as counted by strict chat-template alternation checks. Returns NULL for
 * messages the alternation check skips (tool results, and assistant messages
 * carrying tool_calls). */
const char *cc_template_visible_role(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return NULL;
    const char *role = json_get_str(message, "role", "");
    if (strcmp(role, "tool") == 0) return NULL;
    if (strcmp(role, "assistant") == 0 && json_obj_get(message, "tool_calls"))
        return NULL;
    return role;
}

/* PoP: cc_is_clarify_non_response_sentinel @ agent/context_compressor.py:_is_clarify_non_response_sentinel */
/* True when a clarify user_response is runtime sentinel prose (timeout /
 * no-user), not an actual user answer. For lists, ANY sentinel item poisons
 * the whole response. */
static const char *CC_CLARIFY_NON_RESPONSE_PREFIXES[] = {
    "The user did not provide a response",
    "[user did not respond",
    "[clarify prompt could not be delivered",
    "[oneshot mode:",
    NULL,
};

bool cc_is_clarify_non_response_sentinel(const json_t *response) {
    if (!response) return false;
    if (response->type == JSON_STRING) {
        const char *s = json_get_str(response, "", "");
        while (*s && isspace((unsigned char)*s)) s++;
        for (int i = 0; CC_CLARIFY_NON_RESPONSE_PREFIXES[i]; i++)
            if (strncmp(s, CC_CLARIFY_NON_RESPONSE_PREFIXES[i],
                        strlen(CC_CLARIFY_NON_RESPONSE_PREFIXES[i])) == 0)
                return true;
        return false;
    }
    if (response->type == JSON_ARRAY) {
        int n = (int)json_len(response);
        for (int i = 0; i < n; i++) {
            json_t *item = json_get(response, (size_t)i);
            if (!item || item->type != JSON_STRING) continue;
            const char *s = json_get_str(item, "", "");
            while (*s && isspace((unsigned char)*s)) s++;
            for (int k = 0; CC_CLARIFY_NON_RESPONSE_PREFIXES[k]; k++)
                if (strncmp(s, CC_CLARIFY_NON_RESPONSE_PREFIXES[k],
                            strlen(CC_CLARIFY_NON_RESPONSE_PREFIXES[k])) == 0)
                    return true;
        }
        return false;
    }
    return false;
}

/* PoP: cc_last_assistant_index @ agent/context_compressor.py:_last_assistant_index */
/* Index of the newest assistant message, or -1. */
int cc_last_assistant_index(const json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return -1;
    int n = (int)json_len(messages);
    for (int i = n - 1; i >= 0; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg && msg->type == JSON_OBJECT &&
            strcmp(json_get_str(msg, "role", ""), "assistant") == 0)
            return i;
    }
    return -1;
}

/* PoP: cc_prune_stale_reasoning_replay @ agent/context_compressor.py:_prune_stale_reasoning_replay */
/* Strip stale per-turn replay items (codex_reasoning_items) from assistant
 * messages belonging to turns older than the active one. Turn boundary is the
 * LAST USER message; everything after it keeps its items. Native compaction
 * checkpoints (type=="compaction") are exempt. Mutates in place; returns the
 * count of messages pruned. */
static const char *CC_STALE_REPLAY_PRUNE_KEYS[] = {
    "codex_reasoning_items",
    NULL,
};

int cc_prune_stale_reasoning_replay(json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return 0;
    int n = (int)json_len(messages);
    int last_user_idx = -1;
    for (int i = n - 1; i >= 0; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg && msg->type == JSON_OBJECT &&
            strcmp(json_get_str(msg, "role", ""), "user") == 0) {
            last_user_idx = i;
            break;
        }
    }
    if (last_user_idx < 0) return 0; /* fail open toward correctness */

    int pruned = 0;
    for (int i = 0; i < last_user_idx; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        if (strcmp(json_get_str(msg, "role", ""), "assistant") != 0) continue;
        for (int k = 0; CC_STALE_REPLAY_PRUNE_KEYS[k]; k++) {
            json_t *items = json_obj_get(msg, CC_STALE_REPLAY_PRUNE_KEYS[k]);
            if (!items || items->type != JSON_ARRAY || json_len(items) == 0)
                continue;
            int in = (int)json_len(items);
            int kept_count = 0;
            json_t *kept = json_array();
            if (!kept) continue;
            for (int j = 0; j < in; j++) {
                json_t *item = json_get(items, (size_t)j);
                if (item && item->type == JSON_OBJECT &&
                    strcmp(json_get_str(item, "type", ""), "compaction") == 0) {
                    json_append(kept, json_copy(item));
                    kept_count++;
                }
            }
            if (kept_count == in) {
                json_free(kept);
                continue; /* nothing stale in this sidecar */
            }
            if (kept_count > 0)
                json_set(msg, CC_STALE_REPLAY_PRUNE_KEYS[k], kept);
            else {
                json_free(kept);
                json_obj_del(msg, CC_STALE_REPLAY_PRUNE_KEYS[k]);
            }
            pruned++;
        }
    }
    return pruned;
}




/* ── batch 8: ContextCompressor budget property accessors (pure math) ──── */
/* These re-implement the Python @property getters/setters as pure functions
 * over cc_compressor_budget_t, reusing the threshold-math helpers above. The
 * ContextCompressor struct (in the orchestration TU) owns a budget_t; these
 * let it delegate the lazy-resolution math without duplicating logic. */

/* PoP: _resolve_context_length @ agent/context_compressor.py:_resolve_context_length */
long cc_budget_resolve_context_length(cc_compressor_budget_t *b) {
    if (!b) return -1;
    if (b->resolved_context_length != -1)
        return b->resolved_context_length;
    b->resolved_context_length = get_model_context_length(
        b->model, b->base_url, b->api_key,
        b->config_context_length, b->provider);
    /* Small-context threshold floor (raise-only). */
    b->threshold_percent = cc_effective_threshold_percent(
        b->resolved_context_length, b->base_threshold_percent);
    return b->resolved_context_length;
}

/* PoP: context_length @ agent/context_compressor.py:context_length */
long cc_budget_context_length_get(cc_compressor_budget_t *b) {
    return cc_budget_resolve_context_length(b);
}

/* context_length.setter — invalidate derived budgets unless same value. */
void cc_budget_context_length_set(cc_compressor_budget_t *b, long value) {
    if (!b) return;
    if (value == b->resolved_context_length) return; /* no-op guard (#32221) */
    b->resolved_context_length = value;
    double base = b->base_threshold_percent;
    if (base != 0.0) /* bare test instances never ran __init__ */
        b->threshold_percent = cc_effective_threshold_percent(value, base);
    b->threshold_tokens = -1;
    b->tail_token_budget = -1;
    b->max_summary_tokens = -1;
}

/* PoP: threshold_tokens @ agent/context_compressor.py:threshold_tokens */
long cc_budget_threshold_tokens_get(cc_compressor_budget_t *b) {
    if (!b) return -1;
    if (b->threshold_tokens != -1) return b->threshold_tokens;
    cc_budget_resolve_context_length(b); /* sets floored threshold_percent */
    b->threshold_tokens = cc_compute_threshold_tokens(
        b->resolved_context_length, b->threshold_percent, b->max_tokens);
    b->threshold_tokens = cc_apply_threshold_tokens_cap(
        b->threshold_tokens, b->threshold_tokens_cap, b->resolved_context_length);
    return b->threshold_tokens;
}
void cc_budget_threshold_tokens_set(cc_compressor_budget_t *b, long value) {
    if (b) b->threshold_tokens = value;
}

/* PoP: tail_token_budget @ agent/context_compressor.py:tail_token_budget */
long cc_budget_tail_token_budget_get(cc_compressor_budget_t *b) {
    if (!b) return -1;
    if (b->tail_token_budget != -1) return b->tail_token_budget;
    b->tail_token_budget =
        (long)(cc_budget_threshold_tokens_get(b) * b->summary_target_ratio);
    return b->tail_token_budget;
}
void cc_budget_tail_token_budget_set(cc_compressor_budget_t *b, long value) {
    if (b) b->tail_token_budget = value;
}

/* PoP: max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
long cc_budget_max_summary_tokens_get(cc_compressor_budget_t *b) {
    if (!b) return -1;
    if (b->max_summary_tokens != -1) return b->max_summary_tokens;
    long ctx = cc_budget_resolve_context_length(b);
    long by_ratio = (long)(ctx * 0.05);
    long ceiling = CC_MAX_SUMMARY_TOKENS_CEILING; /* 10,000 — _SUMMARY_TOKENS_CEILING */
    b->max_summary_tokens = by_ratio < ceiling ? by_ratio : ceiling;
    return b->max_summary_tokens;
}
void cc_budget_max_summary_tokens_set(cc_compressor_budget_t *b, long value) {
    if (b) b->max_summary_tokens = value;
}

/* ── note_request_rough_estimate (canonical impl, pure tracker math) ───── */
/* PoP: cc_note_request_rough_estimate @ agent/context_compressor.py:note_request_rough_estimate */
void cc_note_request_rough_estimate(cc_request_tracker_t *t, long rough_tokens) {
    if (!t) return;
    if (rough_tokens < 0) rough_tokens = 0;
    t->pending_request_rough_tokens = rough_tokens;
}

/* ── batch 4: micro-compaction marker helpers (cc_-prefixed wrappers) ───── */
/* These cc_-prefixed wrappers are the call surface the orchestration layer
 * (port_agent_context_compressor.c) uses; each delegates to the faithful
 * underscore-prefixed leaf port. */

/* PoP: _render_micro_marker_content @ agent/context_compressor.py:_render_micro_marker_content */
char *cc_render_micro_marker_content(const char *summary_text) {
    return _render_micro_marker_content(summary_text);
}

/* PoP: _rolling_summary_from_marker @ agent/context_compressor.py:_rolling_summary_from_marker */
char *cc_rolling_summary_from_marker(const char *content) {
    return _rolling_summary_from_marker(content);
}

/* PoP: _cursor_after_splice @ agent/context_compressor.py:_cursor_after_splice */
int cc_cursor_after_splice(const json_t *result, int fallback) {
    /* Scan backwards for a micro-compaction summary marker; return its index+1.
     * Falls back to `fallback` when none is found (mirrors the Python: the
     * marker row, if present, is the last summary marker's successor). */
    if (!result || result->type != JSON_ARRAY) return fallback;
    int n = (int)json_len(result);
    for (int i = n - 1; i >= 0; i--) {
        json_t *m = json_get(result, (size_t)i);
        if (m && m->type == JSON_OBJECT &&
            json_obj_get(m, CC_COMPRESSED_SUMMARY_METADATA_KEY) &&
            json_obj_get(m, CC_MICRO_COMPACT_MARKER_KEY))
            return i + 1;
    }
    return fallback;
}

/* PoP: _merge_adjacent_user_turns @ agent/context_compressor.py:_merge_adjacent_user_turns */
json_t *cc_merge_adjacent_user_turns(const json_t *result) {
    json_t *m = NULL;
    int rc = _merge_adjacent_user_turns((json_t *)result, &m);
    (void)rc;
    return m ? m : json_array(); /* return empty array on failure, matching caller's `merged ? merged : json_array()` */
}
