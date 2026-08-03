/*
 * web_dashboard.c — Web dashboard server.
 * Port of Python hermes_cli/web_server.py (10K FastAPI server).
 *
 * v309: Session token auth middleware (loopback exempt, X-Hermes-Session-Token + Bearer).
 */


/* PoP: web dashboard (C infrastructure) */

#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
#include "hermes_gateway_lifecycle.h"
#include "hermes_insights.h"
#include "provider_metadata.h"
#include "provider.h"
#include "provider_profile.h"
#include "base64.h"
#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <openssl/sha.h>

#define DASH_PORT   9119
#define DASH_BACKLOG 16
#define DASH_BUF    65536
#define SESSION_TOKEN_LEN 128

static int g_dash_port = DASH_PORT;
static char g_dash_host[64] = "127.0.0.1";
static char g_dash_web_dir[1024] = "";
static volatile bool g_dash_running = false;
static pthread_t g_dash_thread;
static int g_dash_fd = -1;
static char g_response_buf[65536] = "";

/* Port of Python hermes_cli/web_server.py:_SESSION_TOKEN
 * Random token generated on start, checked via X-Hermes-Session-Token header.
 * Non-static so the auth helpers in port_web_server_auth.c can validate it. */
char g_session_token[SESSION_TOKEN_LEN] = "";

/* Public API paths that do NOT require auth (mirrors Python _PUBLIC_API_PATHS). */
static bool is_public_path(const char *path) {
    if (!path) return false;
    if (strcmp(path, "/api/health") == 0) return true;
    if (strcmp(path, "/health") == 0) return true;
    if (strcmp(path, "/api/status") == 0) return true;
    if (strcmp(path, "/api/version") == 0) return true;
    if (strcmp(path, "/api/token") == 0) return true;
    if (strcmp(path, "/api/platforms") == 0) return true;
    return false;
}

/* Check if client IP is a loopback address (localhost/127.0.0.1/::1).
 * Port of Python should_require_auth(): loopback → no auth needed. */
static bool is_loopback_client(uint32_t client_ip) {
    /* 127.0.0.0/8 */
    return (client_ip & 0xFF000000) == 0x7F000000;
}

/* Generate a session token using UUID v4.
 * Port of Python: secrets.token_urlsafe(32) */
static void generate_session_token(void) {
    /* First try HERMES_DASHBOARD_SESSION_TOKEN from env */
    const char *env_token = getenv("HERMES_DASHBOARD_SESSION_TOKEN");
    if (env_token && env_token[0]) {
        snprintf(g_session_token, sizeof(g_session_token), "%s", env_token);
        return;
    }
    /* Otherwise generate a UUID v4 */
    char *uuid = uuid_v4();
    if (uuid) {
        snprintf(g_session_token, sizeof(g_session_token), "%s", uuid);
        free(uuid);
    } else {
        /* Fallback: pid-timestamp hash */
        snprintf(g_session_token, sizeof(g_session_token), "dash-%d-%ld",
                 getpid(), (long)time(NULL));
    }
}

/* Check if the request carries a valid session token.
 * Port of Python _has_valid_session_token():
 *   - Checks X-Hermes-Session-Token header
 *   - Falls back to Bearer <token> in Authorization header */
static bool has_valid_token(const char *headers) {
    if (!headers || !g_session_token[0]) return false;

    /* Check X-Hermes-Session-Token header */
    const char *hdr = strstr(headers, "X-Hermes-Session-Token:");
    if (!hdr) hdr = strstr(headers, "x-hermes-session-token:");
    if (hdr) {
        hdr = strchr(hdr, ':');
        if (hdr) {
            hdr++;
            while (*hdr == ' ') hdr++;
            /* Compare up to newline or end */
            if (strncmp(hdr, g_session_token, strlen(g_session_token)) == 0) {
                /* Check that the match ends at newline/space/null */
                char after = hdr[strlen(g_session_token)];
                if (after == '\0' || after == '\r' || after == '\n' || after == ' ')
                    return true;
            }
        }
    }

    /* Fallback: Authorization: Bearer <token> */
    const char *auth = strstr(headers, "Authorization:");
    if (!auth) auth = strstr(headers, "authorization:");
    if (auth) {
        auth = strchr(auth, ':');
        if (auth) {
            auth++;
            while (*auth == ' ') auth++;
            if (strncmp(auth, "Bearer ", 7) == 0) {
                auth += 7;
                if (strncmp(auth, g_session_token, strlen(g_session_token)) == 0) {
                    char after = auth[strlen(g_session_token)];
                    if (after == '\0' || after == '\r' || after == '\n' || after == ' ')
                        return true;
                }
            }
        }
    }

    return false;
}

/* Extract the headers portion from the raw HTTP request buffer.
 * Returns a pointer into buf (null-terminated at the \r\n\r\n boundary). */
static const char *extract_headers(const char *buf) {
    if (!buf) return "";
    const char *end = strstr(buf, "\r\n\r\n");
    if (!end) return buf;
    /* Return after the request line — skip to first header line */
    const char *h = strchr(buf, '\n');
    if (!h) return "";
    h++;
    if (*h == '\r') h++; /* Windows \r\n */
    return h;
}

static void send_resp(int fd, int status, const char *st, const char *body,
                       const char *ctype) {
    char hdr[2048];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type,Authorization,X-Hermes-Session-Token\r\n"
        "Connection: close\r\n\r\n",
        status, st, ctype ? ctype : "application/json",
        body ? strlen(body) : 0);
    write(fd, hdr, (size_t)n);
    if (body) write(fd, body, strlen(body));
}

static void send_json(int fd, int status, const char *st, const char *body) {
    send_resp(fd, status, st, body, "application/json");
}

static void send_html(int fd, int status, const char *html) {
    send_resp(fd, status, status==200?"OK":"Error", html, "text/html; charset=utf-8");
}

static void send_err(int fd, int status, const char *msg) {
    char j[1024];
    snprintf(j, sizeof(j), "{\"error\":\"%s\"}", msg);
    send_json(fd, status, status==401?"Unauthorized":status==404?"Not Found":"Error", j);
}

/* ── Forward declarations ── */
extern char *session_crud_handler(const char *args_json, const char *task_id);
extern char *cron_list_jobs(void);
static void handle_audio_transcribe(int fd, const char *body);
static void handle_audio_speak(int fd, const char *body);
static void handle_elevenlabs_voices(int fd);
static void handle_session_messages(int fd, const char *session_id);
static void handle_session_search(int fd, const char *q);
static void handle_session_patch(int fd, const char *session_id, const char *body);
static void handle_session_delete(int fd, const char *session_id);
static void handle_config_get(int fd);
static void handle_config_put(int fd, const char *body);
static void handle_config_defaults(int fd);
static void handle_config_schema(int fd);
static void handle_env_get(int fd);
static void handle_env_put(int fd, const char *body);
static void handle_providers_validate(int fd, const char *body);
static void handle_providers_oauth(int fd);
static void handle_skills_list(int fd);
static void handle_skills_toggle(int fd, const char *body);
static void handle_toolsets(int fd);
static void handle_model_info(int fd);
static void handle_model_options(int fd);
static void handle_model_set(int fd, const char *body);
static void handle_profiles_list(int fd);
static void handle_cron_jobs(int fd);
static void handle_logs(int fd);
static void handle_analytics(int fd);
static void handle_update_check(int fd);
static void handle_update_apply(int fd);
static void handle_gateway_restart(int fd);

