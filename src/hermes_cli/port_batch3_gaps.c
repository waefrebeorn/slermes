/* Slermes C port — batch 3: hermes_cli/models.py, hermes_cli/banner.py,
 * hermes_cli/update_cmd.py, hermes_cli/config.py, hermes_cli/auth.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include <time.h>

static double now_mono(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/models.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _is_model_free (L632) ── */
/* PoP: models_is_model_free @ hermes_cli/models.py:_is_model_free */
bool models_is_model_free(const char *model_id, json_t *pricing) {
    if (!model_id || !pricing) return false;
    double prompt_val = json_get_num(pricing, "prompt", 1.0);
    double completion_val = json_get_num(pricing, "completion", 1.0);
    return (prompt_val == 0.0 && completion_val == 0.0);
}

/* ── _extract_model_name (L1033) ── */
/* PoP: models_extract_model_name @ hermes_cli/models.py:_extract_model_name */
char *models_extract_model_name(json_t *entry) {
    if (!entry) return NULL;
    const char *model_name = json_get_str(entry, "modelName", NULL);
    if (model_name && *model_name) return xstrdup(model_name);
    return NULL;
}

/* ── _openrouter_model_is_free (L?) ── */
/* PoP: models_openrouter_is_free @ hermes_cli/models.py:_openrouter_model_is_free */
bool models_openrouter_is_free(json_t *pricing) {
    return models_is_model_free(NULL, pricing);
}

/* ── _openrouter_model_supports_tools (L?) ── */
/* PoP: models_openrouter_supports_tools @ hermes_cli/models.py:_openrouter_model_supports_tools */
bool models_openrouter_supports_tools(json_t *item) {
    if (!item) return false;
    return json_get_bool(item, "supports_tools", true);
}

/* ── _format_price_per_mtok (L?) ── */
/* PoP: models_format_price_per_mtok @ hermes_cli/models.py:_format_price_per_mtok */
char *models_format_price_per_mtok(const char *per_token_str) {
    if (!per_token_str) return xstrdup("0");
    double val = atof(per_token_str);
    char buf[32];
    snprintf(buf, sizeof(buf), "$%.2f/M", val * 1000000);
    return xstrdup(buf);
}

/* ── compute_sale_discount (L?) ── */
/* PoP: models_compute_sale_discount_batch3 @ hermes_cli/models.py:compute_sale_discount */
double models_compute_sale_discount_batch3(double original_price, double sale_price) {
    if (original_price <= 0) return 0;
    return (1.0 - sale_price / original_price) * 100.0;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/banner.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _git_stdout (L177) ── */
/* PoP: banner_git_stdout @ hermes_cli/banner.py:_git_stdout */
char *banner_git_stdout(const char *args) {
    if (!args) return xstrdup("");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "git %s 2>/dev/null", args);
    FILE *fp = popen(cmd, "r");
    if (!fp) return xstrdup("");
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    /* Strip trailing newline */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return xstrdup(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/config.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _config_path (L?) ── */
/* PoP: config_path @ hermes_cli/config.py:_config_path */
char *config_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    return xstrdup(path);
}

/* ── _config_dir (L?) ── */
/* PoP: config_dir @ hermes_cli/config.py:_config_dir */
char *config_dir(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes", home);
    return xstrdup(path);
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/auth.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _token_path (L?) ── */
/* PoP: auth_token_path @ hermes_cli/auth.py:_token_path */
char *auth_token_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);
    return xstrdup(path);
}

/* ── _is_token_expired (L?) ── */
/* PoP: auth_is_token_expired @ hermes_cli/auth.py:_is_token_expired */
bool auth_is_token_expired(json_t *token_data) {
    if (!token_data) return true;
    double exp_ts = json_get_num(token_data, "expires_at", 0);
    if (exp_ts == 0) return false; /* no expiry = never expires */
    extern double now_mono(void);
    return now_mono() >= exp_ts;
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/update_cmd.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _update_lock_path (L?) ── */
/* PoP: update_lock_path @ hermes_cli/update_cmd.py:_update_lock_path */
char *update_lock_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/.update_lock", home);
    return xstrdup(path);
}

/* ── _is_update_locked (L?) ── */
/* PoP: update_is_locked @ hermes_cli/update_cmd.py:_is_update_locked */
bool update_is_locked(void) {
    char *path = update_lock_path();
    bool locked = (access(path, F_OK) == 0);
    free(path);
    return locked;
}
