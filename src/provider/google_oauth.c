/*
 * google_oauth.c — Google OAuth credential management for Hermes C.
 * Port of Python agent/google_oauth.py.
 *
 * Manages persistent OAuth tokens for Google Gemini API.
 * Uses ~/.hermes/auth/google_credentials.json for storage.
 *
 * Uses the shared OAuth infrastructure from token_exchange.c
 * (oauth_refresh_token, PKCE, etc.) + added Google-specific logic.
 */

#include "google_oauth.h"
#include "hermes_core_types.h"
#include "hermes_http.h"
#include "hermes_auth.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "libcrypto/crypto.h"

/* ─── Constants ────────────────────────────────────────────────── */

#define GOOGLE_TOKEN_URL       "https://oauth2.googleapis.com/token"
#define GOOGLE_PEOPLE_API      "https://people.googleapis.com/v1/people/me?personFields=emailAddresses"
#define GOOGLE_REFRESH_SCOPE   "https://www.googleapis.com/auth/cloud-platform"

/* Default client credentials for Gemini CLI (from Python gemini-cli oauth.js) */
#define DEFAULT_CLIENT_ID      "123456789-abcdef.apps.googleusercontent.com"
#define DEFAULT_CLIENT_SECRET  "GOCSPX-xxxxxxxxxxxx"

/* ─── Static state ─────────────────────────────────────────────── */

static char g_home[2048] = {0};
static char g_client_id[256] = {0};
static char g_client_secret[256] = {0};

/* ─── Internal helpers ─────────────────────────────────────────── */

static const char *get_home(void) {
    if (g_home[0]) return g_home;
    const char *env = getenv("HERMES_HOME");
    if (env && *env) {
        snprintf(g_home, sizeof(g_home), "%s", env);
        return g_home;
    }
    const char *home = getenv("HOME");
    if (home) {
        snprintf(g_home, sizeof(g_home), "%s/.hermes", home);
        return g_home;
    }
    return NULL;
}

/* Port of Python: _get_client_id */
static const char *get_client_id(void) {
    const char *env = getenv("GOOGLE_CLIENT_ID");
    if (env && *env) return env;
    if (g_client_id[0]) return g_client_id;
    return DEFAULT_CLIENT_ID;
}

/* Port of Python: _require_client_id */
const char *require_client_id(void) {
    const char *id = get_client_id();
    if (!id || !*id) return DEFAULT_CLIENT_ID;
    return id;
}

/* Port of Python: _get_client_secret */
const char *get_client_secret(void) {
    const char *env = getenv("GOOGLE_CLIENT_SECRET");
    if (env && *env) return env;
    if (g_client_secret[0]) return g_client_secret;
    return DEFAULT_CLIENT_SECRET;
}

/* Port of Python: _credentials_path */
static int build_creds_path(char *buf, size_t buf_sz) {
    const char *home = get_home();
    if (!home) return -1;
    return snprintf(buf, buf_sz, "%s/auth/google_credentials.json", home);
}

/* Parse a JSON credentials file into our struct.
 * Returns true on success. */
/* Port of Python: GoogleCredentials.from_dict */
static bool parse_creds_json(const char *json_str, google_oauth_creds_t *out) {
    if (!json_str || !out) return false;
    memset(out, 0, sizeof(*out));

    json_t *root = json_parse(json_str, NULL);
    if (!root || root->type != JSON_OBJECT) {
        json_free(root);
        return false;
    }

    const char *val;
    val = json_get_str(root, "access_token", NULL);
    if (val) out->access_token = strdup(val);
    val = json_get_str(root, "refresh_token", NULL);
    if (val) out->refresh_token = strdup(val);
    val = json_get_str(root, "id_token", NULL);
    if (val) out->id_token = strdup(val);
    val = json_get_str(root, "scope", NULL);
    if (val) out->scope = strdup(val);
    val = json_get_str(root, "token_type", NULL);
    if (val) out->token_type = strdup(val);
    out->expires_at = json_get_num(root, "expires_at", 0);
    val = json_get_str(root, "email", NULL);
    if (val) out->email = strdup(val);
    val = json_get_str(root, "project_id", NULL);
    if (val) out->project_id = strdup(val);
    val = json_get_str(root, "managed_project_id", NULL);
    if (val) out->managed_project_id = strdup(val);

    json_free(root);
    return (out->access_token != NULL);
}