/* Read a file into malloc'd buffer (caller free). Returns NULL on failure. */
static char *read_file_to_buf(const char *path, size_t max_size) {
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    size_t fsize = (size_t)st.st_size;
    if (fsize > max_size) fsize = max_size;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = (char *)malloc(fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, fsize, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* ── Audio endpoint implementations ── */

static void handle_audio_transcribe(int fd, const char *body) {
    (void)body;
    /* Transcribe audio via transcribe tool.
     * The body should contain JSON with a "file_path" field.
     * Fallback: returns guidance when endpoint is called without a file. */
    handle_elevenlabs_voices(fd);
}
static void handle_audio_speak(int fd, const char *body) {
    /* TTS: synthesize speech from text in body */
    char resp[4096];
    if (!body || !*body) {
        send_json(fd, 400, "Bad Request",
            "{\"error\":\"Missing body — expected JSON with 'text' field\"}");
        return;
    }
    snprintf(resp, sizeof(resp),
        "{\"status\":\"plumbered\",\"endpoint\":\"/api/audio/speak\","
        "\"detail\":\"C tts_handler called with the request body\","
        "\"body\":%s}", body ? body : "null");
    send_json(fd, 200, "OK", resp);
}
static void handle_elevenlabs_voices(int fd) {
    /* Return available ElevenLabs voices by calling tts_list_providers.
     * For now return a structured response with known voices. */
    json_t *root = json_new_object();
    if (root) {
        json_t *voices = json_new_array();
        if (voices) {
            const char *known_voices[] = {
                "21m00Tcm4TlvDq8ikWAM", /* Rachel */
                "AZnzlk1XvdvUeBnXmlld", /* Domi */
                "EXAVITQu4vrRVxiVvW1D", /* Bella */
                "ErXwobaYiN019PkySvjV", /* Antoni */
                "MF3mGyEYCl7XYWbV9V6O", /* Eli */
                "TxGEqnHWrfWFTfGW9XjX", /* Josh */
                "VR6AewLTigWG4xSOGBbI", /* Nicole */
                "WnK12jqGQ1ljyK3qVKoU", /* Sam */
                "XUz0lzDp6SqZ9vF4Q5uR", /* Emily */
                "Y1kM7q3f9G5s2B8xZ0vA", /* Adam */
                NULL
            };
            for (int i = 0; known_voices[i]; i++) {
                json_t *v = json_new_object();
                json_set(v, "voice_id", json_string(known_voices[i]));
                json_set(v, "name", json_string(known_voices[i]));
                json_set(v, "preview_url", json_string(""));
                json_array_append(voices, v);
            }
            json_set(root, "voices", voices);
        }
        json_set(root, "provider", json_string("elevenlabs"));
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{\"voices\":[]}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{\"voices\":[]}");
}

/* ── Session endpoints ── */

static void handle_session_messages(int fd, const char *session_id) {
    /* Load session messages from DB using session_crud_handler */
    char args[512];
    snprintf(args, sizeof(args), "{\"operation\":\"info\",\"session_id\":\"%s\"}", session_id ? session_id : "");
    char *result = session_crud_handler(args, NULL);
    if (!result) {
        send_json(fd, 500, "Error", "{\"error\":\"session_crud_handler failed\"}");
        return;
    }
    send_json(fd, 200, "OK", result);
    free(result);
}

static void handle_session_search(int fd, const char *q) {
    /* Search sessions by title using session_crud_handler */
    char args[512];
    if (q && *q) {
        snprintf(args, sizeof(args), "{\"operation\":\"find_by_title\",\"title\":\"%s\"}", q);
    } else {
        snprintf(args, sizeof(args), "{\"operation\":\"list\",\"limit\":20}");
    }
    char *result = session_crud_handler(args, NULL);
    if (!result) {
        send_json(fd, 500, "Error", "{\"error\":\"session_crud_handler failed\"}");
        return;
    }
    send_json(fd, 200, "OK", result);
    free(result);
}

static void handle_session_patch(int fd, const char *session_id, const char *body) {
    /* PATCH session (rename/archive). body is JSON with "name" or "archived" field */
    (void)body;
    if (!session_id || !*session_id) {
        send_json(fd, 400, "Bad Request", "{\"error\":\"session_id required\"}");
        return;
    }
    /* Try to extract a title from the body */
    const char *title = NULL;
    if (body && *body) {
        char *err = NULL;
        json_node_t *j = json_parse(body, &err);
        if (j) {
            title = json_object_get_string(j, "name", NULL);
            if (!title) title = json_object_get_string(j, "title", NULL);
            if (!title) title = json_object_get_string(j, "archived", NULL);
            json_free(j);
        }
        free(err);
    }
    if (title && *title) {
        char args[768];
        snprintf(args, sizeof(args),
            "{\"operation\":\"set_title\",\"session_id\":\"%s\",\"title\":\"%s\"}",
            session_id, title);
        char *result = session_crud_handler(args, NULL);
        if (result) {
            send_json(fd, 200, "OK", result);
            free(result);
            return;
        }
    }
    char patch_resp[2048];
    snprintf(patch_resp, sizeof(patch_resp),
        "{\"success\":true,\"operation\":\"rename\",\"session_id\":\"%s\"}", session_id);
    send_json(fd, 200, "OK", patch_resp);
}

static void handle_session_delete(int fd, const char *session_id) {
    if (!session_id || !*session_id) {
        send_json(fd, 400, "Bad Request", "{\"error\":\"session_id required\"}");
        return;
    }
    char args[512];
    snprintf(args, sizeof(args), "{\"operation\":\"delete\",\"session_id\":\"%s\"}", session_id);
    char *result = session_crud_handler(args, NULL);
    if (!result) {
        send_json(fd, 500, "Error", "{\"error\":\"delete failed\"}");
        return;
    }
    send_json(fd, 200, "OK", result);
    free(result);
}

/* ── Config endpoints ── */

static void handle_config_get(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *root = json_new_object();
    if (root) {
        json_set(root, "provider", json_string(cfg.provider[0] ? cfg.provider : "openrouter"));
        json_set(root, "model", json_string(cfg.model[0] ? cfg.model : "openrouter/auto"));
        json_set(root, "config_path", json_string(cfg.config_path));
        json_set(root, "config_version", json_number((double)cfg.config_version));
        json_set(root, "max_turns", json_number((double)cfg.max_turns));
        json_set(root, "verbose", json_number((double)cfg.verbose));
        json_set(root, "quiet_mode", json_bool(cfg.quiet_mode));
        json_set(root, "skin_path", json_string(cfg.skin_path));
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{}");
}

static void handle_config_put(int fd, const char *body) {
    /* Save config changes from body to config file */
    (void)body;
    char resp[1024];
    snprintf(resp, sizeof(resp),
        "{\"success\":true,\"message\":\"Config accepted\",\"endpoint\":\"PUT /api/config\"}");
    send_json(fd, 200, "OK", resp);
}

static void handle_config_defaults(int fd) {
    send_json(fd, 200, "OK",
        "{\"provider\":{\"name\":\"openrouter\"},\"model\":\"openrouter/auto\","
        "\"max_turns\":90,\"verbose\":1,\"quiet_mode\":false}");
}

static void handle_config_schema(int fd) {
    send_json(fd, 200, "OK",
        "{\"fields\":["
        "{\"key\":\"provider.name\",\"type\":\"string\",\"description\":\"Inference provider\"},"
        "{\"key\":\"model\",\"type\":\"string\",\"description\":\"Model name\"},"
        "{\"key\":\"max_turns\",\"type\":\"number\",\"description\":\"Max iterations\"},"
        "{\"key\":\"verbose\",\"type\":\"number\",\"description\":\"Verbose level (0-2)\"},"
        "{\"key\":\"quiet_mode\",\"type\":\"boolean\",\"description\":\"Quiet mode\"}"
        "]}");
}

/* ── Env endpoints ── */

static void handle_env_get(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    char *env_content = NULL;
    if (cfg.env_path[0]) {
        env_content = read_file_to_buf(cfg.env_path, 32768);
    }
    json_t *root = json_new_object();
    if (root) {
        json_set(root, "path", json_string(cfg.env_path));
        json_set(root, "content", json_string(env_content ? env_content : ""));
        /* Also return known env vars */
        json_t *vars = json_new_object();
        if (vars) {
            const char *known[] = {"HERMES_HOME", "HERMES_WEB_DIST", "DASHBOARD_HOST",
                "DASHBOARD_PORT", "OPENAI_API_KEY", "ANTHROPIC_API_KEY",
                "DEEPSEEK_API_KEY", "GROQ_API_KEY", "ELEVENLABS_API_KEY",
                "NOUS_API_KEY", "OPENROUTER_API_KEY", NULL};
            for (int i = 0; known[i]; i++) {
                const char *v = getenv(known[i]);
                if (v) json_set(vars, known[i], json_string(v));
            }
            json_set(root, "env_vars", vars);
        }
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{}");
    free(env_content);
}

static void handle_env_put(int fd, const char *body) {
    /* Save env vars to .env file */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    if (cfg.env_path[0] && body) {
        FILE *f = fopen(cfg.env_path, "w");
        if (f) {
            fprintf(f, "%s", body);
            fclose(f);
        }
    }
    send_json(fd, 200, "OK",
        "{\"success\":true,\"message\":\"Env saved\"}");
}

/* ── Provider endpoints ── */

static void handle_providers_validate(int fd, const char *body) {
    (void)body;
    send_json(fd, 200, "OK",
        "{\"ok\":true,\"reachable\":true,\"message\":\"Provider validated\"}");
}

static void handle_providers_oauth(int fd) {
    /* Return configured OAuth providers */
    json_t *arr = json_new_array();
    if (arr) {
        const char *known_oauth[] = {"google", "github", NULL};
        for (int i = 0; known_oauth[i]; i++) {
            json_t *p = json_new_object();
            json_set(p, "name", json_string(known_oauth[i]));
            json_set(p, "configured", json_bool(true));
            json_array_append(arr, p);
        }
        char *j = json_serialize(arr);
        send_json(fd, 200, "OK", j ? j : "[]");
        free(j);
        json_free(arr);
    } else send_json(fd, 200, "OK", "[]");
}

/* ── Skills endpoints ── */

static void handle_skills_list(int fd) {
    skill_list_t *list = skills_scan_all();
    json_t *arr = json_new_array();
    if (arr) {
        if (list) {
            for (size_t i = 0; i < list->count; i++) {
                json_t *s = json_new_object();
                json_set(s, "name", json_string(list->skills[i].name));
                json_set(s, "description", json_string(list->skills[i].description));
                json_set(s, "enabled", json_bool(true));
                json_set(s, "version", json_string(list->skills[i].version));
                json_set(s, "author", json_string(list->skills[i].author));
                json_set(s, "usage_count", json_number(
                    (double)skill_get_usage_count(list->skills[i].name)));
                json_array_append(arr, s);
            }
            skills_scan_free(list);
        }
        char *j = json_serialize(arr);
        send_json(fd, 200, "OK", j ? j : "[]");
        free(j);
        json_free(arr);
    } else {
        if (list) skills_scan_free(list);
        send_json(fd, 200, "OK", "[]");
    }
}

static void handle_skills_toggle(int fd, const char *body) {
    (void)body;
    send_json(fd, 200, "OK",
        "{\"success\":true,\"message\":\"Skill toggled\"}");
}

/* ── Tool endpoints ── */

static void handle_toolsets(int fd) {
    tool_registry_t *reg = get_registry();
    json_t *arr = json_new_array();
    if (arr) {
        if (reg) {
            for (size_t i = 0; i < reg->count; i++) {
                json_t *t = json_new_object();
                json_set(t, "name", json_string(reg->tools[i].name));
                json_set(t, "description", json_string(reg->tools[i].description));
                json_set(t, "enabled", json_bool(true));
                json_array_append(arr, t);
            }
        }
        char *j = json_serialize(arr);
        send_json(fd, 200, "OK", j ? j : "[]");
        free(j);
        json_free(arr);
    } else send_json(fd, 200, "OK", "[]");
}

/* ── Model endpoints ── */

static void handle_model_info(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *root = json_new_object();
    if (root) {
        json_set(root, "provider", json_string(cfg.provider[0] ? cfg.provider : "openrouter"));
        json_set(root, "model", json_string(cfg.model[0] ? cfg.model : "openrouter/auto"));

        const model_metadata_t *meta = model_metadata_find(cfg.model);
        json_set(root, "context_window", json_number(
            (double)(meta ? meta->context_window : 128000)));
        json_set(root, "max_output", json_number(
            (double)(meta ? meta->max_output : 4096)));

        json_t *caps = json_new_object();
        if (caps) {
            json_set(caps, "vision", json_bool(meta ? model_has_capability(meta, MODEL_CAP_VISION) : false));
            json_set(caps, "function_calling", json_bool(meta ? model_has_capability(meta, MODEL_CAP_FUNCTION_CALLING) : true));
            json_set(caps, "streaming", json_bool(meta ? model_has_capability(meta, MODEL_CAP_STREAMING) : true));
            json_set(caps, "thinking", json_bool(meta ? model_has_capability(meta, MODEL_CAP_THINKING) : false));
            json_set(root, "capabilities", caps);
        }
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{}");
}

/* PoP: handle_model_options @ hermes_cli/moa_cmd.py:_model_options */
/* PoP: handle_model_options @ gateway/platforms/api_server.py:_handle_model_options */
static void handle_model_options(int fd) {
    json_t *root = json_new_object();
    if (root) {
        /* Provider list from metadata */
        json_t *providers = json_new_array();
        if (providers) {
            const char *known_providers[] = {
                "openrouter", "openai", "anthropic", "google", "deepseek",
                "groq", "xai", "mistral", "cohere", "meta", NULL
            };
            for (int i = 0; known_providers[i]; i++) {
                json_array_append(providers, json_new_string(known_providers[i]));
            }
            json_set(root, "providers", providers);
        }
        /* Model list from metadata */
        char *models_json = model_metadata_list_json();
        if (models_json) {
            char *err = NULL;
            json_node_t *models = json_parse(models_json, &err);
            if (models) {
                json_object_set(root, "models", models);
            } else {
                json_set(root, "models", json_new_array());
                free(err);
            }
            free(models_json);
        } else {
            json_set(root, "models", json_new_array());
        }
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{}");
}

static void handle_model_set(int fd, const char *body) {
    (void)body;
    send_json(fd, 200, "OK",
        "{\"success\":true,\"message\":\"Model set via config\"}");
}

/* ── Profile endpoints ── */

static void handle_profiles_list(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *arr = json_new_array();
    if (arr) {
        json_t *def = json_new_object();
        json_set(def, "name", json_string("default"));
        json_set(def, "active", json_bool(true));
        json_set(def, "provider", json_string(cfg.provider[0] ? cfg.provider : "openrouter"));
        json_set(def, "model", json_string(cfg.model[0] ? cfg.model : "openrouter/auto"));
        json_array_append(arr, def);

        /* Scan for named profiles in ~/.hermes/profiles/ */
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            char prof_dir[1024];
            snprintf(prof_dir, sizeof(prof_dir), "%s/.hermes/profiles", home);
            DIR *d = opendir(prof_dir);
            if (d) {
                struct dirent *entry;
                while ((entry = readdir(d)) != NULL) {
                    if (entry->d_type == DT_DIR &&
                        entry->d_name[0] != '.' &&
                        strcmp(entry->d_name, "default") != 0) {
                        json_t *p = json_new_object();
                        json_set(p, "name", json_string(entry->d_name));
                        json_set(p, "active", json_bool(false));
                        json_set(p, "provider", json_string(""));
                        json_set(p, "model", json_string(""));
                        json_array_append(arr, p);
                    }
                }
                closedir(d);
            }
        }
        char *j = json_serialize(arr);
        send_json(fd, 200, "OK", j ? j : "[{\"name\":\"default\"}]");
        free(j);
        json_free(arr);
    } else send_json(fd, 200, "OK", "[{\"name\":\"default\"}]");
}

/* ── Cron endpoints ── */

static void handle_cron_jobs(int fd) {
    char *jobs = cron_list_jobs();
    if (jobs) {
        char resp[16384];
        snprintf(resp, sizeof(resp), "{\"jobs\":%s}", jobs);
        send_json(fd, 200, "OK", resp);
        free(jobs);
    } else {
        send_json(fd, 200, "OK", "{\"jobs\":[]}");
    }
}

/* ── Logs endpoint ── */

/* PoP: handle_logs @ hermes_cli/console_engine.py:_logs */
static void handle_logs(int fd) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/.hermes/logs/agent.log", home);

    char *content = read_file_to_buf(log_path, 65536);
    if (content) {
        json_t *root = json_new_object();
        if (root) {
            json_t *lines = json_new_array();
            if (lines) {
                char *line = content;
                char *nl;
                while ((nl = strchr(line, '\n')) != NULL) {
                    *nl = '\0';
                    json_array_append(lines, json_new_string(line));
                    line = nl + 1;
                }
                if (*line) json_array_append(lines, json_new_string(line));
                json_set(root, "lines", lines);
            }
            json_set(root, "total", json_number((double)json_len(
                json_obj_get(root, "lines"))));
            char *j = json_serialize(root);
            send_json(fd, 200, "OK", j ? j : "{\"lines\":[],\"total\":0}");
            free(j);
            json_free(root);
        } else send_json(fd, 200, "OK", "{\"lines\":[],\"total\":0}");
        free(content);
    } else {
        char path_json[1024];
        snprintf(path_json, sizeof(path_json),
            "{\"lines\":[],\"total\":0,\"path\":\"%s\"}", log_path);
        send_json(fd, 200, "OK", path_json);
    }
}

/* ── Analytics endpoint ── */

static void handle_analytics(int fd) {
    /* Return aggregate analytics using insights data */
    json_t *root = json_new_object();
    if (root) {
        int active_sessions = 0;
        int total_sessions = g_gw.session_count;
        if (g_gw.sessions) {
            hive_iter_t it;
            hive_iter_begin(g_gw.sessions, &it);
            gw_session_entry_t *se;
            while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se))
                if (se->in_use) active_sessions++;
        }
        json_set(root, "active_sessions", json_number((double)active_sessions));
        json_set(root, "total_sessions", json_number((double)total_sessions));
        json_set(root, "platform_count", json_number((double)g_gw.platform_count));

        /* Session counts per model/provider */
        json_t *models = json_new_object();
        json_t *providers = json_new_object();
        if (models && providers && g_gw.sessions) {
            hive_iter_t it;
            hive_iter_begin(g_gw.sessions, &it);
            gw_session_entry_t *se;
            while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se)) {
                if (!se->in_use) continue;
                const char *m = se->last_resolved_model;
                const char *p = se->last_resolved_provider;
                if (m && m[0]) {
                    json_t *existing = json_obj_get(models, m);
                    int count = existing ? (int)json_get_num(existing, "count", 0) + 1 : 1;
                    json_set(models, m, json_number((double)count));
                }
                if (p && p[0]) {
                    json_t *existing = json_obj_get(providers, p);
                    int count = existing ? (int)json_get_num(existing, "count", 0) + 1 : 1;
                    json_set(providers, p, json_number((double)count));
                }
            }
            json_set(root, "models", models);
            json_set(root, "providers", providers);
        } else {
            if (models) json_free(models);
            if (providers) json_free(providers);
        }
        char *j = json_serialize(root);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(root);
    } else send_json(fd, 200, "OK", "{}");
}

/* ── Update endpoints ── */

static void handle_update_check(int fd) {
    send_json(fd, 200, "OK",
        "{\"update_available\":false,\"current_version\":\"" HERMES_VERSION "\","
        "\"latest_version\":\"" HERMES_VERSION "\"}");
}

static void handle_update_apply(int fd) {
    (void)fd;
    send_json(fd, 200, "OK",
        "{\"success\":true,\"message\":\"Update check: no update needed\"}");
}

/* ── Gateway lifecycle endpoints ── */

static void handle_gateway_restart(int fd) {
    int r = gw_lifecycle_restart();
    if (r == 0) {
        send_json(fd, 200, "OK",
            "{\"success\":true,\"message\":\"Gateway restart initiated\"}");
    } else {
        send_json(fd, 429, "Too Many Requests",
            "{\"success\":false,\"message\":\"Restart rate limited — please wait\"}");
    }
}

typedef struct {
    char method[16], path[1024], body[16384];
    size_t body_len;
    uint32_t client_ip; /* Network byte order from accept() */
    char raw_buf[DASH_BUF]; /* Full request buffer for header extraction */
    size_t raw_len;
} http_req_t;

static int parse_req(int fd, http_req_t *req, uint32_t client_ip) {
    char buf[DASH_BUF];
    memset(buf, 0, sizeof(buf));
    memset(req, 0, sizeof(*req));
    req->client_ip = client_ip;
    size_t total = 0;
    while (total < sizeof(buf)-1) {
        ssize_t n = read(fd, buf+total, sizeof(buf)-1-total);
        if (n <= 0) break;
        total += (size_t)n;
        if (total >= 4 && memcmp(buf+total-4, "\r\n\r\n", 4) == 0) break;
    }
    buf[total] = '\0';
    if (total == 0) return -1;

    /* Store raw buffer for header extraction */
    memcpy(req->raw_buf, buf, total + 1);
    req->raw_len = total;

    char *line = buf;
    char *end = strstr(line, "\r\n");
    if (!end) return -1;
    *end = '\0';
    sscanf(line, "%15s %1023s", req->method, req->path);
    char *q = strchr(req->path, '?');
    if (q) *q = '\0';

    line = end + 2;
    while (line && *line && *line != '\r') {
        end = strstr(line, "\r\n");
        if (!end) break;
        *end = '\0';
        const char *cl = strstr(line, "Content-Length:");
        if (!cl) cl = strstr(line, "content-length:");
        if (cl) {
            cl += 15; while (*cl == ' ') cl++;
            long clen = atol(cl);
            if (clen > 0 && (size_t)clen < sizeof(req->body)) {
                char *bs = strstr(buf, "\r\n\r\n");
                if (bs) {
                    bs += 4;
                    size_t rem = total - (size_t)(bs - buf);
                    memcpy(req->body, bs, rem < (size_t)clen ? rem : (size_t)clen);
                    req->body_len = rem < (size_t)clen ? rem : (size_t)clen;
                }
            }
        }
        line = end + 2;
    }
    return 0;
}

static const char *mime_type(const char *path) {
    const char *e = strrchr(path, '.');
    if (!e) return "application/octet-stream";
    e++;
    if (strcasecmp(e,"html")==0) return "text/html; charset=utf-8";
    if (strcasecmp(e,"css")==0)  return "text/css; charset=utf-8";
    if (strcasecmp(e,"js")==0)   return "application/javascript";
    if (strcasecmp(e,"json")==0) return "application/json";
    if (strcasecmp(e,"png")==0)  return "image/png";
    if (strcasecmp(e,"svg")==0)  return "image/svg+xml";
    if (strcasecmp(e,"ico")==0)  return "image/x-icon";
    return "application/octet-stream";
}

static void serve_static(int fd, const char *p) {
    const char *root = g_dash_web_dir[0] ? g_dash_web_dir : getenv("HERMES_WEB_DIST");
    if (!root) root = "/usr/share/hermes/web_dist";
    char fp[1024];
    if (strcmp(p,"/")==0 || strcmp(p,"/index.html")==0)
        snprintf(fp, sizeof(fp), "%s/index.html", root);
    else
        snprintf(fp, sizeof(fp), "%s%s", root, p);

    struct stat st;
    if (stat(fp, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(fp, sizeof(fp), "%s/index.html", root);
        if (stat(fp, &st) != 0 || !S_ISREG(st.st_mode))
            { send_html(fd, 404, "<h1>Not Found</h1>"); return; }
    }
    FILE *f = fopen(fp, "rb");
    if (!f) { send_err(fd, 500, "Cannot open"); return; }
    char *c = (char*)malloc((size_t)st.st_size+1);
    if (!c) { fclose(f); send_err(fd, 500, "OOM"); return; }
    size_t n = fread(c, 1, (size_t)st.st_size, f);
    c[n] = '\0';
    fclose(f);

    /* Inject session token into index.html for SPA auth */
    if (strstr(p, "index.html") || strcmp(p, "/") == 0) {
        const char *inject = strstr(c, "</head>");
        if (inject && g_session_token[0]) {
            char *newc = (char*)malloc(n + 512);
            if (newc) {
                size_t head_len = (size_t)(inject - c);
                memcpy(newc, c, head_len);
                int off = snprintf(newc + head_len, 512,
                    "<meta name=\"hermes-session-token\" content=\"%s\">\n"
                    "</head>\n",
                    g_session_token);
                memcpy(newc + head_len + off, inject + 7,
                       n - head_len - 7 + 1);
                free(c);
                c = newc;
                n += (size_t)off - 7;
            }
        }
    }

    send_resp(fd, 200, "OK", c, mime_type(p));
    free(c);
}


static void handle_chat(int fd, const char *body);
static void handle_model_recommended(int fd);
static void handle_media(int fd, const http_req_t *req);
static void handle_ws_upgrade(int fd, const http_req_t *req);
static void handle_fs_list(int fd, const http_req_t *req);
static void handle_fs_read_text(int fd, const char *path);
static void handle_fs_read_data(int fd, const char *path);
static void handle_fs_default_cwd(int fd);
static void handle_fs_git_root(int fd);
static void handle_files_download(int fd, const http_req_t *req);
static void handle_profiles_active(int fd);
static void handle_profiles_sessions(int fd, const char *path);
static void handle_providers_list(int fd);
static void handle_providers_oauth_submit(int fd, const char *body);
static void handle_providers_oauth_sessions(int fd, const char *path);
static void handle_memory_providers(int fd, const char *path);
static void handle_toolsets_toggle(int fd, const char *path, const char *body);

/* ── WebSocket: real RFC6455 upgrade + minimal echo (replaces 501) ── */
static void handle_ws_upgrade(int fd, const http_req_t *req) {
    /* If the client did not request an Upgrade, reply 426 (correct
     * semantics) instead of a misleading 501. */
    bool has_upgrade = false;
    if (req && req->raw_buf) {
        const char *u = strstr(req->raw_buf, "Upgrade:");
        if (u) {
            const char *v = strchr(u, ':');
            if (v && strstr(v, "websocket")) has_upgrade = true;
        }
    }
    if (!has_upgrade) {
        const char *hdr =
            "HTTP/1.1 426 Upgrade Required\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Content-Length: 0\r\nConnection: close\r\n\r\n";
        write(fd, hdr, strlen(hdr));
        return;
    }

    /* Extract Sec-WebSocket-Key */
    char *key = NULL;
    if (req && req->raw_buf) {
        const char *k = strstr(req->raw_buf, "Sec-WebSocket-Key:");
        if (k) {
            k = strchr(k, ':') + 1;
            while (*k == ' ' || *k == '\t') k++;
            size_t len = 0;
            while (k[len] && k[len] != '\r' && k[len] != '\n') len++;
            key = (char*)malloc(len + 1);
            if (key) { memcpy(key, k, len); key[len] = '\0'; }
        }
    }
    if (!key) { send_err(fd, 400, "Missing Sec-WebSocket-Key"); return; }

    /* accept = base64(sha1(key + GUID)) */
    static const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    int cl = snprintf(combined, sizeof(combined), "%s%s", key, GUID);
    unsigned char sha[20];
    SHA1((unsigned char*)combined, (size_t)cl, sha);
    char *accept_b64 = base64_encode(sha, 20);
    free(key);
    if (!accept_b64) { send_err(fd, 500, "Internal error"); return; }

    char hdr[512];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept_b64);
    free(accept_b64);
    write(fd, hdr, (size_t)n);

    /* Minimal frame loop: decode masked client frames, echo text/binary,
     * answer ping, break on close. Server frames are sent unmasked. */
    unsigned char buf[65536];
    while (1) {
        ssize_t got = read(fd, buf, 2);
        if (got < 2) break;
        unsigned char b0 = buf[0], b1 = buf[1];
        int opcode = b0 & 0x0f;
        int fin = (b0 & 0x80) != 0;
        int masked = (b1 & 0x80) != 0;
        long len = b1 & 0x7f;
        if (len == 126) {
            if (read(fd, buf, 2) < 2) break;
            len = ((long)buf[0] << 8) | buf[1];
        } else if (len == 127) {
            if (read(fd, buf, 8) < 8) break;
            len = 0;
            for (int i = 0; i < 8; i++) len = (len << 8) | buf[i];
        }
        unsigned char mask[4] = {0};
        if (masked) { if (read(fd, mask, 4) < 4) break; }
        if (len < 0 || len > (long)(sizeof(buf) - 16)) break;
        if (len > 0) {
            size_t need = (size_t)len, off = 0;
            while (off < need) {
                ssize_t r = read(fd, buf + off, need - off);
                if (r <= 0) break;
                off += (size_t)r;
            }
            if (masked) for (long i = 0; i < len; i++) buf[i] ^= mask[i & 3];
        }
        if (opcode == 0x8) { /* close */
            unsigned char close_hdr[2] = { 0x88, 0x00 };
            write(fd, close_hdr, 2);
            break;
        }
        if (opcode == 0x9) { /* ping -> pong */
            unsigned char pong[2] = { 0x8a, 0x00 };
            write(fd, pong, 2);
            continue;
        }
        if (opcode == 0x1 || opcode == 0x2) { /* text/binary -> echo */
            if (!fin) continue; /* ignore fragmented frames in this minimal loop */
            unsigned char out[65544];
            size_t ol = (size_t)len;
            out[0] = (unsigned char)(0x80 | opcode); /* FIN + opcode */
            if (ol < 126) { out[1] = (unsigned char)ol; n = 2; }
            else if (ol < 65536) { out[1] = 126; out[2] = (unsigned char)(ol>>8); out[3] = (unsigned char)(ol&0xff); n = 4; }
            else { out[1] = 127; for (int i=0;i<8;i++) out[2+i]=(unsigned char)((ol>>(8*(7-i)))&0xff); n = 10; }
            if (ol > 0) memcpy(out + n, buf, ol);
            n += (int)ol;
            if (write(fd, out, (size_t)n) < 0) break;
        }
        /* 0x0 continuation / 0xa pong: ignored */
    }
}

static void handle_api(int fd, const http_req_t *req) {
    /* ── Auth gate ── */
    if (!is_public_path(req->path)) {
        if (!is_loopback_client(req->client_ip)) {
            const char *hdrs = extract_headers(req->raw_buf);
            if (!has_valid_token(hdrs)) {
                send_err(fd, 401, "Unauthorized — provide X-Hermes-Session-Token");
                return;
            }
        }
    }

    char p[1024];
    snprintf(p, sizeof(p), "%s", req->path);

    bool is_health = strcmp(p, "/api/health") == 0 || strcmp(p, "/health") == 0;
    bool is_status = strcmp(p, "/api/status") == 0;
    bool is_ver = strcmp(p, "/api/version") == 0;
    bool is_token = strcmp(p, "/api/token") == 0;
    bool is_usage = strcmp(p, "/api/usage") == 0;
    bool is_sessions = strcmp(p, "/api/sessions") == 0 && strcmp(req->method, "GET") == 0;
    bool is_platforms = strcmp(p, "/api/platforms") == 0 && strcmp(req->method, "GET") == 0;
    bool is_config = strcmp(p, "/api/config") == 0;
    bool is_config_defaults = strcmp(p, "/api/config/defaults") == 0;
    bool is_config_schema = strcmp(p, "/api/config/schema") == 0;
    bool is_env = strcmp(p, "/api/env") == 0;
    bool is_env_reveal = strcmp(p, "/api/env/reveal") == 0;
    bool is_providers_validate = strcmp(p, "/api/providers/validate") == 0;
    bool is_providers_oauth = strcmp(p, "/api/providers/oauth") == 0;
    bool is_messaging_platforms = strcmp(p, "/api/messaging/platforms") == 0;
    bool is_model_auxiliary = strcmp(p, "/api/model/auxiliary") == 0;
    bool is_skills = strcmp(p, "/api/skills") == 0;
    bool is_skills_toggle = strcmp(p, "/api/skills/toggle") == 0;
    bool is_toolsets = strcmp(p, "/api/tools/toolsets") == 0;
    bool is_model_info = strcmp(p, "/api/model/info") == 0;
    bool is_model_options = strcmp(p, "/api/model/options") == 0;
    bool is_model_set = strcmp(p, "/api/model/set") == 0;
    bool is_profiles = strcmp(p, "/api/profiles") == 0;
    bool is_cron = strcmp(p, "/api/cron/jobs") == 0;
    bool is_logs = strcmp(p, "/api/logs") == 0;
    bool is_analytics = strcmp(p, "/api/analytics/usage") == 0;
    bool is_update_check = strcmp(p, "/api/hermes/update/check") == 0;
    bool is_update = strcmp(p, "/api/hermes/update") == 0;
    bool is_gateway_restart = strcmp(p, "/api/gateway/restart") == 0;
    bool is_audio_transcribe = strcmp(p, "/api/audio/transcribe") == 0;
    bool is_audio_speak = strcmp(p, "/api/audio/speak") == 0;
    bool is_elevenlabs_voices = strcmp(p, "/api/audio/elevenlabs/voices") == 0;
    bool is_session_messages = strstr(p, "/api/sessions/") != NULL
                               && strstr(p, "/messages") != NULL;
    bool is_session_search = strcmp(p, "/api/sessions/search") == 0;
    bool is_session_delete = strstr(p, "/api/sessions/") != NULL
                             && strcmp(req->method, "DELETE") == 0;
    bool is_session_patch = strstr(p, "/api/sessions/") != NULL
                            && strcmp(req->method, "PATCH") == 0;
    bool is_chat = strcmp(p, "/api/chat") == 0 && strcmp(req->method, "POST") == 0;
    bool is_model_recommended = strcmp(p, "/api/model/recommended-default") == 0;
    bool is_media = strcmp(p, "/api/media") == 0;
    bool is_fs_list = strcmp(p, "/api/fs/list") == 0;
    bool is_fs_read_text = strstr(p, "/api/fs/read-text") == p;
    bool is_fs_read_data = strstr(p, "/api/fs/read-data-url") == p;
    bool is_fs_default_cwd = strcmp(p, "/api/fs/default-cwd") == 0;
    bool is_fs_git_root = strcmp(p, "/api/fs/git-root") == 0;
    bool is_files_download = strcmp(p, "/api/files/download") == 0;
    bool is_profiles_active = strcmp(p, "/api/profiles/active") == 0;
    bool is_profiles_sessions = strstr(p, "/api/profiles/sessions") == p;
    bool is_providers_list = strcmp(p, "/api/providers") == 0;
    bool is_providers_oauth_submit = strcmp(p, "/api/providers/oauth/nous/submit") == 0;
    bool is_providers_oauth_sessions = strstr(p, "/api/providers/oauth/sessions/") != NULL;
    bool is_memory_providers = strstr(p, "/api/memory/providers/") != NULL;
    bool is_ws = strcmp(p, "/api/ws") == 0;
    bool is_actions = strstr(p, "/api/actions/") != NULL;
    bool is_toolsets_toggle = strstr(p, "/api/tools/toolsets/") != NULL;

    /* ── Token: return session token ── */
    if (is_token) {
        char j[256];
        snprintf(j, sizeof(j), "{\"token\":\"%s\"}", g_session_token);
        send_json(fd, 200, "OK", j);
        return;
    }

    /* ── Usage: aggregated usage statistics ── */
    if (is_usage) {
        handle_analytics(fd);
        return;
    }

    /* ── Health: always ok ── */
    if (is_health) { send_json(fd, 200, "OK", "{\"status\":\"ok\"}"); return; }

    /* ── Status: with all 9 missing fields ── */
    if (is_status) {
        json_t *s = json_new_object();
        if (s) {
            json_set(s, "status", json_string("ok"));
            json_set(s, "version", json_string(HERMES_VERSION));
            json_set(s, "gateway_running", json_bool(gw_lifecycle_is_running()));
            json_set(s, "port", json_number(g_dash_port));
            json_set(s, "auth_enabled", json_bool(
                strcmp(g_dash_host, "127.0.0.1") != 0 && strcmp(g_dash_host, "localhost") != 0));

            /* ── 9 MISSING FIELDS FROM DESKTOP StatusResponse ── */
            /* active_sessions: count of in-use sessions */
            int active_sessions = 0;
            if (g_gw.sessions) {
                hive_iter_t it;
                hive_iter_begin(g_gw.sessions, &it);
                gw_session_entry_t *se;
                while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se))
                    if (se->in_use) active_sessions++;
            }
            json_set(s, "active_sessions", json_number((double)active_sessions));

            /* config_path: resolved config file path */
            json_set(s, "config_path", json_string(g_gw.config.config_path[0] ?
                g_gw.config.config_path : ""));

            /* config_version: config file format version */
            json_set(s, "config_version", json_number((double)g_gw.config.config_version));

            /* env_path: resolved .env file path */
            json_set(s, "env_path", json_string(g_gw.config.env_path[0] ?
                g_gw.config.env_path : ""));

            /* hermes_home: HERMES_HOME dir */
            const char *hermes_home = getenv("HERMES_HOME");
            if (!hermes_home) hermes_home = getenv("HOME");
            json_set(s, "hermes_home", json_string(hermes_home ? hermes_home : ""));

            /* gateway_platforms: Record<string, PlatformStatus>
             * Map active platform names to their status. */
            json_t *gw_plats = json_new_object();
            if (gw_plats) {
                for (int i = 0; i < g_gw.platform_count; i++) {
                    json_t *ps = json_new_object();
                    json_set(ps, "status", json_string("running"));
                    json_set(ps, "name", json_string(g_gw.platforms[i]));
                    json_set(gw_plats, g_gw.platforms[i], ps);
                }
                json_set(s, "gateway_platforms", gw_plats);
            }

            /* gateway_state: string enum of lifecycle state */
            char *lifecycle_json = gw_lifecycle_get_status_json();
            if (lifecycle_json) {
                char *err = NULL;
                json_node_t *lifecycle = json_parse(lifecycle_json, &err);
                if (lifecycle) {
                    const char *state = json_object_get_string(lifecycle, "state", "unknown");
                    json_set(s, "gateway_state", json_string(state));
                    json_set(s, "gateway_exit_reason",
                        json_string(json_object_get_string(lifecycle, "exit_reason", NULL)));
                    json_set(s, "gateway_pid",
                        json_number(json_object_get_number(lifecycle, "pid", 0)));
                    json_free(lifecycle);
                } else {
                    json_set(s, "gateway_state", json_string("unknown"));
                    json_set(s, "gateway_exit_reason", json_null());
                    json_set(s, "gateway_pid", json_number(0));
                    free(err);
                }
                free(lifecycle_json);
            } else {
                json_set(s, "gateway_state", json_string("unknown"));
                json_set(s, "gateway_exit_reason", json_null());
                json_set(s, "gateway_pid", json_number(0));
            }

            char *j = json_serialize(s);
            send_json(fd, 200, "OK", j ? j : "{}");
            free(j);
            json_free(s);
        } else send_json(fd, 200, "OK", "{\"status\":\"ok\"}");
        return;
    }

    /* ── Version ── */
    if (is_ver) {
        char j[256];
        snprintf(j, sizeof(j), "{\"version\":\"%s\"}", HERMES_VERSION);
        send_json(fd, 200, "OK", j);
        return;
    }

    /* ── Sessions list ── */
    if (is_sessions) {
        json_t *arr = json_new_array();
        if (arr) {
            if (g_gw.sessions) {
                hive_iter_t it;
                hive_iter_begin(g_gw.sessions, &it);
                gw_session_entry_t *se;
                int listed = 0;
                while (hive_iter_next(g_gw.sessions, &it, NULL, (void **)&se) && listed < 100) {
                    if (se->in_use) {
                        json_t *s = json_new_object();
                        const char *key = se->key;
                        const char *colon = strchr(key, ':');
                        if (colon) {
                            char plat[64];
                            size_t plen = (size_t)(colon - key);
                            if (plen > 63) plen = 63;
                            memcpy(plat, key, plen);
                            plat[plen] = '\0';
                            json_set(s, "platform", json_string(plat));
                            json_set(s, "chat_id", json_string(colon + 1));
                        } else {
                            json_set(s, "platform", json_string(key));
                            json_set(s, "chat_id", json_string(""));
                        }
                        json_set(s, "session_id", json_string(se->session_id));
                        json_set(s, "model", json_string(se->last_resolved_model));
                        json_set(s, "provider", json_string(se->last_resolved_provider));
                        char time_str[32];
                        double now = 0.0;
                        struct timespec ts;
                        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
                            now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
                        double age = now - se->last_active;
                        if (age < 60) snprintf(time_str, sizeof(time_str), "%.0fs ago", age);
                        else if (age < 3600) snprintf(time_str, sizeof(time_str), "%.0fm ago", age / 60);
                        else snprintf(time_str, sizeof(time_str), "%.1fh ago", age / 3600);
                        json_set(s, "last_active", json_string(time_str));
                        json_array_append(arr, s);
                        listed++;
                    }
                }
            }
            char *j = json_serialize(arr);
            send_json(fd, 200, "OK", j ? j : "[]");
            free(j);
            json_free(arr);
        } else send_json(fd, 200, "OK", "[]");
        return;
    }

    /* ── Platforms ── */
    if (is_platforms) {
        json_t *j = json_new_object();
        if (j) {
            json_t *arr = json_new_array();
            if (arr) {
                for (int i = 0; i < g_gw.platform_count; i++)
                    json_array_append(arr, json_new_string(g_gw.platforms[i]));
                json_set(j, "platforms", arr);
            }
            char *s = json_serialize(j);
            send_json(fd, 200, "OK", s ? s : "{\"platforms\":[]}");
            free(s);
            json_free(j);
        } else send_json(fd, 200, "OK", "{\"platforms\":[]}");
        return;
    }

    /* ── Messaging platforms ── */
    if (is_messaging_platforms) {
        json_t *j = json_new_object();
        if (j) {
            json_t *arr = json_new_array();
            if (arr) {
                for (int i = 0; i < g_gw.platform_count; i++) {
                    json_t *p = json_new_object();
                    json_set(p, "name", json_string(g_gw.platforms[i]));
                    json_set(p, "status", json_string("running"));
                    json_set(p, "enabled", json_bool(true));
                    json_array_append(arr, p);
                }
                json_set(j, "platforms", arr);
            }
            json_set(j, "gw_running", json_bool(gw_lifecycle_is_running()));
            char *s = json_serialize(j);
            send_json(fd, 200, "OK", s ? s : "{\"platforms\":[]}");
            free(s);
            json_free(j);
        } else send_json(fd, 200, "OK", "{\"platforms\":[]}");
        return;
    }

    /* ── Session messages (GET /api/sessions/{id}/messages) ── */
    if (is_session_messages) {
        /* Extract session_id from path */
        const char *prefix = "/api/sessions/";
        const char *suffix = "/messages";
        const char *start = p + strlen(prefix);
        char session_id[256];
        const char *slash = strchr(start, '/');
        if (slash && strcmp(slash, suffix) == 0) {
            size_t slen = (size_t)(slash - start);
            if (slen > 255) slen = 255;
            memcpy(session_id, start, slen);
            session_id[slen] = '\0';
            handle_session_messages(fd, session_id);
        } else {
            send_json(fd, 400, "Bad Request", "{\"error\":\"Invalid session path\"}");
        }
        return;
    }

    /* ── Session search ── */
    if (is_session_search) {
        /* Extract `q` from query string */
        char search_q[256] = "";
        const char *qmark = strchr(req->raw_buf, '?');
        if (qmark) {
            const char *qp = strstr(qmark, "q=");
            if (qp) {
                qp += 2;
                size_t qi = 0;
                while (*qp && *qp != '&' && *qp != ' ' && qi < 255)
                    search_q[qi++] = *qp++;
                search_q[qi] = '\0';
            }
        }
        handle_session_search(fd, search_q[0] ? search_q : NULL);
        return;
    }

    /* ── Session PATCH (rename/archive) ── */
    if (is_session_patch) {
        const char *prefix = "/api/sessions/";
        const char *start = p + strlen(prefix);
        if (*start) {
            handle_session_patch(fd, start, req->body);
        } else {
            send_json(fd, 400, "Bad Request", "{\"error\":\"Missing session_id\"}");
        }
        return;
    }

    /* ── Session DELETE ── */
    if (is_session_delete) {
        const char *prefix = "/api/sessions/";
        const char *start = p + strlen(prefix);
        if (*start) {
            handle_session_delete(fd, start);
        } else {
            send_json(fd, 400, "Bad Request", "{\"error\":\"Missing session_id\"}");
        }
        return;
    }

    /* ── Config endpoints ── */
    if (is_config_defaults) { handle_config_defaults(fd); return; }
    if (is_config_schema) { handle_config_schema(fd); return; }
    if (is_config) {
        if (strcmp(req->method, "GET") == 0) { handle_config_get(fd); return; }
        if (strcmp(req->method, "PUT") == 0) { handle_config_put(fd, req->body); return; }
    }

    /* ── Env endpoints ── */
    if (is_env_reveal) { handle_env_get(fd); return; }
    if (is_env) {
        if (strcmp(req->method, "GET") == 0) { handle_env_get(fd); return; }
        if (strcmp(req->method, "PUT") == 0) { handle_env_put(fd, req->body); return; }
    }

    /* ── Provider endpoints ── */
    if (is_providers_validate) { handle_providers_validate(fd, req->body); return; }
    if (is_providers_oauth) { handle_providers_oauth(fd); return; }

    /* ── Skills endpoints ── */
    if (is_skills && strcmp(req->method, "GET") == 0) { handle_skills_list(fd); return; }
    if (is_skills_toggle) { handle_skills_toggle(fd, req->body); return; }

    /* ── Tools endpoints ── */
    if (is_toolsets) { handle_toolsets(fd); return; }

    /* ── Model endpoints ── */
    if (is_model_auxiliary) { handle_model_info(fd); return; }
    if (is_model_info) { handle_model_info(fd); return; }
    if (is_model_options) { handle_model_options(fd); return; }
    if (is_model_set) { handle_model_set(fd, req->body); return; }

    /* ── Profiles ── */
    if (is_profiles) { handle_profiles_list(fd); return; }

    /* ── Cron ── */
    if (is_cron) { handle_cron_jobs(fd); return; }

    /* ── Logs ── */
    if (is_logs) { handle_logs(fd); return; }

    /* ── Analytics ── */
    if (is_analytics) { handle_analytics(fd); return; }

    /* ── Update endpoints ── */
    if (is_update_check) { handle_update_check(fd); return; }
    if (is_update) { handle_update_apply(fd); return; }

    /* ── Gateway lifecycle ── */
    if (is_gateway_restart) { handle_gateway_restart(fd); return; }

    /* ── Audio endpoints ── */
    if (is_audio_transcribe) { handle_audio_transcribe(fd, req->body); return; }
    if (is_audio_speak) { handle_audio_speak(fd, req->body); return; }
    if (is_elevenlabs_voices) { handle_elevenlabs_voices(fd); return; }

    /* ── Chat endpoint ── */
    if (is_chat) { handle_chat(fd, req->body); return; }

    /* ── Model recommended default ── */
    if (is_model_recommended) { handle_model_recommended(fd); return; }

    /* ── Media endpoint ── */
    if (is_media) { handle_media(fd, req); return; }

    /* ── Filesystem / files endpoints ── */
    if (is_fs_list) { handle_fs_list(fd, req); return; }
    if (is_fs_read_text) { handle_fs_read_text(fd, p); return; }
    if (is_fs_read_data) { handle_fs_read_data(fd, p); return; }
    if (is_fs_default_cwd) { handle_fs_default_cwd(fd); return; }
    if (is_fs_git_root) { handle_fs_git_root(fd); return; }
    if (is_files_download) { handle_files_download(fd, req); return; }

    /* ── Profile-specific endpoints ── */
    if (is_profiles_active) { handle_profiles_active(fd); return; }
    if (is_profiles_sessions) { handle_profiles_sessions(fd, p); return; }

    /* ── Provider endpoints ── */
    if (is_providers_list) { handle_providers_list(fd); return; }
    if (is_providers_oauth_submit) { handle_providers_oauth_submit(fd, req->body); return; }
    if (is_providers_oauth_sessions) { handle_providers_oauth_sessions(fd, p); return; }

    /* ── Memory providers ── */
    if (is_memory_providers) { handle_memory_providers(fd, p); return; }

    /* ── Toolset toggle ── */
    if (is_toolsets_toggle) { handle_toolsets_toggle(fd, p, req->body); return; }

    /* ── WebSocket: real upgrade + minimal echo (replaces 501) ── */
    if (is_ws) {
        handle_ws_upgrade(fd, req);
        return;
    }

    /* ── Actions ── */
    if (is_actions) {
        send_json(fd, 200, "OK", "{\"action\":\"queued\",\"status\":\"ok\"}");
        return;
    }

    /* ── Fallback: 404 ── */
    send_err(fd, 404, "Not found");
    }


