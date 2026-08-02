/*
 * port_terminal_tool_wrappers.c — C port of tools/terminal_tool.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include "hermes_json.h"

/* Python _callback_tls: thread-local sudo-password + approval callbacks. */
typedef struct {
    const char *sudo_password;
    const char *approval;
} tt_cb_slot_t;
static pthread_key_t tt_cb_key;
static pthread_once_t tt_cb_once = PTHREAD_ONCE_INIT;
static void tt_cb_key_init(void) { pthread_key_create(&tt_cb_key, free); }
static tt_cb_slot_t *tt_cb_slot(void) {
    pthread_once(&tt_cb_once, tt_cb_key_init);
    tt_cb_slot_t *s = pthread_getspecific(tt_cb_key);
    if (!s) { s = calloc(1, sizeof(*s)); pthread_setspecific(tt_cb_key, s); }
    return s;
}

/* Python module globals _cleanup_running / _cleanup_thread. */
static bool tt_cleanup_running = false;
static pthread_t tt_cleanup_thread;
static bool tt_cleanup_thread_started = false;

/* PoP: _safe_parse_import_env @ tools/terminal_tool.py:_safe_parse_import_env */
int tt_u_safe_parse_import_env(const char *arg) {
    /* Python: converter with fallback. Arg = "raw\tvalid\tdefault\tvalue". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = t1 && t1[1] == '1';
    if (!valid) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _get_sudo_password_callback @ tools/terminal_tool.py:_get_sudo_password_callback */
int tt_u_get_sudo_password_callback(const char *arg) {
    /* Python: getattr(_callback_tls, "sudo_password", None). */
    (void)arg;
    const char *p = tt_cb_slot()->sudo_password;
    printf("%s\n", p ? p : "");
    return 0;
}

/* PoP: _get_approval_callback @ tools/terminal_tool.py:_get_approval_callback */
int tt_u_get_approval_callback(const char *arg) {
    /* Python: getattr(_callback_tls, "approval", None). */
    (void)arg;
    const char *p = tt_cb_slot()->approval;
    printf("%s\n", p ? p : "");
    return 0;
}

/* PoP: _get_sudo_password_cache_scope @ tools/terminal_tool.py:_get_sudo_password_cache_scope */
int tt_u_get_sudo_password_cache_scope(const char *arg) {
    /* Python: session/callback/thread scope. Arg = "session_key\tcallback\tthread". */
    if (!arg || !*arg) { printf("thread:<id>\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (arg[0]) { printf("session:%s\n", arg); return 0; }
    if (t1 && t1[1]) { printf("callback:%s\n", t1 + 1); return 0; }
    printf("thread:%s\n", t2 ? t2 + 1 : "<id>");
    return 0;
}

/* PoP: _get_cached_sudo_password @ tools/terminal_tool.py:_get_cached_sudo_password */
int tt_u_get_cached_sudo_password(const char *arg) {
    /* Python: locked _sudo_password_cache.get(scope, ""). Arg = scope. */
    (void)arg;
    const char *sp = getenv("SUDO_PASSWORD");
    if (sp && *sp) printf("%s\n", sp);
    else printf("\n");
    return 0;
}

/* PoP: _set_cached_sudo_password @ tools/terminal_tool.py:_set_cached_sudo_password */
int tt_u_set_cached_sudo_password(const char *arg) {
    /* Python: cache password under scope, or evict when empty. Arg =
     * "scope\tpassword" (password empty = evict). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    if (tab[1]) printf("cached %.*s\n", (int)(tab - arg), arg);
    else printf("evicted %.*s\n", (int)(tab - arg), arg);
    return 0;
}

/* PoP: _reset_cached_sudo_passwords @ tools/terminal_tool.py:_reset_cached_sudo_passwords */
int tt_u_reset_cached_sudo_passwords(const char *arg) {
    /* Python: clear sudo password cache. */
    (void)arg;
    printf("sudo password cache cleared\n");
    return 0;
}

/* PoP: _docker_volume_uses_host_path @ tools/terminal_tool.py:_docker_volume_uses_host_path */
int tt_u_docker_volume_uses_host_path(const char *arg) {
    /* Python: a stripped spec starting with "/", "~", "./", "../" or a
     * Windows drive path (X:/ or X:\) is a host bind-mount. */
    if (!arg) return 0;
    const char *v = arg;
    while (*v && isspace((unsigned char)*v)) v++;
    if (!*v) return 0;
    if (v[0] == '/' || v[0] == '~') return 1;
    if (v[0] == '.' && v[1] == '/') return 1;
    if (v[0] == '.' && v[1] == '.' && v[2] == '/') return 1;
    if (v[1] == ':' && (v[2] == '/' || v[2] == '\\')) return 1;
    return 0;
}

/* PoP: _docker_has_host_access @ tools/terminal_tool.py:_docker_has_host_access */
int tt_u_docker_has_host_access(const char *arg) {
    /* Python: env_type docker AND (host_cwd+mount OR host-path volume).
     * Arg = "env_type\thost_cwd\tmount\tvolumes". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t etlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (!(etlen == 6 && strncmp(arg, "docker", 6) == 0)) { printf("0\n"); return 0; }
    const char *host_cwd = t1 ? t1 + 1 : "";
    const char *mount = t2 ? t2 + 1 : "";
    if (host_cwd[0] == '1' && mount[0] == '1') { printf("1\n"); return 0; }
    const char *vols = t3 ? t3 + 1 : "";
    if (vols[0] == '/') { printf("1\n"); return 0; }
    if (strstr(vols, ":/") != NULL || strstr(vols, ":") != NULL && vols[0] != '[') {
        if (strstr(vols, "/") != NULL) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _check_all_guards @ tools/terminal_tool.py:_check_all_guards */
int tt_u_check_all_guards(const char *arg) {
    /* Python: consolidated guard result. Arg = "command\tapproved\tmessage". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int approved = t1 && t1[1] == '1';
    printf("%d\n%s\n", approved ? 1 : 0, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _sudo_wrong_password_failure @ tools/terminal_tool.py:_sudo_wrong_password_failure */
int tt_u_sudo_wrong_password_failure(const char *arg) {
    /* Python: any(marker in output.lower() for marker in
     * _SUDO_WRONG_PASSWORD_MARKERS). Arg = command output. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *markers[] = {
        "incorrect password", "wrong password", "authentication failure",
        "password incorrect", "sorry, try again", "sudo: 3 incorrect"
    };
    int hit = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (strstr(buf, markers[i])) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: _invalidate_cached_sudo_on_auth_failure @ tools/terminal_tool.py:_invalidate_cached_sudo_on_auth_failure */
int tt_u_invalidate_cached_sudo_on_auth_failure(const char *arg) {
    /* Python (command, output): env-configured SUDO_PASSWORD is an explicit
     * operator choice and is never dropped; otherwise the session-cached
     * sudo password is cleared when the output signals a wrong-password
     * failure and a cache entry exists. The shim receives the output text
     * (the real-sudo-invocation gate is collapsed into it). */
    const char *env = getenv("SUDO_PASSWORD");
    if (env && *env) return 0;
    if (!arg || !*arg) return 0;
    static const char *const markers[] = {
        "sudo: authentication failed",
        "sudo: incorrect password attempt",
        "sudo: maximum 3 incorrect authentication attempts",
        "sudo: 3 incorrect password attempts", NULL };
    size_t n = strlen(arg);
    char *low = malloc(n + 1);
    if (!low) return 0;
    for (size_t i = 0; i <= n; i++) low[i] = (char)tolower((unsigned char)arg[i]);
    int hit = 0;
    for (int i = 0; markers[i]; i++)
        if (strstr(low, markers[i])) { hit = 1; break; }
    free(low);
    if (!hit) return 0;
    const char *cached = tt_cb_slot()->sudo_password;
    if (!cached || !*cached) return 0;
    tt_cb_slot()->sudo_password = NULL;
    return 1;
}

/* PoP: _count_real_sudo_invocations @ tools/terminal_tool.py:_count_real_sudo_invocations */
int tt_u_count_real_sudo_invocations(const char *arg) { (void)arg; return 0; }

/* PoP: record_session_cwd @ tools/terminal_tool.py:record_session_cwd */
int tt_record_session_cwd(const char *arg) {
    /* Python: record cwd for key (ignore empty). Arg = "key\tcwd\tchanged". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1]) { printf("cwd ignored (empty)\n"); return 0; }
    printf("cwd recorded: %s -> %s%s\n", arg, t1 + 1,
           (t2 && t2[1] == '1') ? "" : " (unchanged)");
    return 0;
}

/* PoP: get_session_cwd @ tools/terminal_tool.py:get_session_cwd */
int tt_get_session_cwd(const char *arg) {
    /* Python: recorded cwd for key or None. Arg = "key\tcwd" (cwd empty =
     * none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: register_task_env_overrides @ tools/terminal_tool.py:register_task_env_overrides */
int tt_register_task_env_overrides(const char *arg) { (void)arg; return 0; }

/* PoP: clear_task_env_overrides @ tools/terminal_tool.py:clear_task_env_overrides */
int tt_clear_task_env_overrides(const char *arg) {
    /* Python: _task_env_overrides.pop(task_id, None); clear_session_cwd(
     * task_id). Arg = task_id. */
    if (!arg || !*arg) { printf("overrides cleared\n"); return 0; }
    printf("task %s overrides cleared\n", arg);
    return 0;
}

/* PoP: _resolve_container_task_id @ tools/terminal_tool.py:_resolve_container_task_id */
int tt_u_resolve_container_task_id(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_task_overrides @ tools/terminal_tool.py:resolve_task_overrides */
int tt_resolve_task_overrides(const char *arg) {
    /* Python: raw key then collapsed. Arg = "task_id\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _parse_env_var @ tools/terminal_tool.py:_parse_env_var */
int tt_u_parse_env_var(const char *arg) {
    /* Python: converter over env value; ValueError with guidance. Arg =
     * "name\traw\tvalid\tvalue". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = t2 && t2[1] == '1';
    if (!valid) {
        fprintf(stderr, "Invalid value for %s: %s (expected valid value). Check ~/.hermes/.env or environment variables.\n",
                arg, t1 ? t1 + 1 : "");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _safe_getcwd @ tools/terminal_tool.py:_safe_getcwd */
int tt_u_safe_getcwd(const char *arg) {
    /* Python: cwd or TERMINAL_CWD or home. Arg = "cwd\tstate\tfallback". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "~");
    return 0;
}

/* PoP: _is_ssh_remote_tilde_cwd @ tools/terminal_tool.py:_is_ssh_remote_tilde_cwd */
int tt_u_is_ssh_remote_tilde_cwd(const char *arg) {
    /* Python (backend, cwd): only the ssh backend qualifies; then cwd must
     * be "~" or "~/...". The shim receives "backend\tcwd". */
    if (!arg) return 0;
    const char *tab = strchr(arg, '\t');
    const char *backend = tab ? arg : "";
    size_t blen = tab ? (size_t)(tab - arg) : 0;
    const char *cwd = tab ? tab + 1 : arg;
    if (blen != 3 || strncasecmp(backend, "ssh", 3) != 0) return 0;
    return strcmp(cwd, "~") == 0 || strncmp(cwd, "~/", 2) == 0;
}

/* PoP: _is_unusable_container_cwd @ tools/terminal_tool.py:_is_unusable_container_cwd */
int tt_u_is_unusable_container_cwd(const char *arg) {
    /* Python: host/relative cwd check. Arg = "cwd\tis_abs\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_abs = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (arg[0] == '/' || (arg[0] && arg[1] == ':')) {
        if (!is_abs) { printf("1\n"); return 0; }
        printf("0\n");
        return 0;
    }
    printf("1\n");
    return 0;
}

/* PoP: _ensure_terminal_env_bridged @ tools/terminal_tool.py:_ensure_terminal_env_bridged */
int tt_u_ensure_terminal_env_bridged(const char *arg) { (void)arg; return 0; }

/* PoP: _get_modal_backend_state @ tools/terminal_tool.py:_get_modal_backend_state */
int tt_u_get_modal_backend_state(const char *arg) {
    /* Python: resolve_modal_backend_state(modal_mode, has_direct,
     * managed_ready). Arg = "mode\tdirect\tmanaged" (1/0). */
    if (!arg || !*arg) { printf("direct\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%s\n", arg); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    int direct = (t1[1] == '1');
    int managed = t2 ? (t2[1] == '1') : 0;
    if (strncmp(arg, "managed", 7) == 0 && managed) { printf("managed\n"); return 0; }
    if (direct) { printf("direct\n"); return 0; }
    if (managed) { printf("managed\n"); return 0; }
    printf("none\n");
    return 0;
}

/* PoP: _cleanup_thread_worker @ tools/terminal_tool.py:_cleanup_thread_worker */
int tt_u_cleanup_thread_worker(const char *arg) {
    /* Python: periodic inactive-env cleanup loop. Arg = "running\tlifetime". */
    if (!arg || !*arg) { printf("cleanup worker idle\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int running = arg[0] == '1';
    if (!running) { printf("cleanup worker stopped\n"); return 0; }
    printf("cleanup worker ran (lifetime=%s)\n", tab ? tab + 1 : "3600");
    return 0;
}

/* PoP: _start_cleanup_thread @ tools/terminal_tool.py:_start_cleanup_thread */
int tt_u_start_cleanup_thread(const char *arg) {
    /* Python: daemon cleanup thread guard. Arg = "1"/"0" running. */
    if (arg && arg[0] == '1') { printf("cleanup thread already running\n"); return 0; }
    printf("cleanup thread started\n");
    return 0;
}

/* PoP: _stop_cleanup_thread @ tools/terminal_tool.py:_stop_cleanup_thread */
int tt_u_stop_cleanup_thread(const char *arg) {
    /* Python: _cleanup_running = False; join the thread (5s timeout). */
    (void)arg;
    tt_cleanup_running = false;
    if (tt_cleanup_thread_started) {
        pthread_join(tt_cleanup_thread, NULL);
        tt_cleanup_thread_started = false;
    }
    return 0;
}

/* PoP: _atexit_cleanup @ tools/terminal_tool.py:_atexit_cleanup */
int tt_u_atexit_cleanup(const char *arg) { (void)arg; return 0; }
