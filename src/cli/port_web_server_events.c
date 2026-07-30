/*
 * port_web_server_events.c — faithful C11 port of the /api/pub + /api/events
 * + /api/pty support layer in hermes_cli/web_server.py.
 *
 * Real behavior throughout: a real subscriber registry with the Python
 * auto-evict semantics (channel dies when last subscriber leaves AND the
 * publisher has disconnected), real tempfile breadcrumbs, byte-exact WS
 * close-reason clamping.
 */

#include "web_server_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reuse: channel-id validation lives in port_web_server.c. */
#include "hermes_web_server_pure.h"

/* ── _ws_close_reason ───────────────────────────────────────────────────── */
/* PoP: ws_events_close_reason @ hermes_cli/web_server.py:_ws_close_reason */
char *ws_events_close_reason(const char *text) {
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (len <= 123) return strdup(text);
    /* encoded[:120].decode("utf-8","ignore") — drop a torn multibyte tail:
     * back up over any continuation bytes that lost their lead byte. */
    size_t cut = 120;
    while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) {
        /* text[cut] is a continuation byte: the char starting before the
         * boundary is torn only if its lead is < cut; find the lead. */
        size_t lead = cut;
        while (lead > 0 && ((unsigned char)text[lead] & 0xC0) == 0x80) lead--;
        /* how many bytes does the lead promise? */
        unsigned char b = (unsigned char)text[lead];
        size_t need = (b & 0xF8) == 0xF0 ? 4 : (b & 0xF0) == 0xE0 ? 3
                       : (b & 0xE0) == 0xC0 ? 2 : 1;
        if (lead + need <= 120) break;  /* char fits entirely, keep it */
        cut = lead;
        break;
    }
    /* also handle a lead byte sitting right at the boundary with its
     * continuations cut off */
    if (cut > 0) {
        size_t lead = cut - 1;
        while (lead > 0 && ((unsigned char)text[lead] & 0xC0) == 0x80) lead--;
        unsigned char b = (unsigned char)text[lead];
        size_t need = (b & 0xF8) == 0xF0 ? 4 : (b & 0xF0) == 0xE0 ? 3
                       : (b & 0xE0) == 0xC0 ? 2 : 1;
        if (need > 1 && lead + need > cut) cut = lead;
    }
    char *out = malloc(cut + 4);
    memcpy(out, text, cut);
    memcpy(out + cut, "...", 4);
    return out;
}

/* ── _resolve_client_ws_host ────────────────────────────────────────────── */
/* PoP: ws_events_resolve_client_host @ hermes_cli/web_server.py:_resolve_client_ws_host */
char *ws_events_resolve_client_host(const char *bound_host) {
    const char *explicit_host = getenv("HERMES_DASHBOARD_WS_HOST");
    if (explicit_host) {
        /* .strip() */
        while (*explicit_host == ' ' || *explicit_host == '\t') explicit_host++;
        size_t l = strlen(explicit_host);
        while (l && (explicit_host[l-1] == ' ' || explicit_host[l-1] == '\t'))
            l--;
        if (l) {
            char *r = malloc(l + 1);
            memcpy(r, explicit_host, l);
            r[l] = '\0';
            return r;
        }
    }
    if (!bound_host || !*bound_host) return NULL;
    /* _WILDCARD_HOSTS = {"0.0.0.0", "::"} */
    if (strcmp(bound_host, "0.0.0.0") == 0 || strcmp(bound_host, "::") == 0)
        return strdup("127.0.0.1");
    return strdup(bound_host);
}

/* ── URL helpers ────────────────────────────────────────────────────────── */

/* urllib.parse.urlencode for a single k=v (RFC 3986 quote_plus). */
static void urlencode_append(char *dst, size_t cap, const char *s) {
    size_t o = strlen(dst);
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 4 < cap; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '*') {
            dst[o++] = (char)c;
        } else if (c == ' ') {
            dst[o++] = '+';
        } else {
            o += (size_t)snprintf(dst + o, cap - o, "%%%02X", c);
        }
    }
    dst[o] = '\0';
}

static void netloc_for(char *dst, size_t cap, const char *host, int port) {
    /* Python: f"[{host}]:{port}" if ":" in host and not startswith("[") */
    if (strchr(host, ':') && host[0] != '[')
        snprintf(dst, cap, "[%s]:%d", host, port);
    else
        snprintf(dst, cap, "%s:%d", host, port);
}

/* PoP: ws_events_build_gateway_ws_url @ hermes_cli/web_server.py:_build_gateway_ws_url */
char *ws_events_build_gateway_ws_url(const char *bound_host, int bound_port,
                                     bool auth_required,
                                     const char *session_token,
                                     const char *internal_credential) {
    char *host = ws_events_resolve_client_host(bound_host);
    if (!host || bound_port <= 0) { free(host); return NULL; }
    char netloc[300];
    netloc_for(netloc, sizeof(netloc), host, bound_port);
    free(host);

    char qs[1024] = "";
    if (auth_required) {
        strcat(qs, "internal=");
        urlencode_append(qs, sizeof(qs), internal_credential ? internal_credential : "");
    } else {
        strcat(qs, "token=");
        urlencode_append(qs, sizeof(qs), session_token ? session_token : "");
    }
    size_t need = strlen("ws://") + strlen(netloc) + strlen("/api/ws?") +
                  strlen(qs) + 1;
    char *url = malloc(need);
    snprintf(url, need, "ws://%s/api/ws?%s", netloc, qs);
    return url;
}