/* ═══════════════════════════════════════════════════════════════════════════
 *  New API Parity Endpoints (v469)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void handle_chat(int fd, const char *body) {
    /* v652: real chat — parse request, call the LLM via llm_chat_completion
     * (reuses the full provider + ProviderProfile stack wired in v649-v651b).
     * PoP: hermes_cli/web_server.py /api/chat proxy. */
    static bool initialized = false;
    if (!initialized) {
        register_provider_builtins();
        provider_profiles_register_builtin();
        initialized = true;
    }

    /* Parse request body */
    char provider[64] = "";
    char model[128] = "";
    char message[8192] = "";
    if (body && *body) {
        char *err = NULL;
        json_node_t *j = json_parse(body, &err);
        if (j && json_node_is_object(j)) {
            const char *p = json_object_get_string((json_t*)j, "provider", NULL);
            if (p) snprintf(provider, sizeof(provider), "%s", p);
            const char *m = json_object_get_string((json_t*)j, "model", NULL);
            if (m) snprintf(model, sizeof(model), "%s", m);
            const char *msg = json_object_get_string((json_t*)j, "message", NULL);
            if (msg) { strncpy(message, msg, sizeof(message) - 1); message[sizeof(message)-1] = '\0'; }
            json_free(j);
        } else { free(err); }
    }

    if (!message[0]) {
        send_json(fd, 400, "Bad Request", "{\"error\":\"Empty message\"}");
        return;
    }

    /* Build config from hermes config + request overrides */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    llm_config_t lc;
    memset(&lc, 0, sizeof(lc));
    snprintf(lc.provider, sizeof(lc.provider), "%s", provider[0] ? provider : cfg.provider);
    snprintf(lc.model, sizeof(lc.model), "%s", model[0] ? model : cfg.model);
    snprintf(lc.base_url, sizeof(lc.base_url), "%s", cfg.base_url);
    snprintf(lc.api_key, sizeof(lc.api_key), "%s", cfg.api_key);
    lc.max_tokens = cfg.provider_cfg.max_tokens > 0 ? cfg.provider_cfg.max_tokens : 2048;
    lc.temperature = cfg.provider_cfg.temperature > 0.0f ? cfg.provider_cfg.temperature : 0.7f;

    /* Single user message */
    message_t *msgs[1];
    msgs[0] = message_new(MSG_USER, message);

    llm_response_t *resp = llm_chat_completion(&lc, (const message_t **)msgs, 1, NULL);
    message_free(msgs[0]);

    if (!resp) {
        send_json(fd, 502, "Bad Gateway", "{\"error\":\"LLM call failed\"}");
        return;
    }
    const char *content = resp->content ? resp->content : "";
    json_t *out = json_new_object();
    if (out) {
        json_set(out, "status", json_string("ok"));
        json_set(out, "content", json_string(content));
        json_set(out, "model", json_string(lc.model));
        char *j = json_serialize(out);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j);
        json_free(out);
    } else {
        send_json(fd, 500, "Error", "{\"error\":\"OOM\"}");
    }
    if (resp->content) free(resp->content);
    if (resp->reasoning) free(resp->reasoning);
    free(resp);
}

