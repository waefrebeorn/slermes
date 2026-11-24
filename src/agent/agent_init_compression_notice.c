/*
 * agent_init_compression_notice.c - C11 port of upstream
 * agent/agent_init_compression_notice.py (gate w26_agentinitc_084bfa).
 *
 * PoP: w26_agentinitc_084bfa_notice @ agent/agent_init_compression_notice.py:_build_codex_gpt5_autoraise_notice
 * PoP: w26_agentinitc_084bfa_state @ agent/agent_init_compression_notice.py:_codex_gpt55_autoraise_notice_state
 * PoP: w26_agentinitc_084bfa_emit @ agent/agent_init_compression_notice.py:_emit_compression_summary
 * PoP: w26_agentinitc_084bfa_constants @ agent/agent_init_compression_notice.py:<banner vocabulary>
 *
 * Everything here is a pure function of values: the notice text from the
 * autoraise dict and the live-resolved window, the marker identity key, and the
 * once-per-profile decision the init makes. The marker file read/write, the
 * agent, its compressor and the console are seams (the decision takes "seen"
 * and the printed line as fixed inputs / outputs).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_agentinitc_084bfa.h"

/* Static fallback windows when no live window was resolved. */
#define AIC_DEFAULT_MODEL "gpt-5.4/5.5"
#define AIC_SPARK_MODEL "gpt-5.3-codex-spark"
#define AIC_SPARK_CAP "128K"
#define AIC_DEFAULT_CAP "272K"
#define AIC_OPTOUT "  Opt back out: hermes config set compression.codex_gpt55_autoraise false"

/* Python truthiness of a JSON value. */
static int aic_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python round(): ties to even, returning an int. */
static long long aic_round(double x) {
    return (long long)nearbyint(x);
}

/* str.strip().lower().rsplit("/", 1)[-1] over an already-materialized string. */
static char *aic_normalize_model(const char *raw) {
    const char *start = raw, *end, *slash, *p;
    char *out;
    size_t len, i;
    if (!raw) raw = "";
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r'
           || *start == '\f' || *start == '\v') start++;
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n'
                           || end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v')) end--;
    slash = NULL;
    for (p = start; p < end; p++) if (*p == '/') slash = p;
    if (slash) start = slash + 1;
    len = (size_t)(end - start);
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    for (i = 0; i < len; i++) {
        char c = start[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[len] = '\0';
    return out;
}

/* str(value) for the shapes the notice prints; "" for None/absent. */
static const char *aic_str_of(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        double n = v->num_val;
        if (n == (double)(long long)n) snprintf(buf, cap, "%lld", (long long)n);
        else snprintf(buf, cap, "%g", n);
        return buf;
    }
    return "";
}

static char *aic_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *aic_concat(const char *a, const char *b) {
    size_t na = strlen(a), nb = strlen(b);
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    memcpy(out, a, na);
    memcpy(out + na, b, nb + 1);
    return out;
}

/* The model the notice names: autoraise.get("model") or the default, normalized. */
static char *aic_model(const json_t *autoraise, const char *fallback) {
    const json_t *m = autoraise ? json_obj_get(autoraise, "model") : NULL;
    char buf[64];
    const char *raw;
    char *norm;
    if (!aic_truthy(m)) raw = fallback;
    else raw = aic_str_of(m, buf, sizeof(buf));
    norm = aic_normalize_model(raw);
    return norm;
}

/* round(context_length / 1000) as "<n>K", else the static fallback window. */
static char *aic_cap(const char *model, const json_t *context_length) {
    char buf[64];
    if (context_length && context_length->type == JSON_NUMBER && context_length->num_val > 0.0) {
        long long k = aic_round(context_length->num_val / 1000.0);
        snprintf(buf, sizeof(buf), "%lldK", k);
        return aic_dup(buf);
    }
    if (model && !strncmp(model, AIC_SPARK_MODEL, strlen(AIC_SPARK_MODEL)))
        return aic_dup(AIC_SPARK_CAP);
    return aic_dup(AIC_DEFAULT_CAP);
}

json_t *w26_agentinitc_084bfa_notice(const json_t *req) {
    const json_t *autoraise = req ? json_obj_get(req, "autoraise") : NULL;
    const json_t *context_length = req ? json_obj_get(req, "context_length") : NULL;
    const json_t *from_v, *to_v;
    char *model, *cap, *out;
    json_t *node;
    long long from_pct, to_pct;
    size_t need;

    if (!autoraise || autoraise->type != JSON_OBJECT) autoraise = NULL;
    model = aic_model(autoraise, AIC_DEFAULT_MODEL);
    if (!model) return json_null();
    cap = aic_cap(model, context_length);
    from_v = autoraise ? json_obj_get(autoraise, "from") : NULL;
    to_v = autoraise ? json_obj_get(autoraise, "to") : NULL;
    from_pct = aic_round(json_get_num(autoraise, "from", 0.0) * 100.0);
    to_pct = aic_round(json_get_num(autoraise, "to", 0.0) * 100.0);
    (void)from_v; (void)to_v;

    need = strlen(model) + strlen(cap) + 512;
    out = (char *)malloc(need);
    if (!out) { free(model); free(cap); return json_null(); }
    snprintf(out, need,
             "ℹ Codex %s caps context at %s, so auto-compaction was raised "
             "to %lld%% (from %lld%%) to use more of the window before "
             "summarizing.\n%s",
             model, cap, to_pct, from_pct, AIC_OPTOUT);
    node = json_string(out);
    free(out);
    free(model);
    free(cap);
    return node;
}

