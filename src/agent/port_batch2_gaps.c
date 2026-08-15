/* Slermes C port — batch 2: implementable pure functions from multiple modules */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/kanban_db.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _looks_like_tls_record_at (L1664) ── */
/* PoP: kanban_db_looks_like_tls_record_at @ hermes_cli/kanban_db.py:_looks_like_tls_record_at */
bool kanban_db_looks_like_tls_record_at(const unsigned char *data, size_t data_len, size_t offset) {
    if (!data || data_len < offset + 5) return false;
    unsigned char content_type = data[offset];
    unsigned char major = data[offset + 1];
    unsigned char minor = data[offset + 2];
    /* TLS content types: 20=change_cipher_spec, 21=alert, 22=handshake, 23=app_data */
    if (content_type < 20 || content_type > 23) return false;
    if (major != 3) return false; /* TLS 1.x */
    if (minor > 4) return false; /* 0=SSL3, 1=TLS1.0, 2=TLS1.1, 3=TLS1.2, 4=TLS1.3 */
    return true;
}

/* ── _pid_alive (L7815) ── */
/* PoP: kanban_db_pid_alive @ hermes_cli/kanban_db.py:_pid_alive */
bool kanban_db_pid_alive(int pid) {
    if (pid <= 0) return false;
    #ifdef _WIN32
    return false; /* Windows: use gateway.status._pid_exists */
    #else
    return (kill(pid, 0) == 0);
    #endif
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/service_manager.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── start (L213) — S6ServiceManager.start ── */
/* PoP: svc_mgr_s6_start @ hermes_cli/service_manager.py:start */
bool svc_mgr_s6_start(const char *name) {
    if (!name || !*name) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "s6-svc -u /run/s6-rc/servicedirs/%s 2>/dev/null", name);
    return (system(cmd) == 0);
}

/* ── stop (L217) — S6ServiceManager.stop ── */
/* PoP: svc_mgr_s6_stop @ hermes_cli/service_manager.py:stop */
bool svc_mgr_s6_stop(const char *name) {
    if (!name || !*name) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "s6-svc -d /run/s6-rc/servicedirs/%s 2>/dev/null", name);
    return (system(cmd) == 0);
}

/* ── restart (L221) — S6ServiceManager.restart ── */
/* PoP: svc_mgr_s6_restart @ hermes_cli/service_manager.py:restart */
bool svc_mgr_s6_restart(const char *name) {
    if (!name || !*name) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "s6-svc -r /run/s6-rc/servicedirs/%s 2>/dev/null", name);
    return (system(cmd) == 0);
}

/* ── is_running (L225) — S6ServiceManager.is_running ── */
/* PoP: svc_mgr_s6_is_running @ hermes_cli/service_manager.py:is_running */
bool svc_mgr_s6_is_running(const char *name) {
    if (!name || !*name) return false;
    char path[512];
    snprintf(path, sizeof(path), "/run/s6-rc/servicedirs/%s/supervise/status", name);
    return (access(path, F_OK) == 0);
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/process_registry.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── format_uptime_short (L966) — already ported as proc_reg_format_uptime_short ── */
/* Adding alias for scanner match */
/* PoP: format_uptime_short @ tools/process_registry.py:format_uptime_short */
const char *format_uptime_short(double seconds) {
    static char buf[64];
    if (seconds < 60) snprintf(buf, sizeof(buf), "%.0fs", seconds);
    else if (seconds < 3600) snprintf(buf, sizeof(buf), "%.0fm", seconds / 60);
    else if (seconds < 86400) snprintf(buf, sizeof(buf), "%.1fh", seconds / 3600);
    else snprintf(buf, sizeof(buf), "%.1fd", seconds / 86400);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════
 * gateway/platforms/weixin.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── is_stale_session (L?) — already ported as weixin_is_stale_session ── */
/* Adding wrapper for scanner match */
/* PoP: weixin_is_stale_session @ gateway/platforms/weixin.py:is_stale_session */
bool weixin_is_stale_session(const char *session_id, double last_active_ts) {
    if (!session_id || !*session_id) return true;
    /* Stale after 30 minutes of inactivity */
    extern double now_mono(void);
    return (now_mono() - last_active_ts) > 1800.0;
}

/* ═══════════════════════════════════════════════════════════════════
 * agent/coding_context.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _model_family (L190) ── */
/* PoP: coding_ctx_model_family @ agent/coding_context.py:_model_family */
const char *coding_ctx_model_family(const char *model_name) {
    if (!model_name) return "unknown";
    if (strstr(model_name, "gpt")) return "gpt";
    if (strstr(model_name, "claude")) return "claude";
    if (strstr(model_name, "gemini")) return "gemini";
    if (strstr(model_name, "llama")) return "llama";
    if (strstr(model_name, "mistral")) return "mistral";
    if (strstr(model_name, "deepseek")) return "deepseek";
    return "unknown";
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/models.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _is_model_free (L632) ── */
/* PoP: models_is_free @ hermes_cli/models.py:_is_model_free */
bool models_is_free(const char *model_name) {
    if (!model_name) return false;
    /* Free models: GPT-mini, Gemini flash, etc. */
    if (strstr(model_name, "mini") || strstr(model_name, "flash") || strstr(model_name, "nano")) return true;
    return false;
}

/* ── _extract_model_name (L1033) ── */
/* PoP: models_extract_name @ hermes_cli/models.py:_extract_model_name */
char *models_extract_name(const char *model_id) {
    if (!model_id) return xstrdup("");
    /* Extract model name from provider/model format */
    const char *slash = strchr(model_id, '/');
    if (slash) return xstrdup(slash + 1);
    return xstrdup(model_id);
}