/* Serialize credentials struct to JSON string (malloc'd). */
/* Port of Python: GoogleCredentials.to_dict */
static char *serialize_creds(const google_oauth_creds_t *creds) {
    if (!creds) return NULL;
    json_t *root = json_object();
    if (creds->access_token)
        json_set(root, "access_token", json_string(creds->access_token));
    if (creds->refresh_token)
        json_set(root, "refresh_token", json_string(creds->refresh_token));
    if (creds->id_token)
        json_set(root, "id_token", json_string(creds->id_token));
    if (creds->scope)
        json_set(root, "scope", json_string(creds->scope));
    if (creds->token_type)
        json_set(root, "token_type", json_string(creds->token_type));
    if (creds->expires_at > 0)
        json_set(root, "expires_at", json_number(creds->expires_at));
    if (creds->email)
        json_set(root, "email", json_string(creds->email));
    if (creds->project_id)
        json_set(root, "project_id", json_string(creds->project_id));
    if (creds->managed_project_id)
        json_set(root, "managed_project_id", json_string(creds->managed_project_id));
    char *out = json_serialize(root);
    json_free(root);
    return out;
}

/* ─── Lifecycle ────────────────────────────────────────────────── */

void google_oauth_creds_free(google_oauth_creds_t *creds) {
    if (!creds) return;
    free(creds->access_token);
    free(creds->refresh_token);
    free(creds->id_token);
    free(creds->scope);
    free(creds->token_type);
    free(creds->email);
    free(creds->project_id);
    free(creds->managed_project_id);
    memset(creds, 0, sizeof(*creds));
}

/* ─── Credential I/O ──────────────────────────────────────────── */

/* mkdir -p ~/.hermes/auth/ */
static bool ensure_auth_dir(void) {
    const char *home = get_home();
    if (!home) return false;
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/auth", home);
    char mkcmd[4096];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s' 2>/dev/null", dir);
    int ret = system(mkcmd);
    (void)ret;
    /* chmod 0700 on auth dir */
    chmod(dir, 0700);
    return true;
}

/* Port of Python: load_credentials */
google_oauth_creds_t *google_oauth_load_credentials(void) {
    char path[2048];
    if (build_creds_path(path, sizeof(path)) < 0) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[nread] = '\0';

    google_oauth_creds_t *creds = (google_oauth_creds_t *)malloc(sizeof(google_oauth_creds_t));
    if (!creds) { free(buf); return NULL; }

    if (!parse_creds_json(buf, creds)) {
        free(buf);
        free(creds);
        return NULL;
    }
    free(buf);

    if (!creds->access_token) {
        google_oauth_creds_free(creds);
        free(creds);
        return NULL;
    }
    return creds;
}

/* Port of Python: save_credentials */
bool save_credentials(const google_oauth_creds_t *creds) {
    if (!creds) return false;
    char path[2048];
    if (build_creds_path(path, sizeof(path)) < 0) return false;
    if (!ensure_auth_dir()) return false;

    char *json_str = serialize_creds(creds);
    if (!json_str) return false;

    /* Atomic write: write to temp file, then rename */
    char tmp_path[2064];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", path, (int)getpid());

    /* Secure parent directory */
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/auth", get_home());
    hermes_file_permissions_harden(dir, NULL, NULL, geteuid());

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { free(json_str); return false; }

    size_t len = strlen(json_str);
    ssize_t written = write(fd, json_str, len);
    if (written < 0 || (size_t)written != len) {
        close(fd);
        unlink(tmp_path);
        free(json_str);
        return false;
    }
    fsync(fd);
    close(fd);

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        free(json_str);
        return false;
    }

    free(json_str);
    return true;
}

/* Port of Python: clear_credentials */
void clear_credentials(void) {
    char path[2048];
    if (build_creds_path(path, sizeof(path)) < 0) return;
    unlink(path);
}

/* ─── Token operations ────────────────────────────────────────── */

/* Check if access token is expired (or about to expire in <60s). */
/* Port of Python: GoogleCredentials.access_token_expired */
static bool token_is_expired(double expires_at) {
    if (expires_at <= 0) return false; /* unknown expiry, assume valid */
    double now = (double)time(NULL);
    return (now + 60) >= expires_at; /* expired if within 60s of expiry */
}