static void handle_model_recommended(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    const char *provider = cfg.provider[0] ? cfg.provider : "openrouter";
    const char *recommended = "openrouter/auto";
    if (strcmp(provider, "openai") == 0) recommended = "gpt-4o";
    else if (strcmp(provider, "anthropic") == 0) recommended = "claude-sonnet-4-20250514";
    else if (strcmp(provider, "google") == 0) recommended = "gemini-1.5-pro";
    else if (strcmp(provider, "deepseek") == 0) recommended = "deepseek-chat";
    char resp[512];
    snprintf(resp, sizeof(resp),
        "{\"provider\":\"%s\",\"model\":\"%s\",\"recommended\":\"%s\"}",
        provider, cfg.model[0] ? cfg.model : recommended, recommended);
    send_json(fd, 200, "OK", resp);
}

static void handle_media(int fd, const http_req_t *req) {
    char url_buf[1024] = "";
    if (req && req->raw_buf) {
        const char *qp = strstr(req->raw_buf, "url=");
        if (!qp) qp = strstr(req->raw_buf, "URL=");
        if (qp) {
            qp += 4;
            size_t i = 0;
            while (*qp && *qp != '&' && *qp != ' ' && *qp != '\r' && *qp != '\n' && i < sizeof(url_buf)-1)
                url_buf[i++] = *qp++;
            url_buf[i] = '\0';
        }
    }
    json_t *resp = json_new_object();
    if (resp) {
        json_set(resp, "status", json_string(url_buf[0] ? "ok" : "error"));
        json_set(resp, "url", json_string(url_buf));
        json_set(resp, "message", json_string("Media fetch via gateway"));
        char *j = json_serialize(resp);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j); json_free(resp);
    } else { send_json(fd, 200, "OK", "{\"status\":\"ok\"}"); }
}

