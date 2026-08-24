/*
 * command_token_source.c — PoP of agent/command_token_source.py.
 *
 * Python -> C symbol map (1:1):
 *   _MINT_TIMEOUT_SECONDS          -> CMD_TOKEN_MINT_TIMEOUT_SECONDS
 *   _TOKEN_REFRESH_LEEWAY_SECONDS  -> CMD_TOKEN_REFRESH_LEEWAY_SECONDS
 *   _NO_TTL_REFRESH_SECONDS        -> CMD_TOKEN_NO_TTL_REFRESH_SECONDS
 *   CommandTokenError              -> error string returned via out-param
 *   _mint                          -> command_token_source__mint
 *   CommandTokenSource             -> struct command_token_source
 *   CommandTokenSource.__call__    -> command_token_source_call
 *   build_command_token_provider   -> build_command_token_provider
 */
#define _POSIX_C_SOURCE 200809L
#include "command_token_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include "hermes_json.h"
#include "auth_helpers.h" /* auth_parse_iso_timestamp — hermes_cli.auth._parse_iso_timestamp */

const int CMD_TOKEN_MINT_TIMEOUT_SECONDS = 15;
const double CMD_TOKEN_REFRESH_LEEWAY_SECONDS = 60.0;
const double CMD_TOKEN_NO_TTL_REFRESH_SECONDS = 900.0;

static double mono_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* SIGALRM fires when the mint budget expires; the interrupted stdio read
 * then fails and we report the timeout like subprocess.TimeoutExpired. */
static volatile sig_atomic_t g_mint_timed_out = 0;

static void mint_alarm_handler(int sig)
{
    (void)sig;
    g_mint_timed_out = 1;
}

char *command_token_source__mint(const char *command, const char *label,
                                 char *err, size_t err_sz, double *out_ttl)
{
    char default_label[] = "";
    if (!label) label = default_label;
    if (out_ttl) *out_ttl = -1.0;
    if (err && err_sz) err[0] = '\0';

    g_mint_timed_out = 0;
    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = mint_alarm_handler;
    sigaction(SIGALRM, &sa, &old_sa);
    alarm((unsigned)CMD_TOKEN_MINT_TIMEOUT_SECONDS);

    FILE *fp = popen(command, "r");
    if (!fp) {
        alarm(0);
        sigaction(SIGALRM, &old_sa, NULL);
        snprintf(err, err_sz,
                 "key_cmd for provider '%s' could not be executed: popen failed",
                 label);
        return NULL;
    }

    /* capture_output=True, text=True: read all of stdout */
    size_t cap = 4096, len = 0;
    char *stdout_buf = malloc(cap);
    size_t n;
    while ((n = fread(stdout_buf + len, 1, cap - len, fp)) > 0) {
        len += n;
        if (len == cap) { cap *= 2; stdout_buf = realloc(stdout_buf, cap); }
    }
    int status = pclose(fp);
    alarm(0);
    sigaction(SIGALRM, &old_sa, NULL);

    if (g_mint_timed_out) {
        free(stdout_buf);
        snprintf(err, err_sz, "key_cmd for provider '%s' timed out after %ds",
                 label, CMD_TOKEN_MINT_TIMEOUT_SECONDS);
        return NULL;
    }
    stdout_buf[len] = '\0';

    int returncode = status == -1 ? -1 : WEXITSTATUS(status);
    if (returncode != 0) {
        /* NEVER echo stdout/stderr or the command string: both can carry the
         * very credential this module protects. Name only the provider. */
        snprintf(err, err_sz,
                 "key_cmd for provider '%s' exited %ld. Run that provider's "
                 "key_cmd manually to see why (e.g. `databricks auth login` "
                 "if its OAuth session expired).", label, (long)returncode);
        free(stdout_buf);
        return NULL;
    }

    /* stdout.strip() emptiness check */
    int blank = 1;
    for (size_t i = 0; i < len; i++)
        if (!isspace((unsigned char)stdout_buf[i])) { blank = 0; break; }
    if (blank) {
        snprintf(err, err_sz, "key_cmd for provider '%s' produced no output",
                 label);
        free(stdout_buf);
        return NULL;
    }

    /* JSON payload path — stdout.lstrip().startswith("{") */
    const char *ls = stdout_buf;
    while (*ls && isspace((unsigned char)*ls)) ls++;
    if (*ls == '{') {
        json_t *payload = json_parse(stdout_buf, NULL); /* decode failure => None */
        if (payload && payload->type == JSON_OBJECT) {
            const char *at = json_get_str(payload, "access_token", "");
            char token[8192];
            snprintf(token, sizeof token, "%s", at ? at : "");
            /* .strip() */
            char *t = token;
            while (*t && isspace((unsigned char)*t)) t++;
            char *e = t + strlen(t);
            while (e > t && isspace((unsigned char)e[-1])) e--;
            *e = '\0';
            if (!*t) {
                snprintf(err, err_sz,
                         "key_cmd for provider '%s' returned JSON without an "
                         "'access_token' field", label);
                json_free(payload);
                free(stdout_buf);
                return NULL;
            }
            json_t *ttl_n = json_object_get(payload, "expires_in");
            double ttl_val = ttl_n && ttl_n->type == JSON_NUMBER
                                 ? ttl_n->num_val : -1.0;
            if (ttl_val > 0) {
                char *out = strdup(t);
                if (out_ttl) *out_ttl = ttl_val;
                json_free(payload);
                free(stdout_buf);
                return out;
            }
            /* Relative lifetime absent: try absolute ISO 8601 deadlines. */
            const char *fields[] = { "expiry", "expiresOn" };
            for (int i = 0; i < 2; i++) {
                const char *fv = NULL;
                json_t *fn = json_object_get(payload, fields[i]);
                if (fn && fn->type == JSON_STRING) fv = fn->str_val;
                double deadline = fv ? auth_parse_iso_timestamp(fv) : -1.0;
                if (deadline > 0) {
                    double remaining = deadline - (double)time(NULL);
                    if (remaining > 0) {
                        char *out = strdup(t);
                        if (out_ttl) *out_ttl = remaining;
                        json_free(payload);
                        free(stdout_buf);
                        return out;
                    }
                }
            }
            char *out = strdup(t); /* ttl stays None */
            json_free(payload);
            free(stdout_buf);
            return out;
        }
        if (payload) json_free(payload);
        /* JSONDecodeError => payload None => fall through to bare-token path */
    }

    /* Bare token: strip surrounding whitespace, take verbatim. Multiple
     * lines are an explicit refusal, not silently-truncated credentials. */
    char *b = stdout_buf;
    while (*b && isspace((unsigned char)*b)) b++;
    char *be = b + strlen(b);
    while (be > b && isspace((unsigned char)be[-1])) be--;
    *be = '\0';
    if (strchr(b, '\n')) {
        snprintf(err, err_sz,
                 "key_cmd for provider '%s' printed multiple lines; it must "
                 "print only the token (or JSON with an 'access_token' field)",
                 label);
        free(stdout_buf);
        return NULL;
    }
    char *out = strdup(b);
    free(stdout_buf);
    return out;
}