/* Port of Python: get_valid_access_token */
char *google_oauth_get_valid_token(char **out_error) {
    if (out_error) *out_error = NULL;

    google_oauth_creds_t *creds = google_oauth_load_credentials();
    if (!creds) {
        if (out_error) *out_error = strdup("No Google OAuth credentials found. Run 'hermes /auth google' first.");
        return NULL;
    }

    /* Check if token needs refresh */
    if (creds->refresh_token && token_is_expired(creds->expires_at)) {
        google_oauth_creds_free(creds);
        free(creds);

        /* Try to refresh */
        if (!google_oauth_refresh_token()) {
            if (out_error) *out_error = strdup("Failed to refresh Google OAuth token.");
            return NULL;
        }

        /* Reload after refresh */
        creds = google_oauth_load_credentials();
        if (!creds) {
            if (out_error) *out_error = strdup("Google OAuth token refresh succeeded but reload failed.");
            return NULL;
        }
    }

    char *token = creds->access_token ? strdup(creds->access_token) : NULL;
    if (!token) {
        if (out_error) *out_error = strdup("Google OAuth token not found.");
    }

    google_oauth_creds_free(creds);
    free(creds);
    return token;
}

/* Port of Python: refresh_access_token */
bool google_oauth_refresh_token(void) {
    google_oauth_creds_t *creds = google_oauth_load_credentials();
    if (!creds || !creds->refresh_token) {
        if (creds) { google_oauth_creds_free(creds); free(creds); }
        return false;
    }

    /* Use shared OAuth refresh — takes (url, client_id, refresh_token, timeout_sec) */
    bool ok = false;
    oauth_token_t *new_tok = oauth_refresh_token(
        GOOGLE_TOKEN_URL,
        get_client_id(),
        creds->refresh_token,
        30);
    if (!new_tok) {
        google_oauth_creds_free(creds);
        free(creds);
        return false;
    }

    /* Save the refreshed token */
    google_oauth_creds_t new_creds;
    memset(&new_creds, 0, sizeof(new_creds));
    new_creds.access_token = new_tok->access_token ? strdup(new_tok->access_token) : NULL;
    new_creds.refresh_token = new_tok->refresh_token ? strdup(new_tok->refresh_token) : NULL;
    new_creds.id_token = new_tok->id_token ? strdup(new_tok->id_token) : NULL;
    new_creds.token_type = new_tok->token_type ? strdup(new_tok->token_type) : NULL;
    new_creds.expires_at = new_tok->expires_at;

    /* Load old creds to preserve email/project fields */
    google_oauth_creds_t *old = creds; /* alias — already loaded */
    if (old) {
        if (old->email && !new_creds.email) new_creds.email = strdup(old->email);
        if (old->project_id) new_creds.project_id = strdup(old->project_id);
        if (old->managed_project_id) new_creds.managed_project_id = strdup(old->managed_project_id);
        if (old->scope && !new_creds.scope) new_creds.scope = strdup(old->scope);
    }

    oauth_token_free(new_tok);
    free(new_tok);
    google_oauth_creds_free(creds);
    free(creds);

    ok = save_credentials(&new_creds);

    /* Free the moved fields (they were strdup'd into new_creds) */
    free(new_creds.access_token);
    free(new_creds.refresh_token);
    free(new_creds.id_token);
    free(new_creds.token_type);
    free(new_creds.email);
    free(new_creds.project_id);
    free(new_creds.managed_project_id);
    free(new_creds.scope);

    return ok;
}

/* Port of Python: _fetch_user_email */
char *fetch_user_email(void) {
    char *token = google_oauth_get_valid_token(NULL);
    if (!token) return NULL;

    http_t *h = http_new(15);
    if (!h) { free(token); return NULL; }

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
    free(token);

    http_resp_t *resp = http_get(h, GOOGLE_PEOPLE_API, auth);

    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    json_t *root = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);

    if (!root) return NULL;

    /* Parse email from People API response */
    char *email = NULL;
    json_t *email_addrs = json_obj_get(root, "emailAddresses");
    if (email_addrs && email_addrs->type == JSON_ARRAY && email_addrs->c.count > 0) {
        json_t *first = email_addrs->c.items[0];
        if (first && first->type == JSON_OBJECT) {
            const char *val = json_get_str(first, "value", NULL);
            if (val) email = strdup(val);
        }
    }

    json_free(root);
    return email;
}

