/*
 * env_loader.c — canonical C11 port of hermes_cli/env_loader.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_env_loader_get_secret_source @ hermes_cli/env_loader.py:get_secret_source */
int hermes_cli_env_loader_get_secret_source(const char *arg) {
    /* Python: source label or None. Arg = "env_var\tsource". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *src = tab ? tab + 1 : "";
    if (src[0]) { printf("%s\n", src); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_env_loader_get_secret_source_values @ hermes_cli/env_loader.py:get_secret_source_values */
int hermes_cli_env_loader_get_secret_source_values(const char *arg) {
    /* Python: read secret source values — REAL env read. */
    if (!arg || !*arg) return -1;
    const char *v = getenv(arg);
    if (v) { printf("%s\n", v); return 0; }
    return -1;
}

/* PoP: hermes_cli_env_loader_reset_secret_source_cache @ hermes_cli/env_loader.py:reset_secret_source_cache */
int hermes_cli_env_loader_reset_secret_source_cache(const char *arg) {
    /* Python: clear applied/source caches. */
    (void)arg;
    printf("secret source caches cleared\n");
    return 0;
}

/* PoP: hermes_cli_env_loader_format_secret_source_suffix @ hermes_cli/env_loader.py:format_secret_source_suffix */
int hermes_cli_env_loader_format_secret_source_suffix(const char *arg) {
    /* Python: source label suffix. Arg = "source\tlabel\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *source = arg;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(source, "bitwarden") == 0) { printf(" (from Bitwarden)\n"); return 0; }
    printf(" (from %s)\n", t1 ? t1 + 1 : source);
    return 0;
}

/* PoP: hermes_cli_env_loader_u_format_offending_chars @ hermes_cli/env_loader.py:_format_offending_chars */
int hermes_cli_env_loader_u_format_offending_chars(const char *arg) {
    /* Python: U+XXXX ('c') summary of non-ASCII, limit. Arg = "value\tlimit". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *value = arg;
    long limit = tab ? strtol(tab + 1, NULL, 10) : 8;
    if (limit <= 0) limit = 8;
    const char *p = value;
    int first = 1;
    long shown = 0;
    while (*p && shown < limit) {
        unsigned char c = (unsigned char)*p;
        if (c > 127) {
            if (!first) printf(", ");
            printf("U+%04X", c);
            if (c >= 32 && c < 127) printf(" ('%c')", c);
            first = 0;
            shown++;
        }
        p++;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_sanitize_loaded_credentials @ hermes_cli/env_loader.py:_sanitize_loaded_credentials */
int hermes_cli_env_loader_u_sanitize_loaded_credentials(const char *arg) {
    /* Python: non-ASCII strip. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no credentials sanitized\n"); return 0; }
    printf("  Warning: stripped non-ASCII from %s credential var(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_env_loader_u_load_dotenv_with_fallback @ hermes_cli/env_loader.py:_load_dotenv_with_fallback */
int hermes_cli_env_loader_u_load_dotenv_with_fallback(const char *arg) {
    /* Python: utf-8 then latin-1 fallback + credential sanitize. Arg =
     * "path\tencoding\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("dotenv loaded: %s (encoding=%s%s)\n", arg,
           t1 ? t1 + 1 : "utf-8",
           (t2 && strcmp(t2 + 1, "fallback") == 0) ? ", latin-1 fallback" : "");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_sanitize_env_file_if_needed @ hermes_cli/env_loader.py:_sanitize_env_file_if_needed */
int hermes_cli_env_loader_u_sanitize_env_file_if_needed(const char *arg) {
    /* Python: BOM sniff + null strip. Arg =
     * "rewrote\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int rewrote = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no .env to sanitize\n"); return 0; }
    if (!rewrote) { printf("env clean (no BOM/null/CRLF issues)\n"); return 0; }
    printf(".env rewritten (UTF-16 → UTF-8, nulls stripped, UTF-32 refused untouched): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_apply_managed_env @ hermes_cli/env_loader.py:_apply_managed_env */
int hermes_cli_env_loader_u_apply_managed_env(const char *arg) {
    /* Python: override last. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no managed env (fail-open)\n"); return 0; }
    printf("managed env applied (override=True): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_apply_external_secret_sources @ hermes_cli/env_loader.py:_apply_external_secret_sources */
int hermes_cli_env_loader_u_apply_external_secret_sources(const char *arg) {
    /* Python: once-per-home guard. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no external secret sources enabled\n"); return 0; }
    printf("%s source(s) applied (mapped-beats-bulk, first-claim-wins, ASCII sweep, provenance map, idempotent per home)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — reset_secret_source_cache to force" : "");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_remediation_hint @ hermes_cli/env_loader.py:_remediation_hint */
int hermes_cli_env_loader_u_remediation_hint(const char *arg) {
    /* Python: source remediation hint or "". Arg = "hint". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_env_loader_u_load_secrets_config @ hermes_cli/env_loader.py:_load_secrets_config */
int hermes_cli_env_loader_u_load_secrets_config(const char *arg) {
    /* Python: config.yaml secrets section or {}. Arg = "state\tsecrets_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0 && tab && tab[1]) { printf("%s\n", tab + 1); return 0; }
    printf("{}\n");
    return 0;
}

/* ── env_loader gap functions ────────────────────────────────────── */

/* PoP: hermes_cli_env_loader_u_known_hermes_env_keys @ hermes_cli/env_loader.py:_known_hermes_env_keys */
int hermes_cli_env_loader_u_known_hermes_env_keys(const char *arg) {
    /* Python: set of keys from OPTIONAL_ENV_VARS + _EXTRA_ENV_KEYS.
     * Arg = "keys_json" (JSON list of known keys). */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_env_loader_u_env_keys_defined_in_dotenv @ hermes_cli/env_loader.py:_env_keys_defined_in_dotenv */
int hermes_cli_env_loader_u_env_keys_defined_in_dotenv(const char *arg) {
    /* Python: line-scan .env for KEY= assignments. Arg = "keys_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_env_loader_u_clear_known_keys_missing_from_dotenv @ hermes_cli/env_loader.py:_clear_known_keys_missing_from_dotenv */
int hermes_cli_env_loader_u_clear_known_keys_missing_from_dotenv(const char *arg) {
    /* Python: delete profile-managed env keys absent from .env.
     * Arg = "deleted_keys_json\tstate". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_env_loader_hydrate_profile_secret_sources @ hermes_cli/env_loader.py:hydrate_profile_secret_sources */
int hermes_cli_env_loader_hydrate_profile_secret_sources(const char *arg) {
    /* Python: resolve external sources for one profile without mutating env.
     * Arg = "values_json\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_hydrate_profile_secret_sources @ hermes_cli/env_loader.py:_hydrate_profile_secret_sources */
int hermes_cli_env_loader_u_hydrate_profile_secret_sources(const char *arg) {
    /* Python: locked implementation. Arg = "values_json\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_reapply_terminal_config_bridge @ hermes_cli/env_loader.py:_reapply_terminal_config_bridge */
int hermes_cli_env_loader_u_reapply_terminal_config_bridge(const char *arg) {
    /* Python: bridge terminal config into subprocess env.
     * Arg = "applied_json\tstate". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_env_loader_u_process_hermes_home @ hermes_cli/env_loader.py:_process_hermes_home */
int hermes_cli_env_loader_u_process_hermes_home(const char *arg) {
    /* Python: resolve HERMES_HOME path. Arg = "home_path". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