/* PoP: ws_events_build_sidecar_url @ hermes_cli/web_server.py:_build_sidecar_url */
char *ws_events_build_sidecar_url(const char *bound_host, int bound_port,
                                  bool auth_required,
                                  const char *session_token,
                                  const char *internal_credential,
                                  const char *channel) {
    char *host = ws_events_resolve_client_host(bound_host);
    if (!host || bound_port <= 0) { free(host); return NULL; }
    char netloc[300];
    netloc_for(netloc, sizeof(netloc), host, bound_port);
    free(host);

    char qs[1400] = "";
    if (auth_required) {
        strcat(qs, "internal=");
        urlencode_append(qs, sizeof(qs), internal_credential ? internal_credential : "");
    } else {
        strcat(qs, "token=");
        urlencode_append(qs, sizeof(qs), session_token ? session_token : "");
    }
    strcat(qs, "&channel=");
    urlencode_append(qs, sizeof(qs), channel ? channel : "");

    size_t need = strlen("ws://") + strlen(netloc) + strlen("/api/pub?") +
                  strlen(qs) + 1;
    char *url = malloc(need);
    snprintf(url, need, "ws://%s/api/pub?%s", netloc, qs);
    return url;
}

/* ── channel registry ───────────────────────────────────────────────────── */

typedef struct {
    int id;
    ws_event_send_fn send;
    void *ctx;
} subscriber_t;

typedef struct channel_s {
    char *name;
    subscriber_t *subs;
    size_t n_subs, cap_subs;
    bool publisher_live;
    struct channel_s *next;
} channel_t;

struct ws_event_registry {
    channel_t *head;
    int next_id;
};

ws_event_registry_t *ws_event_registry_new(void) {
    ws_event_registry_t *r = calloc(1, sizeof *r);
    r->next_id = 1;
    return r;
}

static void channel_free(channel_t *c) {
    free(c->name);
    free(c->subs);
    free(c);
}

void ws_event_registry_free(ws_event_registry_t *reg) {
    if (!reg) return;
    channel_t *c = reg->head;
    while (c) { channel_t *n = c->next; channel_free(c); c = n; }
    free(reg);
}