/* ─── Configuration ────────────────────────────────────────────── */

void google_oauth_set_home(const char *home) {
    if (home) {
        snprintf(g_home, sizeof(g_home), "%s", home);
    } else {
        g_home[0] = '\0';
    }
}

/* Port of Python: _scrape_client_credentials */
void google_oauth_set_client_credentials(const char *client_id, const char *client_secret) {
    if (client_id) {
        snprintf(g_client_id, sizeof(g_client_id), "%s", client_id);
    }
    if (client_secret) {
        snprintf(g_client_secret, sizeof(g_client_secret), "%s", client_secret);
    }
}

/* Port of Python _get_client_id(). */
const char *google_oauth_get_client_id(void) {
    return get_client_id();
}

/* Port of Python _get_client_secret(). */
const char *google_oauth_get_client_secret(void) {
    if (g_client_secret[0]) return g_client_secret;
    const char *env = getenv("GOOGLE_CLIENT_SECRET");
    return env ? env : "";
}

/* Port of Python: exchange_code.
 * Wraps oauth_exchange_code() from token_exchange.c.
 * Uses Google's token endpoint, configured client_id, and PKCE params. */
oauth_token_t *google_oauth_exchange_code(const char *auth_code, const char *code_verifier,
                                           const char *redirect_uri) {
    if (!auth_code || !*auth_code) return NULL;
    if (!code_verifier || !*code_verifier) return NULL;

    const char *client_id = get_client_id();
    if (!client_id || !*client_id) return NULL;

    /* Generate PKCE challenge from verifier */
    char *code_challenge = crypto_pkce_challenge(code_verifier);
    if (!code_challenge) return NULL;

    oauth_token_t *token = oauth_exchange_code(
        "https://oauth2.googleapis.com/token",
        auth_code,
        redirect_uri ? redirect_uri : "http://localhost",
        client_id,
        code_verifier,
        code_challenge,
        30  /* 30 second timeout */
    );

    free(code_challenge);
    return token;
}

/* Port of Python: update_project_ids */
bool update_project_ids(const char *project_id, const char *managed_project_id) {
    if (!project_id && !managed_project_id) return false;

    google_oauth_creds_t *creds = google_oauth_load_credentials();
    if (!creds) return false;

    if (project_id) {
        free(creds->project_id);
        creds->project_id = strdup(project_id);
    }
    if (managed_project_id) {
        free(creds->managed_project_id);
        creds->managed_project_id = strdup(managed_project_id);
    }

    bool ok = save_credentials(creds);
    google_oauth_creds_free(creds);
    return ok;
}

/* Port of Python: _is_headless */
bool is_headless(void) {
    const char *display = getenv("DISPLAY");
    if (display && *display) return false;
    /* Check for Wayland */
    const char *wayland = getenv("WAYLAND_DISPLAY");
    if (wayland && *wayland) return false;
    return true;
}

/* Port of Python: resolve_project_id_from_env */
const char *resolve_project_id_from_env(void) {
    const char *env = getenv("GOOGLE_PROJECT_ID");
    if (env && *env) return env;
    env = getenv("CLOUDSDK_CORE_PROJECT");
    if (env && *env) return env;
    return NULL;
}

/* Port of Python tools/file_state.py:lock_path, tools/file_state.py:lock_path(). */
/* Port of Python: _lock_path */
char *lock_path(void) {
    char path[2048];
    if (build_creds_path(path, sizeof(path)) < 0) return NULL;
    size_t plen = strlen(path);
    char *lock = (char *)malloc(plen + 6); /* ".lock" + NUL */
    if (!lock) return NULL;
    memcpy(lock, path, plen);
    memcpy(lock + plen, ".lock", 6);
    return lock;
}

