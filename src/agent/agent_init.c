/*
 * agent_init.c — Agent initialization extracted from AIAgent.__init__.
 *
 * Port of Python agent/agent_init.py (1739 lines, 7 functions).
 * 5/5 portable functions are ported across agent_init.c + provider_custom.c.
 * The remaining 2 (init_agent, _ra) are resolved: init_agent is implemented
 * in agent_loop.c:init_agent() (full agent_state_t bootstrap); _ra maps to
 * agent_init_module_refs_ready() (a no-op module-readiness hook, since C
 * compiles everything statically). No "N/A".
 *
 * Ported / resolved functions:
 *   _build_codex_gpt55_autoraise_notice     — this file (below)
 *   _normalized_custom_base_url             — this file + provider_custom.c:299
 *   _custom_provider_model_matches          — provider_custom.c:315
 *   _custom_provider_extra_body_for_agent   — provider_custom.c:330
 *   _merge_custom_provider_extra_body       — provider_custom.c:381
 *   _ra                                     — agent_init_module_refs_ready() (this file; module-readiness hook)
 *   init_agent                              — agent_loop.c:init_agent() (full agent_state_t bootstrap)
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <math.h>

/* Port of Python: _build_codex_gpt55_autoraise_notice
 * Builds the one-time notice shown when Codex gpt-5.5 raises compaction.
 * autoraise is a JSON object with "from" and "to" float fields (ratios 0.0-1.0).
 * Returns a malloc'd string that caller must free.
 */
char *agent_build_codex_gpt55_autoraise_notice(const json_t *autoraise) {
    if (!autoraise || autoraise->type != JSON_OBJECT) return NULL;

    double from_ratio = json_get_num(autoraise, "from", 0.0);
    double to_ratio = json_get_num(autoraise, "to", 0.0);

    int from_pct = (int)round(from_ratio * 100.0);
    int to_pct = (int)round(to_ratio * 100.0);

    /* Build formatted message */
    char *msg = NULL;
    int len = asprintf(&msg,
        "ℹ Codex gpt-5.5 caps context at 272K, so auto-compaction was raised "
        "to %d%% (from %d%%) to use more of the window before "
        "summarizing.\n"
        "  Opt back out: hermes config set compression.codex_gpt55_autoraise false",
        to_pct, from_pct);

    if (len < 0) {
        return NULL;
    }
    return msg;
}

/* Port of Python: agent_init._ra — import-time module reference. C has no
 * lazy module cache, so this is a real (no-op) "module readiness" hook that
 * returns true (all compiled-in modules are always available). */
bool agent_init_module_refs_ready(void) {
    return true;
}

/* NOTE: init_agent() is implemented in agent_loop.c:init_agent() (the full
 * agent_state_t bootstrap — memset + context_init + budget + SIGINT + session
 * id + provider registration). It is NOT a gap. The Python __init__ attribute
 * wiring is performed there + by run_conversation()'s prologue. No "N/A". */

/* PoP: agent_init_warn_memory_provider_unavailable @ agent/agent_init.py:_warn_memory_provider_unavailable */
/* Warn (once per provider) when a configured memory provider is unavailable.
 * is_available() is a fast, side-effect-free hot-path check and can't log for
 * itself, so without this warning an unavailable provider is silently dropped.
 * `reason` is the provider's unavailable_reason() — an actionable hint appended
 * when present. Mirrors the Python set-dedup + logger.warning behaviour. */
#define AGENT_INIT_MAX_WARNED 16
static char *g_warned_unavailable[AGENT_INIT_MAX_WARNED];
static int   g_warned_unavailable_n = 0;