static channel_t *find_channel(ws_event_registry_t *reg, const char *name,
                               bool create) {
    for (channel_t *c = reg->head; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    if (!create) return NULL;
    channel_t *c = calloc(1, sizeof *c);
    c->name = strdup(name);
    c->next = reg->head;
    reg->head = c;
    return c;
}

/* Auto-evict: drop channel when it has no subscribers AND no live publisher
 * (Python: "entries auto-evict when the last subscriber drops AND the
 * publisher has disconnected"). */
static void maybe_evict(ws_event_registry_t *reg, channel_t *c) {
    if (c->n_subs > 0 || c->publisher_live) return;
    channel_t **pp = &reg->head;
    while (*pp && *pp != c) pp = &(*pp)->next;
    if (*pp) { *pp = c->next; channel_free(c); }
}

/* PoP: ws_event_subscribe @ hermes_cli/web_server.py:events_ws */
int ws_event_subscribe(ws_event_registry_t *reg, const char *channel,
                       ws_event_send_fn send, void *sub_ctx) {
    if (!reg || !channel) return -1;
    /* _channel_or_close_code gate */
    char *ok = web_channel_or_close_code(channel);
    if (!ok) return -1;
    free(ok);
    channel_t *c = find_channel(reg, channel, true);
    if (c->n_subs == c->cap_subs) {
        c->cap_subs = c->cap_subs ? c->cap_subs * 2 : 4;
        c->subs = realloc(c->subs, c->cap_subs * sizeof *c->subs);
    }
    int id = reg->next_id++;
    c->subs[c->n_subs++] = (subscriber_t){ id, send, sub_ctx };
    return id;
}

/* PoP: ws_event_unsubscribe @ hermes_cli/web_server.py:events_ws */
void ws_event_unsubscribe(ws_event_registry_t *reg, const char *channel,
                          int sub_id) {
    if (!reg || !channel) return;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return;
    for (size_t i = 0; i < c->n_subs; i++) {
        if (c->subs[i].id == sub_id) {
            memmove(&c->subs[i], &c->subs[i + 1],
                    (c->n_subs - i - 1) * sizeof *c->subs);
            c->n_subs--;
            break;
        }
    }
    maybe_evict(reg, c);
}

/* PoP: ws_event_publisher_connect @ hermes_cli/web_server.py:pub_ws */
void ws_event_publisher_connect(ws_event_registry_t *reg, const char *channel) {
    if (!reg || !channel) return;
    char *ok = web_channel_or_close_code(channel);
    if (!ok) return;
    free(ok);
    channel_t *c = find_channel(reg, channel, true);
    c->publisher_live = true;
}

/* PoP: ws_event_publisher_disconnect @ hermes_cli/web_server.py:pub_ws */
void ws_event_publisher_disconnect(ws_event_registry_t *reg,
                                   const char *channel) {
    if (!reg || !channel) return;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return;
    c->publisher_live = false;
    maybe_evict(reg, c);
}

/* PoP: ws_event_broadcast @ hermes_cli/web_server.py:_broadcast_event */
int ws_event_broadcast(ws_event_registry_t *reg, const char *channel,
                       const char *payload) {
    if (!reg || !channel) return 0;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return 0;
    /* snapshot the list (Python copies under the lock, then sends) */
    int delivered = 0;
    size_t i = 0;
    while (i < c->n_subs) {
        subscriber_t s = c->subs[i];
        bool ok = s.send ? s.send(s.ctx, payload) : false;
        if (ok) {
            delivered++;
            i++;
        } else {
            /* "Subscriber went away mid-send" — drop it (the Python finally
             * clause would remove it on its next iteration). */
            memmove(&c->subs[i], &c->subs[i + 1],
                    (c->n_subs - i - 1) * sizeof *c->subs);
            c->n_subs--;
        }
    }
    maybe_evict(reg, c);
    return delivered;
}

size_t ws_event_channel_count(const ws_event_registry_t *reg) {
    size_t n = 0;
    for (const channel_t *c = reg ? reg->head : NULL; c; c = c->next) n++;
    return n;
}

size_t ws_event_subscriber_count(const ws_event_registry_t *reg,
                                 const char *channel) {
    if (!reg || !channel) return 0;
    for (const channel_t *c = reg->head; c; c = c->next)
        if (strcmp(c->name, channel) == 0) return c->n_subs;
    return 0;
}

/* ── per-channel active-session files ───────────────────────────────────── */

typedef struct sf_entry {
    char *channel;
    char *path;
    struct sf_entry *next;
} sf_entry_t;

struct ws_session_files {
    sf_entry_t *head;
};

ws_session_files_t *ws_session_files_new(void) {
    return calloc(1, sizeof(ws_session_files_t));
}

void ws_session_files_free(ws_session_files_t *sf) {
    if (!sf) return;
    sf_entry_t *e = sf->head;
    while (e) {
        sf_entry_t *n = e->next;
        free(e->channel);
        free(e->path);
        free(e);
        e = n;
    }
    free(sf);
}

/* PoP: ws_session_file_for_channel @ hermes_cli/web_server.py:_active_session_file_for_channel */
char *ws_session_file_for_channel(ws_session_files_t *sf, const char *channel) {
    if (!sf || !channel) return NULL;
    for (sf_entry_t *e = sf->head; e; e = e->next)
        if (strcmp(e->channel, channel) == 0) return strdup(e->path);

    /* tempfile.mkstemp(prefix="hermes-pty-active-", suffix=".json") */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/tmp";
    char tmpl[512];
    snprintf(tmpl, sizeof(tmpl), "%s/hermes-pty-active-XXXXXX.json", tmpdir);
    int fd = mkstemps(tmpl, 5);  /* 5 = strlen(".json") */
    if (fd < 0) return NULL;
    close(fd);

    sf_entry_t *e = calloc(1, sizeof *e);
    e->channel = strdup(channel);
    e->path = strdup(tmpl);
    e->next = sf->head;
    sf->head = e;
    return strdup(tmpl);
}

/* PoP: ws_session_file_forget @ hermes_cli/web_server.py:_forget_active_session_file */
void ws_session_file_forget(ws_session_files_t *sf, const char *channel) {
    if (!sf || !channel) return;
    sf_entry_t **pp = &sf->head;
    while (*pp) {
        if (strcmp((*pp)->channel, channel) == 0) {
            sf_entry_t *e = *pp;
            unlink(e->path);  /* missing_ok=True: ignore errors */
            *pp = e->next;
            free(e->channel);
            free(e->path);
            free(e);
            return;
        }
        pp = &(*pp)->next;
    }
}

/* ── theme CSS escape ───────────────────────────────────────────────────── */
/* PoP: ws_events_theme_css_esc @ hermes_cli/web_server.py:_esc */
char *ws_events_theme_css_esc(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s), extra = 0;
    for (const char *p = s; (p = strstr(p, "</")) != NULL; p += 2) extra++;
    char *out = malloc(len + extra + 1);
    char *o = out;
    for (const char *p = s; *p; ) {
        if (p[0] == '<' && p[1] == '/') {
            *o++ = '<';
            *o++ = '\\';
            *o++ = '/';
            p += 2;
        } else {
            *o++ = *p++;
        }
    }
    *o = '\0';
    return out;
}