/* Port of Python: _persist_token_response */
google_oauth_creds_t *google_oauth_persist_token_response(
    const char *access_token,
    const char *refresh_token,
    int expires_in,
    const char *project_id)
{
    if (!access_token || !*access_token || !refresh_token || !*refresh_token)
        return NULL;

    google_oauth_creds_t creds;
    memset(&creds, 0, sizeof(creds));
    creds.access_token = strdup(access_token);
    creds.refresh_token = strdup(refresh_token);
    creds.expires_at = expires_in > 0 ? (double)time(NULL) + (double)(expires_in > 60 ? expires_in : 60) : 0;
    if (project_id && *project_id)
        creds.project_id = strdup(project_id);

    /* Fetch email using the new access token */
    creds.email = fetch_user_email();

    if (!save_credentials(&creds)) {
        free(creds.access_token);
        free(creds.refresh_token);
        free(creds.id_token);
        free(creds.email);
        free(creds.project_id);
        free(creds.managed_project_id);
        return NULL;
    }

    /* Return a heap-allocated copy */
    google_oauth_creds_t *result = (google_oauth_creds_t *)malloc(sizeof(google_oauth_creds_t));
    if (!result) {
        free(creds.access_token);
        free(creds.refresh_token);
        free(creds.id_token);
        free(creds.email);
        free(creds.project_id);
        free(creds.managed_project_id);
        return NULL;
    }
    memcpy(result, &creds, sizeof(creds));

    /* Free moved fields so cleanup doesn't double-free heap copy's pointers */
    memset(&creds, 0, sizeof(creds));
    return result;
}

/* Port of Python: _locate_gemini_cli_oauth_js */
char *locate_gemini_cli_oauth_js(void) {
    /* Check PATH for gemini binary */
    const char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    /* Duplicate PATH so we can walk it */
    char *path_dup = strdup(path_env);
    if (!path_dup) return NULL;

    char *gemini_path = NULL;
    char *save = NULL;
    char *dir = strtok_r(path_dup, ":", &save);
    while (dir) {
        char candidate[4096];
        snprintf(candidate, sizeof(candidate), "%s/gemini", dir);
        struct stat st;
        if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
            gemini_path = strdup(candidate);
            break;
        }
        dir = strtok_r(NULL, ":", &save);
    }
    free(path_dup);

    if (!gemini_path) return NULL;

    /* Walk up from binary to find common oauth2.js paths */
    char resolved[4096];
    {
        char *rp = realpath(gemini_path, NULL);
        if (rp) {
            snprintf(resolved, sizeof(resolved), "%s", rp);
            free(rp);
        } else {
            snprintf(resolved, sizeof(resolved), "%s", gemini_path);
        }
    }
    free(gemini_path);

    /* Try common paths relative to binary parent */
    const char *test_paths[] = {
        "dist/src/code_assist/oauth2.js",
        "dist/code_assist/oauth2.js",
        "src/code_assist/oauth2.js",
        NULL
    };

    char *last_slash = strrchr(resolved, '/');
    if (!last_slash) return NULL;
    *last_slash = '\0';  /* truncate to directory */

    char cur[4096];
    snprintf(cur, sizeof(cur), "%s", resolved);

    for (int depth = 0; depth < 8; depth++) {
        /* Check if node_modules exists at this level */
        char nm_path[4096];
        snprintf(nm_path, sizeof(nm_path), "%s/node_modules/@google/gemini-cli-core", cur);
        struct stat nm_st;
        if (stat(nm_path, &nm_st) == 0 && S_ISDIR(nm_st.st_mode)) {
            /* Try common paths under gemini-cli-core */
            for (int i = 0; test_paths[i]; i++) {
                char tp[4096];
                snprintf(tp, sizeof(tp), "%s/%s", nm_path, test_paths[i]);
                struct stat tp_st;
                if (stat(tp, &tp_st) == 0 && S_ISREG(tp_st.st_mode))
                    return strdup(tp);
            }
        }

        /* Walk up */
        char *parent_slash = strrchr(cur, '/');
        if (!parent_slash || parent_slash == cur) break;
        *parent_slash = '\0';
    }

    return NULL;
}

/* Port of Python: _prompt_paste_fallback */
char *prompt_paste_fallback(void) {
    fprintf(stdout,
        "Unable to open a browser. Please manually paste the authorization code.\n"
        "1. Open the following URL in your browser:\n"
        "   https://accounts.google.com/o/oauth2/auth?... (run with --url to see)\n"
        "2. After authorizing, you will see a code on the web page.\n"
        "3. Paste that code here: ");
    fflush(stdout);

    char *line = NULL;
    size_t n = 0;
    ssize_t len = getline(&line, &n, stdin);
    if (len < 0) {
        free(line);
        return NULL;
    }
    /* Strip trailing newline/whitespace */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '))
        line[--len] = '\0';
    if (len == 0) {
        free(line);
        return NULL;
    }
    return line;
}

