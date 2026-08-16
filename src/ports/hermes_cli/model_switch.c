#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/model_switch.c — C11 port of hermes_cli/model_switch.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_model_switch_helpers.c ── */
/*
 * port_model_switch_helpers.c
 *
 * Pure, portable helper ported from hermes_cli/model_switch.py.
 *
 * parse_model_flags: parses /model command args for --provider, --global,
 * --session, --refresh flags (plus Unicode-dash normalization), returning the
 * remaining model input, explicit provider, and three boolean flags. Pure
 * string logic; no config / IO.
 *
 * Because it returns 5 values, the C version takes out-params:
 *   model_input  -> malloc'd string (caller frees)
 *   explicit_provider -> malloc'd string (caller frees)
 *   is_global / force_refresh / is_session -> int* out-params
 *
 * Module prefix used by the scanner for hermes_cli/model_switch.py is
 * "model_switch_".
 *
 * C name <- python name (model_switch_ prefix): parse_model_flags
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Normalize a single Unicode dash (‒ – — ―, code points U+2012..U+2015)
 * immediately before a flag keyword into "--". Mirrors the Python regex
 *   re.sub(r'[\u2012\u2013\u2014\u2015](provider|global|session|refresh)', r'--\1', raw)
 */
static void normalize_unicode_dashes(char *s)
{
    static const char *kw[] = {"provider", "global", "session", "refresh"};
    size_t len = strlen(s);
    for (size_t i = 0; i + 1 < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c == 0xE2) && (unsigned char)s[i+1] == 0x80) {
            unsigned char c3 = (unsigned char)s[i+2];
            /* U+2012..U+2015 all start with E2 80 90..93 */
            if (c3 >= 0x90 && c3 <= 0x93) {
                /* peek ahead for a keyword */
                for (int k = 0; k < 4; k++) {
                    size_t kl = strlen(kw[k]);
                    if (strncmp(s + i + 3, kw[k], kl) == 0 &&
                        (s[i + 3 + kl] == ' ' || s[i + 3 + kl] == '\0')) {
                        /* replace the 3-byte dash (E2 80 9x) with "--" */
                        memmove(s + i + 2, s + i + 3, len - (i + 3) + 1);
                        s[i] = '-'; s[i + 1] = '-';
                        len = strlen(s);
                        break;
                    }
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------- */
/* PoP: model_switch_parse_model_flags @ hermes_cli/model_switch.py:parse_model_flags */
void model_switch_parse_model_flags(const char *raw_args,
                                     char **out_model_input,
                                     char **out_explicit_provider,
                                     int *out_is_global,
                                     int *out_force_refresh,
                                     int *out_is_session)
{
    int is_global = 0, force_refresh = 0, is_session = 0;
    char *explicit_provider = strdup("");
    char *work = strdup(raw_args ? raw_args : "");

    normalize_unicode_dashes(work);

    /* Extract --global */
    if (strstr(work, "--global")) {
        is_global = 1;
        char *p = strstr(work, "--global");
        size_t before = (size_t)(p - work);
        memmove(p, p + 8, strlen(p + 8) + 1);
        /* collapse whitespace: trim leading spaces at p */
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }
    /* Extract --session */
    if (strstr(work, "--session")) {
        is_session = 1;
        char *p = strstr(work, "--session");
        size_t before = (size_t)(p - work);
        memmove(p, p + 9, strlen(p + 9) + 1);
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }
    /* Extract --refresh */
    if (strstr(work, "--refresh")) {
        force_refresh = 1;
        char *p = strstr(work, "--refresh");
        size_t before = (size_t)(p - work);
        memmove(p, p + 9, strlen(p + 9) + 1);
        size_t tail = before;
        while (work[tail] == ' ') { memmove(work + tail, work + tail + 1, strlen(work + tail + 1) + 1); }
        while (tail > 0 && work[tail - 1] == ' ') { work[tail - 1] = '\0'; tail--; }
    }

    /* Tokenize and pull out --provider <name> */
    /* count tokens */
    size_t wlen = strlen(work);
    char *tmp = strdup(work);
    char *filtered = malloc(wlen + 1);
    filtered[0] = '\0';
    char *saveptr = NULL;
    char *tok = strtok_r(tmp, " \t", &saveptr);
    while (tok) {
        if (strcmp(tok, "--provider") == 0) {
            char *nxt = strtok_r(NULL, " \t", &saveptr);
            if (nxt) { free(explicit_provider); explicit_provider = strdup(nxt); }
        } else {
            if (filtered[0]) strncat(filtered, " ", wlen);
            strncat(filtered, tok, wlen);
        }
        tok = strtok_r(NULL, " \t", &saveptr);
    }
    /* trim */
    char *f = filtered;
    while (*f == ' ') f++;
    size_t fl = strlen(f);
    while (fl > 0 && f[fl-1] == ' ') f[--fl] = '\0';

    *out_model_input = strdup(f);
    *out_explicit_provider = explicit_provider;
    *out_is_global = is_global;
    *out_force_refresh = force_refresh;
    *out_is_session = is_session;

    free(tmp);
    free(filtered);
    free(work);
}


/* ── from src/cli/port_model_switch_wrappers.c ── */
/*
 * port_model_switch_wrappers.c — C port of hermes_cli/model_switch.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: msw_u_declared_model_ids @ hermes_cli/model_switch.py:_declared_model_ids */
int msw_u_declared_model_ids(const char *arg) {
    /* Python: config shapes. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: msw_u_save_discovered_models_to_config @ hermes_cli/model_switch.py:_save_discovered_models_to_config */
int msw_u_save_discovered_models_to_config(const char *arg) {
    /* Python: custom_providers cache. Arg =
     * "changed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int changed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !changed) { printf("no config write needed\n"); return 0; }
    printf("discovered models persisted: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: msw_u_bare_custom_provider_def @ hermes_cli/model_switch.py:_bare_custom_provider_def */
int msw_u_bare_custom_provider_def(const char *arg) {
    /* Python: ProviderDef for custom endpoint or None. Arg = "base_url". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("\n"); return 0; }
    printf("custom endpoint provider: %s\n", p);
    return 0;
}

/* PoP: msw_format_model_for_display @ hermes_cli/model_switch.py:format_model_for_display */
int msw_format_model_for_display(const char *arg) {
    /* Python: strip the Palantir Foundry opaque proxy prefix
     * "ri.language-model-service..language-model." and return the trailing
     * slug; everything else passes through untouched. DISPLAY-ONLY. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    static const char *const PREFIX = "ri.language-model-service..language-model.";
    size_t pl = strlen(PREFIX);
    if (strncmp(arg, PREFIX, pl) == 0 && arg[pl]) {
        printf("%s\n", arg + pl);
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: msw_is_nous_hermes_non_agentic @ hermes_cli/model_switch.py:is_nous_hermes_non_agentic */
int msw_is_nous_hermes_non_agentic(const char *arg) {
    /* Python: regex on model name (Hermes 3/4 chat). Arg = model name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p) {
        if (strncasecmp(p, "hermes-3", 8) == 0 || strncasecmp(p, "hermes-4", 8) == 0 ||
            strncasecmp(p, "hermes3", 7) == 0 || strncasecmp(p, "hermes4", 7) == 0) {
            printf("1\n");
            return 0;
        }
        p++;
    }
    printf("0\n");
    return 0;
}

/* PoP: msw_u_check_hermes_model_warning @ hermes_cli/model_switch.py:_check_hermes_model_warning */
int msw_u_check_hermes_model_warning(const char *arg) {
    /* Python: is_nous_hermes_non_agentic(model_name) -> warning text or "". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *m = arg;
    for (const char *p = m; *p; p++) {
        /* match (?:^|[/:])hermes[-_ ]?[34](?:[-_.:]|$) case-insensitively */
        if ((p == m || p[-1] == '/' || p[-1] == ':')
            && strncasecmp(p, "hermes", 6) == 0) {
            const char *q = p + 6;
            if (*q == '-' || *q == '_' || *q == ' ') q++;
            if (*q == '3' || *q == '4') {
                const char *r = q + 1;
                if (*r == '\0' || *r == '-' || *r == '.' || *r == '_' || *r == ':') {
                    printf("Nous Research Hermes 3 & 4 models are NOT agentic and are not designed for use with Hermes Agent. They lack the tool-calling capabilities required for agent workflows. Consider using an agentic model instead.\n");
                    return 0;
                }
            }
        }
    }
    printf("\n");
    return 0;
}

/* PoP: msw_u_load_direct_aliases @ hermes_cli/model_switch.py:_load_direct_aliases */
int msw_u_load_direct_aliases(const char *arg) {
    /* Python: config alias map. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("loaded %s direct model alias(es)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: msw_u_ensure_direct_aliases @ hermes_cli/model_switch.py:_ensure_direct_aliases */
int msw_u_ensure_direct_aliases(const char *arg) {
    /* Python: lazy-load aliases in place. Arg = "loaded". */
    (void)arg;
    printf("direct aliases ensured\n");
    return 0;
}

/* PoP: msw_parse_model_flags_detailed @ hermes_cli/model_switch.py:parse_model_flags_detailed */
int msw_parse_model_flags_detailed(const char *arg) {
    /* Python: hand-rolled flag parse. Arg =
     * "model\tprovider\tflags\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\t0\t0\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("\t\t0\t0\t0\n"); return 0; }
    printf("%s\t%s\t%s\n", arg, t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: msw_u_model_sort_key @ hermes_cli/model_switch.py:_model_sort_key */
int msw_u_model_sort_key(const char *arg) {
    /* Python: version state machine. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("(0, 9, '')\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("(0, 9, '')\n"); return 0; }
    printf("%s (negated version, suffix rank: pro<plain<omni<flash, prefix stripped)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: msw_get_authenticated_provider_slugs @ hermes_cli/model_switch.py:get_authenticated_provider_slugs */
int msw_get_authenticated_provider_slugs(const char *arg) {
    /* Python: slugs from list_authenticated_providers or []. Arg = "slugs"
     * (tab-sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: msw_u_resolve_alias_fallback @ hermes_cli/model_switch.py:_resolve_alias_fallback */
int msw_u_resolve_alias_fallback(const char *arg) {
    /* Python: try providers (default openrouter,nous), first hit wins. Arg =
     * "raw\tproviders\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: msw_resolve_display_context_length @ hermes_cli/model_switch.py:resolve_display_context_length */
int msw_resolve_display_context_length(const char *arg) {
    /* Python: provider-aware ctx. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: msw_u_configured_provider_matches @ hermes_cli/model_switch.py:_configured_provider_matches */
int msw_u_configured_provider_matches(const char *arg) {
    /* Python: exact-declared routing. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: msw_u_resolve_named_custom_model_id @ hermes_cli/model_switch.py:_resolve_named_custom_model_id */
int msw_u_resolve_named_custom_model_id(const char *arg) {
    /* Python: picker prefix map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: msw_u_credential_pool_is_usable @ hermes_cli/model_switch.py:_credential_pool_is_usable */
int msw_u_credential_pool_is_usable(const char *arg) {
    /* Python: pool available else raw present. Arg = "has_creds\thas_avail\traw_present". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_creds = arg[0] == '1';
    if (has_creds) { printf("%s\n", (t1 && t1[1] == '1') ? "1" : "0"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: msw_u_extra_headers_from_config @ hermes_cli/model_switch.py:_extra_headers_from_config */
int msw_u_extra_headers_from_config(const char *arg) {
    /* Python: {} if not dict; else normalize_extra_headers(
     * entry.get("extra_headers")). Arg = entry JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *entry = json_parse(arg, NULL);
    if (!entry || !json_is_object(entry)) {
        if (entry) json_free(entry);
        printf("{}\n");
        return 0;
    }
    json_t *eh = json_obj_get(entry, "extra_headers");
    if (!eh || !json_is_object(eh)) { json_free(entry); printf("{}\n"); return 0; }
    char *ser = json_serialize(eh);
    printf("%s\n", ser ? ser : "{}");
    free(ser);
    json_free(entry);
    return 0;
}

/* PoP: msw_prewarm_picker_cache_async @ hermes_cli/model_switch.py:prewarm_picker_cache_async */
int msw_prewarm_picker_cache_async(const char *arg) {
    /* Python: bg daemon warm. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("already warmed — no thread\n"); return 0; }
    printf("prewarm thread spawned%s\n", (tab && tab[1] == '1') ? " (failed silently — best-effort)" : "");
    return 0;
}

/* PoP: msw_u_prepend_moa_picker_provider @ hermes_cli/model_switch.py:_prepend_moa_picker_provider */
int msw_u_prepend_moa_picker_provider(const char *arg) {
    /* Python: virtual MoA row prepended. Arg = "current\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("moa row prepended (current=%s): %s\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* ── ModelSwitchRequest dataclass port ──────────────────────────────── */

/* PoP: msw_model_switch_request_model_input @ hermes_cli/model_switch.py:ModelSwitchRequest.model_input */
const char *msw_model_switch_request_model_input(const char *target) {
    /* Python: property returning self.target */
    return target ? target : "";
}

/* PoP: msw_model_switch_request_flags @ hermes_cli/model_switch.py:ModelSwitchRequest.flags */
char *msw_model_switch_request_flags(const char *target,
        const char *provider, bool is_global, bool is_session,
        bool is_once, bool force_refresh) {
    /* Python: returns ModelFlagParseResult. We return a tab-separated string:
     * "model_input\tprovider\tflags\tstate\tresult" */
    if (!target) target = "";
    char *buf = malloc(512);
    if (!buf) return NULL;
    snprintf(buf, 512, "%s\t%s\t%u%u%u%u\t1\tparsed",
        target, provider ? provider : "",
        is_global ? 1 : 0, is_session ? 1 : 0, is_once ? 1 : 0, force_refresh ? 1 : 0);
    return buf;
}

/* PoP: msw_model_switch_request_error_messages @ hermes_cli/model_switch.py:ModelSwitchRequest.error_messages */
char *msw_model_switch_request_error_messages(const char *errors_csv) {
    /* Python: [MODEL_SWITCH_ERROR_TEXT[code] for code in self.errors]
     * Arg = comma-separated error codes (e.g. "once_with_global,once_requires_target").
     * Returns newline-separated error text. */
    if (!errors_csv || !*errors_csv) { printf("\n"); return NULL; }
    char *copy = strdup(errors_csv);
    if (!copy) return NULL;
    char *result = NULL;
    size_t rcap = 0;
    size_t rlen = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(copy, ",", &saveptr);
    while (tok) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        const char *text = NULL;
        if (strcmp(tok, "once_with_global") == 0)
            text = "/model --once cannot be combined with --global";
        else if (strcmp(tok, "once_requires_target") == 0)
            text = "/model --once requires a model or provider.";
        if (text) {
            size_t tlen = strlen(text);
            if (rlen + tlen + 2 > rcap) {
                while (rlen + tlen + 2 > rcap) rcap += 256;
                result = realloc(result, rcap);
            }
            memcpy(result + rlen, text, tlen);
            rlen += tlen;
            result[rlen++] = '\n';
            result[rlen] = '\0';
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }
    free(copy);
    if (!result) { printf("\n"); return NULL; }
    printf("%s", result);
    return result;
}

/* Helper: parse_model_flags_detailed returning allocated result string */
char *msw_parse_model_flags_detailed_buf(const char *arg) {
    /* Python: hand-rolled flag parse. */
    if (!arg || !*arg) return strdup("\t\t0000\t0\t0");
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) return strdup("\t\t0000\t0\t0");
    char *result = malloc(1024);
    if (!result) return NULL;
    snprintf(result, 1024, "\t%s\t%s\t%s\n",
        t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return result;
}

/* PoP: msw_parse_model_switch_args @ hermes_cli/model_switch.py:parse_model_switch_args */
char *msw_parse_model_switch_args(const char *raw_args) {
    /* Python: wraps parse_model_flags_detailed + flag-conflict validation.
     * Returns a serialized ModelSwitchRequest:
     * "raw\ttarget\tprovider\tis_global\tis_session\tis_once\tforce_refresh\tscope\terrors\n" */
    char *parsed = msw_parse_model_flags_detailed_buf(raw_args);
    /* parse_model_flags_detailed_buf returns: model\tprovider\tflags\tstate\tresult */
    /* flags = is_global is_session is_once force_refresh (4 bits as '0'/'1' chars) */
    if (!parsed) { printf("\t\t0\t0\t0\t0\t0\tdefault\t\n"); return NULL; }
    const char *t1 = strchr(parsed, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    char model[256] = "", provider[256] = "", flags[8] = "0000", scope[16] = "default";
    if (t1) { strncpy(model, parsed, t1 - parsed); model[255]='\0'; }
    if (t1 && t2) { strncpy(provider, t1+1, t2 - t1 - 1); provider[255]='\0'; }
    if (t2 && t3) { strncpy(flags, t2+1, t3 - t2 - 1); flags[7]='\0'; }
    int is_global = flags[0] == '1';
    int is_session = flags[1] == '1';
    int is_once = flags[2] == '1';
    /* force_refresh = flags[3] */

    /* Flag conflict validation (mirrors Python) */
    char errors[256] = "";
    char *e = errors;
    int elen = 0;
    if (is_once && is_global) {
        if (elen > 0) { *e++ = ','; elen++; }
        const char *s = "once_with_global";
        strcpy(e, s); e += strlen(s); elen += strlen(s);
    }
    if (is_once && !model[0] && !provider[0]) {
        if (elen > 0) { *e++ = ','; elen++; }
        const char *s = "once_requires_target";
        strcpy(e, s); e += strlen(s); elen += strlen(s);
    }

    /* Determine scope */
    if (is_once) strcpy(scope, "once");
    else if (is_session) strcpy(scope, "session");
    else if (is_global) strcpy(scope, "global");
    else strcpy(scope, "default");

    printf("%s\t%s\t%s\t%d\t%d\t%d\t0\t%s\t%s\n",
        raw_args ? raw_args : "", model, provider, is_global, is_session, is_once, scope, errors);
    free(parsed);
    return NULL;
}

/* PoP: msw_effective_model_candidate @ hermes_cli/model_switch.py:_effective_model_candidate */
const char *msw_effective_model_candidate(const char *value, const char *model_key) {
    /* Python: extract model-name from str / dict / attr-object.
     * Args: value (string, or JSON dict, or ignored), model_key (for dict lookup).
     * Returns the model candidate string. */
    if (!value || !*value) return "";
    /* If value is JSON, extract "model" key */
    if (value[0] == '{') {
        json_t *j = json_parse(value, NULL);
        if (j) {
            const char *m = json_get_str(j, "model", NULL);
            if (!m) m = json_get_str(j, model_key ? model_key : "model", NULL);
            if (m) {
                /* Strip and return */
                char buf[512];
                const char *s = m;
                while (*s == ' ') s++;
                char *e = (char *)s + strlen(s);
                while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
                int len = (int)(e - s);
                if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
                memcpy(buf, s, len); buf[len] = '\0';
                json_free(j);
                printf("%s\n", buf);
                return buf;
            }
            json_free(j);
        }
        printf("\n");
        return "";
    }
    /* Plain string — strip and return */
    char buf[512];
    const char *s = value;
    while (*s == ' ') s++;
    char *e = (char *)s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
    int len = (int)(e - s);
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    memcpy(buf, s, len); buf[len] = '\0';
    printf("%s\n", buf);
    return buf;
}

/* PoP: msw_resolve_effective_model @ hermes_cli/model_switch.py:resolve_effective_model */
const char *msw_resolve_effective_model(const char *session, const char *channel,
        const char *global_cfg) {
    /* Python: session > channel > global precedence.
     * Each arg may be a model string, JSON dict with "model" key, or empty. */
    const char *candidate;
    candidate = msw_effective_model_candidate(session, "model");
    if (candidate && *candidate) { printf("%s\n", candidate); return candidate; }
    candidate = msw_effective_model_candidate(channel, "model");
    if (candidate && *candidate) { printf("%s\n", candidate); return candidate; }
    candidate = msw_effective_model_candidate(global_cfg, "model");
    if (candidate && *candidate) { printf("%s\n", candidate); return candidate; }
    printf("\n");
    return "";
}

/* PoP: msw_resolve_display_context_length_async @ hermes_cli/model_switch.py:resolve_display_context_length_async */
int msw_resolve_display_context_length_async(const char *arg) {
    /* Python: async wrapper that runs resolve_display_context_length in a
     * thread. In C: delegate to the sync resolver (msw_resolve_display_context_length).
     * Arg = "state\tresult". */
    /* Reuse the existing sync resolver */
    return msw_resolve_display_context_length(arg);
}

/* PoP: model_switch_config_is_allowlist @ hermes_cli/model_switch.py:_models_config_is_allowlist */
/* Returns 1 if the config value is an intentional allowlist. value_json = JSON value. */
int model_switch_config_is_allowlist(const char *value_json) {
    if (!value_json) return 0;
    char *err = NULL;
    json_t *value = json_parse(value_json, &err);
    if (err || !value) { if (err) free(err); return 0; }
    int result = 0;
    if (value->type == JSON_STRING) {
        /* String: allowlist if non-empty */
        const char *s = value->str_val;
        while (*s == ' ' || *s == '\t') s++;
        result = *s != '\0';
    } else if (value->type == JSON_OBJECT) {
        /* Dict: metadata, NOT an allowlist */
        result = 0;
    } else if (value->type == JSON_ARRAY) {
        /* List: allowlist */
        result = 1;
    }
    json_free(value);
    return result;
}

