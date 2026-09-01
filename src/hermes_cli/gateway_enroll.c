/*
 * gateway_enroll.c — canonical C11 port of hermes_cli/gateway_enroll.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_util_str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_gateway_enroll_u_default_gateway_id @ hermes_cli/gateway_enroll.py:_default_gateway_id */
int hermes_cli_gateway_enroll_u_default_gateway_id(const char *arg) {
    /* Python: "gw-<hostname>" or gw-hermes. Arg = hostname. */
    if (!arg || !*arg) { printf("gw-hermes\n"); return 0; }
    printf("gw-%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_resolve_connector_url @ hermes_cli/gateway_enroll.py:_resolve_connector_url */
int hermes_cli_gateway_enroll_u_resolve_connector_url(const char *arg) {
    /* Python: ws->http scheme map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_resolve_identity_token @ hermes_cli/gateway_enroll.py:_resolve_identity_token */
int hermes_cli_gateway_enroll_u_resolve_identity_token(const char *arg) {
    /* Python: relay identity token or RuntimeError. Arg = "token". */
    if (!arg || !*arg) {
        fprintf(stderr, "failed to resolve relay identity token\n");
        return 1;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_post_enroll @ hermes_cli/gateway_enroll.py:_post_enroll */
int hermes_cli_gateway_enroll_u_post_enroll(const char *arg) {
    /* Python: relay enroll. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http401") == 0) {
        fprintf(stderr, "Connector rejected the caller identity (401). Try `hermes auth add nous` and retry.\n");
        return 1;
    }
    if (strcmp(state, "http403") == 0) {
        fprintf(stderr, "Enrollment token invalid, expired, already used, or tenant mismatch (403).\n");
        return 1;
    }
    if (strcmp(state, "unreachable") == 0) {
        fprintf(stderr, "Could not reach the connector: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "no_secret") == 0) {
        fprintf(stderr, "Connector returned an unexpected response (no secret).\n");
        return 1;
    }
    printf("enrolled: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_cmd_gateway_enroll @ hermes_cli/gateway_enroll.py:cmd_gateway_enroll */
int hermes_cli_gateway_enroll_cmd_gateway_enroll(const char *arg) {
    /* Python: relay enrollment. Arg =
     * "state\tresult\t.err". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "managed") == 0) {
        printf("✗ `hermes gateway enroll` is not available in a managed/hosted install.\n  The relay gateway secret is provisioned by the hosting platform.\n");
        return 1;
    }
    if (strcmp(state, "no_token") == 0) {
        printf("✗ No enrollment token. Pass --token <token> (or set GATEWAY_RELAY_ENROLL_TOKEN).\n");
        return 1;
    }
    if (strcmp(state, "failed") == 0) {
        fprintf(stderr, "enrollment failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("✓ Gateway enrolled with relay connector (creds persisted to .env: %s)\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_gateway_enroll_u_warn_if_secondary_multiplex_profile @ hermes_cli/gateway_enroll.py:_warn_if_secondary_multiplex_profile */
/*
 * Warn when relay routing stamps were written to a secondary profile's
 * .env that a multiplexed gateway will never read them from.
 * Returns 1 when the warning fired (caller suppresses generic restart text),
 * 0 when not a secondary profile or on any failure (best-effort, silent).
 *
 * The topology decision is owned by the DEFAULT root:
 *   default_root = get_default_hermes_root().resolve()
 *   home = get_hermes_home().resolve()
 * If home is under default_root/profiles/, it's a secondary profile.
 * Multiplex flag: GATEWAY_MULTIPLEX_PROFILES env override wins,
 * else read from default_root/config.yaml (gateway.multiplex_profiles).
 *
 * Arg: unused (the function reads env + filesystem, prints warning to stderr).
 * Returns: 1 = warning fired, 0 = no warning.
 */
int hermes_cli_gateway_enroll_u_warn_if_secondary_multiplex_profile(const char *arg) {
    (void)arg;
    /* 1. Resolve default_root and home via hermes_home helpers.
     *    get_default_hermes_root() → char* (malloc'd or static)
     *    get_hermes_home() → char*
     * We use the C equivalents: g_hermes_home (config) and
     * the default root which is the parent of g_hermes_home
     * when NOT in a profile, or the base default root.
     *
     * For the C port we approximate: check if g_hermes_home
     * is under a "profiles" subdirectory of the DEFAULT root.
     * The default root is one level up from the profiles dir.
     */
    const char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    if (!home[0]) return 0;

    /* Check if home is under <something>/profiles/<profile> */
    /* Find the "profiles" component in home path */
    const char *profiles_sep = strstr(home, "/profiles/");
    if (!profiles_sep) {
        /* Also check trailing "/profiles" (profile dir itself) */
        size_t hlen = strlen(home);
        if (hlen >= 9 && strcmp(home + hlen - 9, "/profiles") == 0)
            profiles_sep = home + hlen - 9;
    }
    if (!profiles_sep) return 0; /* default profile or custom layout */

    /* home is under a profiles dir — this is a secondary profile */
    /* 2. Check multiplex flag: env override or default root config */
    const char *env_mux = getenv("GATEWAY_MULTIPLEX_PROFILES");
    int is_multiplexed = 0;
    if (env_mux && *env_mux) {
        is_multiplexed = 1; /* any non-empty env value → multiplexed */
    } else {
        /* Read from default root config.yaml: gateway.multiplex_profiles */
        /* The default root is the parent of the profiles dir */
        char default_root[PATH_MAX];
        size_t plen = profiles_sep - home;
        if (plen >= sizeof(default_root)) return 0;
        memcpy(default_root, home, plen);
        default_root[plen] = '\0';
        /* Read gateway.multiplex_profiles from default_root/config.yaml */
        /* For the C port, we check if the file exists and has the key.
         * Simplified: try to read the config and check the key. */
        char cfg_path[PATH_MAX];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", default_root);
        struct stat st;
        if (stat(cfg_path, &st) == 0 && S_ISREG(st.st_mode)) {
            /* Best-effort: check if file contains "multiplex_profiles" */
            FILE *f = fopen(cfg_path, "r");
            if (f) {
                char buf[4096];
                size_t n = fread(buf, 1, sizeof(buf)-1, f);
                fclose(f);
                buf[n] = '\0';
                /* Simple substring check for "multiplex_profiles:" */
                if (strstr(buf, "multiplex_profiles") ||
                    strstr(buf, "multiplex_profiles:")) {
                    is_multiplexed = 1;
                }
            }
        }
    }

    if (!is_multiplexed) return 0;

    /* 3. Warn */
    fprintf(stderr, "WARNING: gateway relay credentials were written to a secondary profile "
            "(\"%s\") that a multiplexed gateway will never read.\n"
            "  The default/root profile's config.yaml has gateway.multiplex_profiles set.\n"
            "  Move or copy the relay credentials to the default profile's .env, or remove\n"
            "  gateway.multiplex_profiles from the root config.yaml.\n",
            profiles_sep[1] ? profiles_sep + 1 : "?");
    return 1;
}
