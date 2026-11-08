/*
 * web_dashboard_http.c - dashboard HTTP request/response helpers.
 *
 * Split from web_dashboard.c (soul.md: self-contained modules). Pure HTTP
 * plumbing: routing predicates, token checks, response writers and static
 * file serving. No file-scope state of its own.
 */
#include "web_dashboard_http.h"
#include "hermes_core_types.h"
#include "hermes_cron.h"
#include "hermes_skills.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
#include "hermes_gateway_lifecycle.h"
#include "hermes_insights.h"
#include "provider_metadata.h"
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


/* Public API paths that do NOT require auth (mirrors Python _PUBLIC_API_PATHS). */
bool is_public_path(const char *path) {
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
bool is_loopback_client(uint32_t client_ip) {
    /* 127.0.0.0/8 */
    return (client_ip & 0xFF000000) == 0x7F000000;
}


/* Generate a session token using UUID v4.
 * Port of Python: secrets.token_urlsafe(32) */
void generate_session_token(void) {
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
bool has_valid_token(const char *headers) {
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
const char *extract_headers(const char *buf) {
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


void send_resp(int fd, int status, const char *st, const char *body,
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


void send_json(int fd, int status, const char *st, const char *body) {
    send_resp(fd, status, st, body, "application/json");
}


static void send_html(int fd, int status, const char *html) {
    send_resp(fd, status, status==200?"OK":"Error", html, "text/html; charset=utf-8");
}


void send_err(int fd, int status, const char *msg) {
    char j[1024];
    snprintf(j, sizeof(j), "{\"error\":\"%s\"}", msg);
    send_json(fd, status, status==401?"Unauthorized":status==404?"Not Found":"Error", j);
}


/* Read a file into malloc'd buffer (caller free). Returns NULL on failure. */
char *read_file_to_buf(const char *path, size_t max_size) {
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


int parse_req(int fd, http_req_t *req, uint32_t client_ip) {
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


void serve_static(int fd, const char *p) {
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