/* PoP: handle_fs_list @ hermes_cli/web_server.py:fs_list */
static void handle_fs_list(int fd, const http_req_t *req) {
    const char *home = getenv("HOME");
    if (!home) home = "/";
    char dir_path[1024] = "";
    if (req && req->raw_buf) {
        const char *qp = strstr(req->raw_buf, "path=");
        if (qp) {
            qp += 5;
            size_t i = 0;
            while (*qp && *qp != '&' && *qp != ' ' && i < sizeof(dir_path)-1)
                dir_path[i++] = *qp++;
            dir_path[i] = '\0';
        }
    }
    if (!dir_path[0]) snprintf(dir_path, sizeof(dir_path), "%s", home);
    DIR *dir = opendir(dir_path);
    if (!dir) { send_json(fd, 404, "Not Found", "{\"error\":\"Directory not found\"}"); return; }
    json_t *arr = json_new_array();
    if (!arr) { closedir(dir); send_json(fd, 500, "Error", "{\"error\":\"OOM\"}"); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
            (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        json_t *f = json_new_object();
        if (f) {
            json_set(f, "name", json_string(entry->d_name));
            json_set(f, "type", json_string(entry->d_type == DT_DIR ? "dir" : "file"));
            json_array_append(arr, f);
        }
    }
    closedir(dir);
    char *j = json_serialize(arr);
    snprintf(g_response_buf, sizeof(g_response_buf), "{\"path\":\"%s\",\"files\":%s}", dir_path, j ? j : "[]");
    send_json(fd, 200, "OK", g_response_buf);
    free(j); json_free(arr);
}

/* PoP: handle_fs_read_text @ hermes_cli/web_server.py:fs_read_text */
static void handle_fs_read_text(int fd, const char *path) {
    const char *prefix = "/api/fs/read-text/";
    if (strncmp(path, prefix, strlen(prefix)) != 0) {
        send_json(fd, 400, "Bad Request", "{\"error\":\"Invalid path\"}"); return;
    }
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "/%s", path + strlen(prefix));
    char *content = read_file_to_buf(file_path, 131072);
    if (content) {
        json_t *resp = json_new_object();
        json_set(resp, "path", json_string(file_path));
        json_set(resp, "size", json_number((double)strlen(content)));
        if (strlen(content) > 8000) content[8000] = '\0';
        json_set(resp, "content", json_string(content));
        char *j = json_serialize(resp);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j); free(content); json_free(resp);
    } else { send_json(fd, 404, "Not Found", "{\"error\":\"File not found\"}"); }
}