json_t *w26_agentinitc_084bfa_state(const json_t *req) {
    const json_t *autoraise = req ? json_obj_get(req, "autoraise") : NULL;
    char *model, *suffix, *out;
    long long from_pct, to_pct;
    size_t need;

    if (!autoraise || autoraise->type != JSON_OBJECT) return json_null();
    model = aic_model(autoraise, "");
    if (!model) return json_null();
    from_pct = aic_round(json_get_num(autoraise, "from", 0.0) * 100.0);
    to_pct = aic_round(json_get_num(autoraise, "to", 0.0) * 100.0);
    need = strlen(model) + 64;
    suffix = (char *)malloc(need);
    if (!suffix) { free(model); return json_null(); }
    snprintf(suffix, need, ":%lld:%lld", from_pct, to_pct);
    out = aic_concat(model, suffix);
    free(suffix);
    free(model);
    return out ? json_string(out) : json_null();
}

/* Python's {:,} grouping for a non-negative integer. */
static char *aic_group(long long v) {
    char raw[32], buf[48];
    size_t n, i, j = 0;
    snprintf(raw, sizeof(raw), "%lld", v);
    n = strlen(raw);
    for (i = 0; i < n; i++) {
        if (i && ((n - i) % 3) == 0) buf[j++] = ',';
        buf[j++] = raw[i];
    }
    buf[j] = '\0';
    return aic_dup(buf);
}

json_t *w26_agentinitc_084bfa_emit(const json_t *req) {
    const json_t *autoraise = req ? json_obj_get(req, "autoraise") : NULL;
    const json_t *context_length = req ? json_obj_get(req, "context_length") : NULL;
    const json_t *eff_cap = req ? json_obj_get(req, "eff_cap") : NULL;
    const json_t *tokens_cap = req ? json_obj_get(req, "tokens_cap") : NULL;
    int enabled = req ? json_get_bool(req, "enabled", false) : 0;
    int notice_enabled = req ? json_get_bool(req, "notice_enabled", false) : 0;
    int seen = req ? json_get_bool(req, "seen", false) : 0;
    int quiet = req ? json_get_bool(req, "quiet_mode", false) : 0;
    double threshold_tokens = req ? json_get_num(req, "threshold_tokens", 0.0) : 0.0;
    double threshold_percent = req ? json_get_num(req, "threshold_percent", 0.0) : 0.0;
    json_t *out = json_object();
    json_t *warning = json_null();

    if (autoraise && autoraise->type != JSON_OBJECT) autoraise = NULL;

    if (aic_truthy(autoraise) && enabled && notice_enabled && !seen) {
        json_free(warning);
        warning = w26_agentinitc_084bfa_notice(req);
        if (!warning) warning = json_null();
    }
    json_set(out, "warning", warning);

    if (quiet) {
        json_set(out, "line", json_null());
        return out;
    }
    {
        char *len_s = aic_group((long long)(context_length ? context_length->num_val : 0.0));
        char *tok_s = aic_group((long long)threshold_tokens);
        char *line;
        size_t need;
        int cap_binds = eff_cap && eff_cap->type == JSON_NUMBER
                        && eff_cap->num_val == threshold_tokens
                        && tokens_cap && tokens_cap->type == JSON_NUMBER;
        char *cap_s = cap_binds ? aic_group((long long)tokens_cap->num_val) : NULL;
        char cap_note[80];
        if (cap_binds && cap_s) snprintf(cap_note, sizeof(cap_note), " (capped at %s tokens)", cap_s);
        else cap_note[0] = '\0';
        need = strlen(len_s) + strlen(tok_s) + strlen(cap_note) + 256;
        line = (char *)malloc(need);
        if (!line) {
            free(len_s); free(tok_s); free(cap_s);
            json_set(out, "line", json_null());
            return out;
        }
        if (enabled) {
            snprintf(line, need,
                     "📊 Context limit: %s tokens (compress at %lld%% = %s%s)",
                     len_s, (long long)(threshold_percent * 100.0), tok_s, cap_note);
        } else {
            snprintf(line, need, "📊 Context limit: %s tokens (auto-compression disabled)", len_s);
        }
        json_set(out, "line", json_string(line));
        free(line);
        free(len_s); free(tok_s); free(cap_s);
    }
    return out;
}

json_t *w26_agentinitc_084bfa_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_model", json_string(AIC_DEFAULT_MODEL));
    json_set(o, "spark_model", json_string(AIC_SPARK_MODEL));
    json_set(o, "spark_cap", json_string(AIC_SPARK_CAP));
    json_set(o, "default_cap", json_string(AIC_DEFAULT_CAP));
    json_set(o, "optout", json_string(AIC_OPTOUT));
    json_set(o, "marker_name", json_string(".codex_gpt55_autoraise_notice"));
    json_set(o, "config_key", json_string("compression.codex_gpt55_autoraise"));
    return o;
}
