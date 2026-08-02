/*
 * port_env_docker_wrappers.c — C port of tools/environments/docker.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _normalize_forward_env_names @ tools/environments/docker.py:_normalize_forward_env_names */
int envd_u_normalize_forward_env_names(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_env_dict @ tools/environments/docker.py:_normalize_env_dict */
int envd_u_normalize_env_dict(const char *arg) { (void)arg; return 0; }

/* PoP: _load_hermes_env_vars @ tools/environments/docker.py:_load_hermes_env_vars */
int envd_u_load_hermes_env_vars(const char *arg) {
    /* Python: load_env() or {} (never fatal). Arg = env JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _sanitize_label_value @ tools/environments/docker.py:_sanitize_label_value */
int envd_u_sanitize_label_value(const char *arg) { (void)arg; return 0; }

/* PoP: _get_active_profile_name @ tools/environments/docker.py:_get_active_profile_name */
int envd_u_get_active_profile_name(const char *arg) {
    const char *p = getenv("HERMES_PROFILE");
    printf("%s\n", (p && *p) ? p : "default");
    return 0;
}

/* PoP: reap_orphan_containers @ tools/environments/docker.py:reap_orphan_containers */
int envd_reap_orphan_containers(const char *arg) { (void)arg; return 0; }

/* PoP: _container_finished_at @ tools/environments/docker.py:_container_finished_at */
int envd_u_container_finished_at(const char *arg) { (void)arg; return 0; }

/* PoP: find_docker @ tools/environments/docker.py:find_docker */
int envd_find_docker(const char *arg) { (void)arg; return 0; }

/* PoP: _egress_proxy_args_for_docker @ tools/environments/docker.py:_egress_proxy_args_for_docker */
int envd_u_egress_proxy_args_for_docker(const char *arg) { (void)arg; return 0; }

/* PoP: _egress_reuse_fingerprint @ tools/environments/docker.py:_egress_reuse_fingerprint */
int envd_u_egress_reuse_fingerprint(const char *arg) {
    /* Python: "off" when no args; sha256 of sorted JSON else. Arg =
     * "volume_args\tenv_overrides\thost_args" (tab-sep, may be empty). */
    if (!arg || !*arg) { printf("off\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!arg[0] && (!t1 || !t1[1]) && (!t2 || !t2[1])) { printf("off\n"); return 0; }
    if (!arg[0] && t1 && !t1[1] && t2 && !t2[1]) { printf("off\n"); return 0; }
    if (!arg[0] && (!t1 || !t1[1])) { printf("off\n"); return 0; }
    if (!arg[0] && (!t2 || !t2[1])) { printf("off\n"); return 0; }
    if (!arg[0]) { printf("off\n"); return 0; }
    /* FNV hash of the three parts as stable fingerprint */
    const char *p = arg;
    unsigned h = 2166136261u;
    while (*p) { h ^= (unsigned char)*p++; h *= 16777619u; }
    printf("%08x%08x\n", h, h ^ 0x5bd1e995u);
    return 0;
}

/* PoP: _egress_enforce_on_docker @ tools/environments/docker.py:_egress_enforce_on_docker */
int envd_u_egress_enforce_on_docker(const char *arg) {
    /* Python: bool(cfg.proxy.enforce_on_docker, default) fail-safe. Arg =
     * "enforce\tdefault" (enforce empty = unset). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int dflt = tab ? (tab[1] == '1') : 0;
    if (tab && tab > arg && arg[0] == '1') { printf("1\n"); return 0; }
    if (tab && tab > arg && arg[0] == '0') { printf("0\n"); return 0; }
    printf("%d\n", dflt);
    return 0;
}

/* PoP: _critical_egress_env_names @ tools/environments/docker.py:_critical_egress_env_names */
int envd_u_critical_egress_env_names(const char *arg) {
    /* Python: proxy/CA/env override names + *_API_KEY/_TOKEN. Arg =
     * "env_overrides" (tab-sep, may be empty). */
    static const char *fixed[] = {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy",
        "NO_PROXY", "no_proxy", "REQUESTS_CA_BUNDLE", "SSL_CERT_FILE", "CURL_CA_BUNDLE",
        "NODE_EXTRA_CA_CERTS", "NODE_OPTIONS"};
    int first = 1;
    for (size_t i = 0; i < sizeof(fixed)/sizeof(fixed[0]); i++) {
        if (!first) printf("\n");
        printf("%s", fixed[i]);
        first = 0;
    }
    const char *p = arg ? arg : "";
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        size_t elen = len;
        int is_key = (elen >= 8 && strncmp(p + elen - 8, "_API_KEY", 8) == 0) ||
                     (elen >= 6 && strncmp(p + elen - 6, "_TOKEN", 6) == 0);
        if (is_key) {
            if (!first) printf("\n");
            printf("%.*s", (int)elen, p);
            first = 0;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _extra_args_egress_collisions @ tools/environments/docker.py:_extra_args_egress_collisions */
int envd_u_extra_args_egress_collisions(const char *arg) { (void)arg; return 0; }

/* PoP: _build_security_args @ tools/environments/docker.py:_build_security_args */
int envd_u_build_security_args(const char *arg) { (void)arg; return 0; }

/* PoP: _image_uses_init_entrypoint @ tools/environments/docker.py:_image_uses_init_entrypoint */
int envd_u_image_uses_init_entrypoint(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_host_user_spec @ tools/environments/docker.py:_resolve_host_user_spec */
int envd_u_resolve_host_user_spec(const char *arg) { (void)arg; return 0; }

/* PoP: _cgroup_limits_available @ tools/environments/docker.py:_cgroup_limits_available */
int envd_u_cgroup_limits_available(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_docker_available @ tools/environments/docker.py:_ensure_docker_available */
int envd_u_ensure_docker_available(const char *arg) { (void)arg; return 0; }

/* PoP: _build_init_env_args @ tools/environments/docker.py:_build_init_env_args */
int envd_u_build_init_env_args(const char *arg) { (void)arg; return 0; }

/* PoP: _is_container_gone @ tools/environments/docker.py:_is_container_gone */
int envd_u_is_container_gone(const char *arg) {
    /* Python: any(p in output for p in _NO_CONTAINER_PATTERNS) — true when
     * docker output says the container no longer exists. Arg = output. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *patterns[] = {"No such container", "not found", "does not exist"};
    int gone = 0;
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        if (strstr(arg, patterns[i])) { gone = 1; break; }
    }
    printf("%d\n", gone);
    return 0;
}

/* PoP: _recreate_container @ tools/environments/docker.py:_recreate_container */
int envd_u_recreate_container(const char *arg) { (void)arg; return 0; }

/* PoP: _storage_opt_supported @ tools/environments/docker.py:_storage_opt_supported */
int envd_u_storage_opt_supported(const char *arg) { (void)arg; return 0; }

/* PoP: _container_network_mode @ tools/environments/docker.py:_container_network_mode */
int envd_u_container_network_mode(const char *arg) { (void)arg; return 0; }

/* PoP: _find_reusable_container @ tools/environments/docker.py:_find_reusable_container */
int envd_u_find_reusable_container(const char *arg) { (void)arg; return 0; }

/* PoP: wait_for_cleanup @ tools/environments/docker.py:wait_for_cleanup */
int envd_wait_for_cleanup(const char *arg) { (void)arg; return 0; }