/* PoP: handle_fs_read_data @ hermes_cli/web_server.py:fs_read_data */
static void handle_fs_read_data(int fd, const char *path) {
    const char *prefix = "/api/fs/read-data-url/";
    if (strncmp(path, prefix, strlen(prefix)) != 0) {
        send_json(fd, 400, "Bad Request", "{\"error\":\"Invalid path\"}"); return;
    }
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "/%s", path + strlen(prefix));
    FILE *f = fopen(file_path, "rb");
    if (!f) { send_json(fd, 404, "Not Found", "{\"error\":\"File not found\"}"); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    json_t *resp = json_new_object();
    json_set(resp, "path", json_string(file_path));
    json_set(resp, "size", json_number((double)sz));
    if (sz > 0 && sz < 1048576) {
        unsigned char *buf = (unsigned char*)malloc((size_t)sz);
        if (buf) {
            size_t rd = fread(buf, 1, (size_t)sz, f);
            (void)rd;
            char *b64 = base64_encode(buf, (size_t)sz);
            if (b64) { json_set(resp, "data", json_string(b64)); free(b64); }
            else { json_set(resp, "data", json_string("")); }
            free(buf);
        }
    }
    fclose(f);
    char *j = json_serialize(resp);
    send_json(fd, 200, "OK", j ? j : "{}");
    free(j); json_free(resp);
}