int command_token_source_init(command_token_source_t *s, const char *command,
                              const char *label)
{
    if (!s || !command) return -1;
    s->command = strdup(command);
    s->label = strdup(label && *label ? label : "custom");
    pthread_mutex_init(&s->lock, NULL);
    s->token = strdup("");
    s->expires_at = 0.0;
    return 0;
}

void command_token_source_free(command_token_source_t *s)
{
    if (!s) return;
    free(s->command);
    free(s->label);
    free(s->token);
    pthread_mutex_destroy(&s->lock);
    free(s);
}

char *command_token_source_call(command_token_source_t *s)
{
    if (!s) return NULL;
    pthread_mutex_lock(&s->lock);
    if (s->token && *s->token && mono_now() < s->expires_at) {
        char *out = strdup(s->token);
        pthread_mutex_unlock(&s->lock);
        return out;
    }
    char err[1024];
    double ttl = -1.0;
    char *token = command_token_source__mint(s->command, s->label,
                                             err, sizeof err, &ttl);
    if (!token) {
        /* Python re-raises CommandTokenError to the caller; surface as NULL. */
        pthread_mutex_unlock(&s->lock);
        return NULL;
    }
    free(s->token);
    s->token = token;
    double now = mono_now();
    s->expires_at = ttl > 0
        ? now + (ttl - CMD_TOKEN_REFRESH_LEEWAY_SECONDS > 5.0
                     ? ttl - CMD_TOKEN_REFRESH_LEEWAY_SECONDS : 5.0)
        : now + CMD_TOKEN_NO_TTL_REFRESH_SECONDS;
    char *out = strdup(s->token);
    pthread_mutex_unlock(&s->lock);
    return out;
}

/* PoP: build_command_token_provider @ agent/command_token_source.py:build_command_token_provider */
command_token_source_t *build_command_token_provider(const char *key_cmd,
                                                     const char *provider_label)
{
    /* str(key_cmd or "").strip() — blank means "no dynamic provider". */
    if (!key_cmd) return NULL;
    const char *b = key_cmd;
    while (*b && isspace((unsigned char)*b)) b++;
    char *e = (char *)b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    if (e == b) return NULL;
    char *command = malloc((size_t)(e - b) + 1);
    memcpy(command, b, (size_t)(e - b));
    command[e - b] = '\0';
    command_token_source_t *s = malloc(sizeof *s);
    command_token_source_init(s, command, provider_label);
    free(command);
    return s;
}