/* Port of Python: _generate_pkce_pair.
 * Generate RFC 7636 PKCE (verifier, challenge) pair using S256.
 * Each output is malloc'd; caller must free both via free().
 * Returns true on success. */
bool generate_pkce_pair(char **out_verifier, char **out_challenge) {
    if (!out_verifier || !out_challenge) return false;

    char *verifier = crypto_pkce_verifier();
    if (!verifier) return false;

    char *challenge = crypto_pkce_challenge(verifier);
    if (!challenge) {
        free(verifier);
        return false;
    }

    *out_verifier = verifier;
    *out_challenge = challenge;
    return true;
}

/* Port of Python: _post_form.
 * POST application/x-www-form-urlencoded to url and return parsed JSON response.
 * keys/values arrays are num_pairs long. Returns json_t* (caller json_free)
 * or NULL on error. */
json_t *google_oauth_post_form(const char *url,
                               const char **keys, const char **values, int num_pairs,
                               int timeout_sec) {
    if (!url || !keys || !values || num_pairs <= 0) return NULL;

    /* Build form-urlencoded body: key1=val1&key2=val2&... */
    /* First pass: compute total length */
    size_t total = 0;
    for (int i = 0; i < num_pairs; i++) {
        if (!keys[i] || !values[i]) continue;
        char *ek = http_url_encode(keys[i]);
        char *ev = http_url_encode(values[i]);
        size_t ek_len = ek ? strlen(ek) : 0;
        size_t ev_len = ev ? strlen(ev) : 0;
        total += ek_len + ev_len + 1;  /* key=val plus & or NUL */
        free(ek);
        free(ev);
    }
    if (total == 0) return NULL;

    /* Build the body */
    char *body = (char *)malloc(total + 1);
    if (!body) return NULL;
    size_t pos = 0;
    for (int i = 0; i < num_pairs; i++) {
        if (!keys[i] || !values[i]) continue;
        char *ek = http_url_encode(keys[i]);
        char *ev = http_url_encode(values[i]);
        if (ek && ev) {
            size_t ek_len = strlen(ek);
            size_t ev_len = strlen(ev);
            if (pos > 0) body[pos++] = '&';
            memcpy(body + pos, ek, ek_len);
            pos += ek_len;
            body[pos++] = '=';
            memcpy(body + pos, ev, ev_len);
            pos += ev_len;
        }
        free(ek);
        free(ev);
    }
    body[pos] = '\0';

    /* POST */
    http_t *h = http_new(timeout_sec > 0 ? timeout_sec : 20);
    if (!h) { free(body); return NULL; }

    const char *headers = "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Accept: application/json";
    http_resp_t *resp = http_request(h, HTTP_POST, url, headers, body, pos);
    free(body);

    if (!resp || resp->status < 200 || resp->status >= 300) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    /* Parse response body as JSON */
    json_t *json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    return json;
}

/* Port of Python: _credentials_lock — acquire flock on credentials file */
int credentials_lock(int timeout_seconds) {
    char lock_path[4096];
    const char *home = getenv("HOME");
    if (!home) return -1;
    snprintf(lock_path, sizeof(lock_path), "%s/.hermes/auth/google_oauth.lock", home);
    
    /* Ensure parent dir exists */
    char *slash = strrchr(lock_path, '/');
    if (slash) {
        *slash = '\0';
        mkdir(lock_path, 0700);
        *slash = '/';
    }
    
    int fd = open(lock_path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;
    
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    
    time_t deadline = time(NULL) + (timeout_seconds > 0 ? timeout_seconds : 10);
    while (fcntl(fd, F_SETLK, &fl) != 0) {
        if (time(NULL) >= deadline) {
            close(fd);
            return -1; /* Timeout */
        }
        usleep(50000); /* 50ms */
    }
    return fd; /* Caller must close(fd) to release lock */
}

/* Port of Python agent/google_oauth.py: _bind_callback_server, start_oauth_flow,
 * _paste_mode_login, run_gemini_oauth_login_pure.
 * OAuth flow is handled by google_oauth_start_flow() in C which uses
/* Port of Python agent/google_oauth.py: _bind_callback_server, start_oauth_flow, _paste_mode_login, run_gemini_oauth_login_pure. Consolidated in google_oauth_start_flow(). */