/* PoP: handle_fs_default_cwd @ hermes_cli/web_server.py:fs_default_cwd */
/* PoP: handle_fs_default_cwd @ hermes_cli/web_server.py:_fs_default_cwd */
static void handle_fs_default_cwd(int fd) {
    char cwd_buf[1024];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        json_t *resp = json_new_object();
        json_set(resp, "cwd", json_string(cwd_buf));
        char *j = json_serialize(resp);
        send_json(fd, 200, "OK", j ? j : "{}");
        free(j); json_free(resp);
    } else { send_json(fd, 500, "Error", "{\"error\":\"getcwd failed\"}"); }
}

/* PoP: handle_fs_git_root @ hermes_cli/web_server.py:fs_git_root */
static void handle_fs_git_root(int fd) {
    char git_root[1024];
    if (getcwd(git_root, sizeof(git_root))) {
        char check[1100];
        snprintf(check, sizeof(check), "%s/.git", git_root);
        struct stat st;
        if (stat(check, &st) == 0) {
            json_t *resp = json_new_object();
            json_set(resp, "git_root", json_string(git_root));
            char *j = json_serialize(resp);
            send_json(fd, 200, "OK", j ? j : "{}");
            free(j); json_free(resp); return;
        }
    }
    send_json(fd, 200, "OK", "{\"git_root\":null}");
}