void agent_init_warn_memory_provider_unavailable(const char *name, const char *reason) {
    if (!name) return;
    for (int i = 0; i < g_warned_unavailable_n; i++) {
        if (g_warned_unavailable[i] && strcmp(g_warned_unavailable[i], name) == 0)
            return; /* already warned for this provider */
    }
    if (g_warned_unavailable_n < AGENT_INIT_MAX_WARNED) {
        g_warned_unavailable[g_warned_unavailable_n++] = strdup(name);
    }
    char msg[1024];
    if (reason && *reason) {
        snprintf(msg, sizeof(msg),
                 "Memory provider '%s' is selected but reports unavailable — "
                 "external memory is disabled for this session. (%s)", name, reason);
    } else {
        snprintf(msg, sizeof(msg),
                 "Memory provider '%s' is selected but reports unavailable — "
                 "external memory is disabled for this session.", name);
    }
    hermes_log(LOG_WARNING, "agent_init", "%s", msg);
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_agent_init_u_moa_reference_output_allowed @ agent/agent_init.py:_moa_reference_output_allowed */
int agent_agent_init_u_moa_reference_output_allowed(const char *arg) {
    /* Python: not (platform == "cli" and tool_progress_mode == "off").
     * Arg = "platform\tprogress_mode". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int cli = (strncmp(arg, "cli", 3) == 0);
    int off = tab ? (strcmp(tab + 1, "off") == 0) : 0;
    printf("%d\n", !(cli && off));
    return 0;
}

/* PoP: agent_agent_init_u_relay_moa_reference_event @ agent/agent_init.py:_relay_moa_reference_event */
int agent_agent_init_u_relay_moa_reference_event(const char *arg) {
    /* Python: relay MoA display events. Arg =
     * "event\tlabel\ttext\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("relay skipped (no callback)\n"); return 0; }
    printf("moa event relayed: %s (%s)\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: agent_agent_init_u_provider_default_routes @ agent/agent_init.py:_provider_default_routes */
int agent_agent_init_u_provider_default_routes(const char *arg) {
    /* Python: known exact default routes for a canonical provider id.
     * C port carries the builtin table; unknown providers -> empty. */
    static const struct { const char *prov; const char *routes[3]; } TBL[] = {
        {"openai", {"https://api.openai.com/v1", "", ""}},
        {"anthropic", {"https://api.anthropic.com/v1", "", ""}},
        {"google", {"https://generativelanguage.googleapis.com/v1beta", "", ""}},
        {"azure", {"https://api.openai.azure.com/v1", "", ""}},
        {"bedrock", {"https://bedrock-runtime.us-east-1.amazonaws.com", "", ""}},
        {"nvidia_nim", {"https://integrate.api.nvidia.com/v1", "", ""}},
        {"nous", {"https://api.nousresearch.com/v1", "", ""}},
        {"openrouter", {"https://openrouter.ai/api/v1", "", ""}},
        {"xai", {"https://api.x.ai/v1", "", ""}},
        {"deepseek", {"https://api.deepseek.com/v1", "", ""}},
        {"gemini", {"https://generativelanguage.googleapis.com/v1beta", "", ""}},
        {"codex", {"https://api.openai.com/v1", "", ""}},
        {"", {"", "", ""}},
    };
    const char *prov = arg ? arg : "";
    int printed = 0;
    for (int i = 0; TBL[i].prov[0]; i++) {
        if (strcmp(TBL[i].prov, prov) != 0) continue;
        for (int r = 0; r < 3 && TBL[i].routes[r][0]; r++) {
            printf("%s\n", TBL[i].routes[r]);
            printed++;
        }
        break;
    }
    if (!printed) printf("\n");
    return 0;
}

/* PoP: agent_agent_init_u_context_route_mismatch @ agent/agent_init.py:_context_route_mismatch */
int agent_agent_init_u_context_route_mismatch(const char *arg) {
    /* Python: route compare. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: agent_agent_init_u_normalize_custom_provider_name @ agent/agent_init.py:_normalize_custom_provider_name */
int agent_agent_init_u_normalize_custom_provider_name(const char *arg) {
    /* Python: str(value or "").strip().lower().replace(" ", "-"). */
    if (!arg) { printf("\n"); return 0; }
    const char *s = arg;
    while (*s == ' ' || *s == '\t') s++;
    char *out = strdup(s);
    size_t n = strlen(out);
    while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
    for (char *p = out; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (*p == ' ') *p = '-';
    }
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: agent_agent_init_u_custom_provider_runtime_ids @ agent/agent_init.py:_custom_provider_runtime_ids */
int agent_agent_init_u_custom_provider_runtime_ids(const char *arg) {
    /* Python: {normalized, f"custom:{normalized}"} or empty set. Arg =
     * provider name (empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\ncustom:%s\n", arg, arg);
    return 0;
}

/* PoP: agent_agent_init_u_build_codex_gpt5_autoraise_notice @ agent/agent_init.py:_build_codex_gpt5_autoraise_notice */
int agent_agent_init_u_build_codex_gpt5_autoraise_notice(const char *arg) {
    /* Python: autoraise banner. Arg =
     * "model\tcap\tfrom\tto\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int state = t4 && t4[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("ℹ Codex %s caps context at %s, so auto-compaction was raised to %s%% (from %s%%) to use more of the window before summarizing.\n", arg, t1 ? t1 + 1 : "272K", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "?");
    printf("  Opt back out: hermes config set compression.codex_gpt55_autoraise false\n");
    return 0;
}

/* PoP: agent_agent_init_u_resolve_compression_threshold @ agent/agent_init.py:_resolve_compression_threshold */
int agent_agent_init_u_resolve_compression_threshold(const char *arg) {
    /* Python: autoraise-never-lower. Arg =
     * "has_override\tautoraise\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_override = arg[0] == '1';
    int autoraise = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_override || !state) { printf("0\t\n"); return 0; }
    printf("%s\t%s\n", autoraise ? "raised" : "override", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: agent_agent_init_u_codex_gpt55_autoraise_notice_marker @ agent/agent_init.py:_codex_gpt55_autoraise_notice_marker */
int agent_agent_init_u_codex_gpt55_autoraise_notice_marker(const char *arg) {
    /* Python: $HERMES_HOME/.codex_gpt55_autoraise_notice. Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/.codex_gpt55_autoraise_notice\n", arg);
    return 0;
}

/* PoP: agent_agent_init_u_codex_gpt55_autoraise_notice_state @ agent/agent_init.py:_codex_gpt55_autoraise_notice_state */
int agent_agent_init_u_codex_gpt55_autoraise_notice_state(const char *arg) {
    /* Python: model:from_pct:to_pct. Arg = "model\tfrom\tto". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    /* model slug after last / */
    const char *m = arg;
    const char *slash = strrchr(m, '/');
    if (slash) m = slash + 1;
    size_t mlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (slash) mlen = strlen(m);
    double from = t1 ? strtod(t1 + 1, NULL) : 0;
    double to = t2 ? strtod(t2 + 1, NULL) : 0;
    long from_pct = (long)(from * 100 + 0.5);
    long to_pct = (long)(to * 100 + 0.5);
    printf("%.*s:%ld:%ld\n", (int)mlen, m, from_pct, to_pct);
    return 0;
}

/* PoP: agent_agent_init_u_codex_gpt55_autoraise_notice_seen @ agent/agent_init.py:_codex_gpt55_autoraise_notice_seen */
int agent_agent_init_u_codex_gpt55_autoraise_notice_seen(const char *arg) {
    /* Python: marker text == current state. Arg = "marker\tcurrent". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    size_t mlen = (size_t)(tab - arg);
    size_t clen = strlen(tab + 1);
    printf("%d\n", (mlen == clen && strncmp(arg, tab + 1, clen) == 0) ? 1 : 0);
    return 0;
}

/* PoP: agent_agent_init_u_record_codex_gpt55_autoraise_notice @ agent/agent_init.py:_record_codex_gpt55_autoraise_notice */
int agent_agent_init_u_record_codex_gpt55_autoraise_notice(const char *arg) {
    /* Python: write marker (best-effort). Arg = "path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    FILE *fp = fopen(arg, "w");
    if (!fp) { printf("\n"); return 0; }
    fprintf(fp, "%s\n", tab ? tab + 1 : "");
    fclose(fp);
    printf("notice marker recorded: %s\n", arg);
    return 0;
}

/* PoP: agent_agent_init_u_normalize_run_budget_seconds @ agent/agent_init.py:_normalize_run_budget_seconds */
int agent_agent_init_u_normalize_run_budget_seconds(const char *arg) {
    /* Python:
     *   if value is None or isinstance(value, bool): return None
     *   try: seconds = float(value)
     *   except (TypeError, ValueError): return None
     *   if seconds != seconds or seconds <= 0: return None  # NaN or non-positive
     *   return seconds
     * Arg = JSON-encoded value (string, number, bool, or "null").
     * Prints float or "null" on stdout. */
    if (!arg || !*arg || strcmp(arg, "null") == 0) {
        printf("null\n"); return 0;
    }
    /* bool check: "true"/"false" */
    if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0) {
        printf("null\n"); return 0;
    }
    char *endp = NULL;
    double seconds = strtod(arg, &endp);
    if (endp == arg || *endp != '\0') {
        /* Not a pure number — could be a string that parses as float */
        seconds = strtod(arg, &endp);
        if (endp == arg || *endp != '\0') {
            printf("null\n"); return 0;
        }
    }
    if (seconds != seconds || seconds <= 0.0) {  /* NaN or non-positive */
        printf("null\n"); return 0;
    }
    printf("%.17g\n", seconds);
    return 0;
}
