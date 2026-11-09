/*
 * port_container_boot.c
 *
 * Pure helpers ported from hermes_cli/container_boot.py - s6 profile-gateway
 * reconciliation at container start.
 *
 * The module's real work is filesystem and process I/O (reading the process
 * cmdline table, writing service slots, appending a reconcile log). What is
 * portable is the argv interpretation:
 *
 *   cb_strip_argv_prefix      <- container_boot.py:_strip_container_argv_prefix
 *   cb_is_legacy_gateway_run  <- container_boot.py:_is_legacy_gateway_run_request
 *   cb_is_dashboard_container <- container_boot.py:_is_dashboard_container
 *   cb_boot_notice            <- container_boot.py:boot_notice
 *
 * An earlier revision of this file was a STUB: it took a single string and
 * stripped "docker run " prefixes, and detected a dashboard by substring. The
 * Python drops everything through the main-wrapper.sh token and compares
 * args[0] exactly. It was registered in objects.mk with no oracle, so the
 * divergence shipped - this file replaces it.
 */

#include "port_container_boot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _strip_container_argv_prefix  (container_boot.py:_strip_container_argv_prefix)
 *
 *      args = list(argv)
 *      wrapper_idx = next((i for i, a in enumerate(args)
 *                          if a.endswith("main-wrapper.sh")), None)
 *      if wrapper_idx is not None:
 *          args = args[wrapper_idx + 1:]
 *      elif args and Path(args[0]).name == "init":
 *          args = args[1:]
 *      if args and args[0].endswith("entrypoint-dispatch.sh"):
 *          args = args[1:]
 *      return args
 *
 *  Everything through the main-wrapper.sh token is dropped - the stable
 *  boundary the image owns - rather than peeling tokens positionally (which
 *  broke on the s6 v2->v3 bump). Returns a NEW NULL-terminated array of
 *  borrowed pointers; the caller frees the ARRAY only. *out_count is set.
 * =========================================================================== */

/* PoP: _strip_container_argv_prefix @ hermes_cli/container_boot.py:_strip_container_argv_prefix */
const char **cb_strip_argv_prefix(const char *const *argv, int *out_count)
{
    int n = 0;
    if (argv) while (argv[n]) n++;

    const char **out = (const char **)calloc((size_t)n + 1, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int start = 0;
    for (int i = 0; i < n; i++) {
        size_t len = strlen(argv[i]);
        const char *sfx = "main-wrapper.sh";
        size_t sl = strlen(sfx);
        if (len >= sl && strcmp(argv[i] + len - sl, sfx) == 0) { start = i + 1; break; }
    }
    if (start == 0 && n > 0) {
        /* Path(args[0]).name == "init" - basename comparison */
        const char *a0 = argv[0];
        const char *base = a0 + strlen(a0);
        while (base > a0 && base[-1] != '/' && base[-1] != '\\') base--;
        if (strcmp(base, "init") == 0) start = 1;
    }
    if (start < n) {
        size_t len = strlen(argv[start]);
        const char *sfx = "entrypoint-dispatch.sh";
        size_t sl = strlen(sfx);
        if (len >= sl && strcmp(argv[start] + len - sl, sfx) == 0) start++;
    }

    int k = 0;
    for (int i = start; i < n; i++) out[k++] = argv[i];
    if (out_count) *out_count = k;
    return out;
}

/* ===========================================================================
 *  _is_legacy_gateway_run_request  (container_boot.py:_is_legacy_gateway_run_request)
 *
 *      args = _strip_container_argv_prefix(argv)
 *      if "--no-supervise" in args: return False
 *      return len(args) >= 2 and args[0] == "gateway" and args[1] == "run"
 * =========================================================================== */

/* PoP: _is_legacy_gateway_run_request @ hermes_cli/container_boot.py:_is_legacy_gateway_run_request */
int cb_is_legacy_gateway_run(const char *const *argv)
{
    int n = 0;
    const char **args = cb_strip_argv_prefix(argv, &n);
    if (!args) return 0;

    int has_no_supervise = 0;
    for (int i = 0; i < n; i++)
        if (strcmp(args[i], "--no-supervise") == 0) { has_no_supervise = 1; break; }
    int result = !has_no_supervise && n >= 2 &&
                 strcmp(args[0], "gateway") == 0 && strcmp(args[1], "run") == 0;
    free(args);
    return result;
}

/* ===========================================================================
 *  _is_dashboard_container  (container_boot.py:_is_dashboard_container)
 *
 *      args = _strip_container_argv_prefix(argv)
 *      return bool(args) and args[0] == "dashboard"
 *
 *  A dashboard-only container never supervises gateways.
 * =========================================================================== */

/* PoP: _is_dashboard_container @ hermes_cli/container_boot.py:_is_dashboard_container */
int cb_is_dashboard_container(const char *const *argv)
{
    int n = 0;
    const char **args = cb_strip_argv_prefix(argv, &n);
    if (!args) return 0;
    int result = (n > 0 && strcmp(args[0], "dashboard") == 0);
    free(args);
    return result;
}

/* ===========================================================================
 *  boot_notice  (container_boot.py:boot_notice)
 *
 *      "reconcile: profile gateway(s " + ", ".join(sorted(folded)) +
 *      " asked to autostart; one gateway per container serves every "
 *      "non-standalone profile, so the ROOT slot (gateway-default) was started "
 *      "instead and multiplexes them."
 *
 *  `folded` is sorted (ASCII, as Python's sorted() on str) before joining.
 *  Caller frees.
 * =========================================================================== */

static int cb_cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: boot_notice @ hermes_cli/container_boot.py:boot_notice */
char *cb_boot_notice(const char *const *folded)
{
    int n = 0;
    if (folded) while (folded[n]) n++;

    const char **sorted = (const char **)calloc((size_t)n + 1, sizeof(char *));
    if (!sorted) return NULL;
    for (int i = 0; i < n; i++) sorted[i] = folded[i];
    if (n > 1) qsort(sorted, (size_t)n, sizeof(char *), cb_cmp_str);

    const char *head = "reconcile: profile gateway(s) ";
    const char *tail = " asked to autostart; one gateway per container serves every "
                       "non-standalone profile, so the ROOT slot (gateway-default) was "
                       "started instead and multiplexes them.";

    size_t cap = strlen(head) + strlen(tail) + 64;
    for (int i = 0; i < n; i++) cap += strlen(sorted[i]) + 2;
    char *out = (char *)malloc(cap);
    if (!out) { free(sorted); return NULL; }

    char *p = out;
    p += sprintf(p, "%s", head);
    for (int i = 0; i < n; i++) {
        if (i) p += sprintf(p, ", ");
        p += sprintf(p, "%s", sorted[i]);
    }
    sprintf(p, "%s", tail);
    free(sorted);
    return out;
}