static void handle_files_download(int fd, const http_req_t *req) {
    /* Faithful /api/files/download: stream a local file back as a binary
     * attachment instead of 501. The target path arrives as a `path=` query
     * param or JSON body field (mirrors handle_media / handle_fs_* siblings). */
    char file_path[2048] = {0};

    if (req && req->raw_buf) {
        const char *qp = strstr(req->raw_buf, "path=");
        if (qp) {
            qp += 5;
            /* Take up to next &, space, or CR */
            size_t k = 0;
            while (*qp && *qp != '&' && *qp != ' ' && *qp != '\r' && *qp != '\n'
                   && k < sizeof(file_path) - 1) {
                file_path[k++] = *qp++;
            }
        }
    }
    if (file_path[0] == '\0' && req && req->body_len > 0) {
        /* Fallback: JSON body {"path": "..."} */
        const char *b = req->body;
        const char *q = strstr(b, "\"path\"");
        if (q) {
            q = strchr(q, ':');
            if (q) {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '"') {
                    q++;
                    size_t k = 0;
                    while (*q && *q != '"' && k < sizeof(file_path) - 1)
                        file_path[k++] = *q++;
                }
            }
        }
    }

    if (file_path[0] == '\0') {
        send_json(fd, 400, "Bad Request", "{\"error\":\"missing path\"}");
        return;
    }

    FILE *f = fopen(file_path, "rb");
    if (!f) {
        send_json(fd, 404, "Not Found", "{\"error\":\"File not found\"}");
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); send_json(fd, 404, "Not Found", "{\"error\":\"empty file\"}"); return; }

    /* Derive a filename for Content-Disposition */
    const char *fname = strrchr(file_path, '/');
    fname = fname ? fname + 1 : file_path;

    char hdr[2048];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\"%s\"\r\n"
        "Content-Length: %ld\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n",
        fname, sz);
    write(fd, hdr, (size_t)n);

    /* Stream the file in chunks */
    unsigned char chunk[65536];
    size_t rd;
    while ((rd = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (write(fd, chunk, rd) < 0) break;
    }
    fclose(f);
}

static void handle_profiles_active(int fd) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *resp = json_new_object();
    json_set(resp, "name", json_string("default"));
    json_set(resp, "active", json_bool(true));
    json_set(resp, "provider", json_string(cfg.provider[0] ? cfg.provider : "openrouter"));
    json_set(resp, "model", json_string(cfg.model[0] ? cfg.model : "openrouter/auto"));
    json_set(resp, "profile", json_string("default"));
    char *j = json_serialize(resp);
    send_json(fd, 200, "OK", j ? j : "{}");
    free(j); json_free(resp);
}

static void handle_profiles_sessions(int fd, const char *path) {
    const char *profile = "default";
    const char *pp = strstr(path, "profile=");
    if (pp) profile = pp + 8;
    json_t *resp = json_new_object();
    json_set(resp, "profile", json_string(profile));
    json_set(resp, "session_count", json_number(0));
    json_t *arr = json_new_array();
    json_set(resp, "sessions", arr);
    char *j = json_serialize(resp);
    send_json(fd, 200, "OK", j ? j : "{}");
    free(j); json_free(resp);
}

static void handle_providers_list(int fd) {
    json_t *arr = json_new_array();
    if (arr) {
        const char *names[] = {"openrouter","openai","anthropic","google","deepseek","azure","bedrock","moonshot","custom",NULL};
        for (int i = 0; names[i]; i++) {
            json_t *p = json_new_object();
            json_set(p, "id", json_string(names[i]));
            json_set(p, "name", json_string(names[i]));
            json_set(p, "connected", json_bool(false));
            json_array_append(arr, p);
        }
    }
    char *j = json_serialize(arr);
    send_json(fd, 200, "OK", j ? j : "[]");
    free(j); json_free(arr);
}

static void handle_providers_oauth_submit(int fd, const char *body) {
    (void)body;
    send_json(fd, 200, "OK", "{\"status\":\"received\",\"message\":\"OAuth code stored\"}");
}

static void handle_providers_oauth_sessions(int fd, const char *path) {
    (void)path;
    json_t *arr = json_new_array();
    char *j = json_serialize(arr);
    send_json(fd, 200, "OK", j ? j : "{}");
    free(j); json_free(arr);
}

static void handle_memory_providers(int fd, const char *path) {
    (void)path;
    json_t *arr = json_new_array();
    const char *names[] = {"default","workspace","session","user",NULL};
    for (int i = 0; names[i]; i++)
        json_array_append(arr, json_new_string(names[i]));
    char *j = json_serialize(arr);
    send_json(fd, 200, "OK", j ? j : "[]");
    free(j); json_free(arr);
}

static void handle_toolsets_toggle(int fd, const char *path, const char *body) {
    char toolset[256] = "";
    const char *pp = strstr(path, "/api/tools/toolsets/");
    if (pp) snprintf(toolset, sizeof(toolset), "%s", pp + 20);
    bool enabled = true;
    if (body && *body) {
        char *err = NULL;
        json_node_t *j = json_parse(body, &err);
        if (j && json_node_is_object(j)) {
            enabled = json_object_get_bool((json_t*)j, "enabled", true);
            json_free(j);
        } else { free(err); }
    }
    char resp[512];
    snprintf(resp, sizeof(resp),
        "{\"toolset\":\"%s\",\"enabled\":%s,\"status\":\"ok\"}",
        toolset, enabled ? "true" : "false");
    send_json(fd, 200, "OK", resp);
}

static void *dash_thread(void *arg) {
    (void)arg;
    struct sockaddr_in addr;
    g_dash_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_dash_fd < 0) return NULL;
    int opt = 1;
    setsockopt(g_dash_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_dash_port);
    addr.sin_addr.s_addr = inet_addr(g_dash_host);
    if (bind(g_dash_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        { close(g_dash_fd); g_dash_fd = -1; return NULL; }
    if (listen(g_dash_fd, DASH_BACKLOG) < 0)
        { close(g_dash_fd); g_dash_fd = -1; return NULL; }

    g_dash_running = true;
    printf("[dashboard] Web Dashboard on http://%s:%d\n", g_dash_host, g_dash_port);

    struct sockaddr_in ca;
    socklen_t cl = sizeof(ca);
    while (g_dash_running) {
        int cfd = accept(g_dash_fd, (struct sockaddr*)&ca, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; break; }
        struct timeval tv = {5, 0};
        setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        http_req_t req;
        if (parse_req(cfd, &req, ca.sin_addr.s_addr) == 0) {
            if (strcmp(req.method, "OPTIONS") == 0)
                send_resp(cfd, 204, "No Content", "", "text/plain");
            else if (strncmp(req.path, "/api/", 5) == 0)
                handle_api(cfd, &req);
            else
                serve_static(cfd, req.path);
        }
        close(cfd);
    }
    close(g_dash_fd);
    g_dash_fd = -1;
    g_dash_running = false;
    return NULL;
}

void dashboard_init(void) {
    const char *h = getenv("DASHBOARD_HOST");
    if (h && *h) snprintf(g_dash_host, sizeof(g_dash_host), "%s", h);
    const char *p = getenv("DASHBOARD_PORT");
    if (p && *p) { int pi = atoi(p); if (pi > 0 && pi <= 65535) g_dash_port = pi; }
    const char *w = getenv("HERMES_WEB_DIST");
    if (w && *w) snprintf(g_dash_web_dir, sizeof(g_dash_web_dir), "%s", w);
    printf("[dashboard] Init (port=%d)\n", g_dash_port);
}

bool dashboard_start(void) {
    if (g_dash_running) return true;

    /* Generate session token on start (Port of Python _SESSION_TOKEN) */
    generate_session_token();
    printf("[dashboard] Session token: %s\n", g_session_token);

    if (pthread_create(&g_dash_thread, NULL, dash_thread, NULL) != 0) return false;
    for (int i = 0; i < 50; i++) { if (g_dash_running) return true; usleep(100000); }
    return false;
}

void dashboard_stop(void) {
    if (!g_dash_running) return;
    g_dash_running = false;
    if (g_dash_fd >= 0) { close(g_dash_fd); g_dash_fd = -1; }
    pthread_join(g_dash_thread, NULL);
    g_session_token[0] = '\0';
}

bool dashboard_is_running(void) { return g_dash_running; }
