/*
 * approval_transport.c — Port of hermes_cli/approval_transport.py.
 * See include/approval_transport.h for the contract.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "approval_transport.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/sha.h>

/* ── small helpers ─────────────────────────────────────────────────────── */

static char *xstrdup(const char *s)
{
    if (!s) s = "";
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

static double monotonic_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void msleep(double seconds)
{
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

/* json-escape a string value body (RFC 8259) */
static void json_escape_buf(const char *s, char *out, size_t outsz)
{
    size_t w = 0;
    for (const char *p = s ? s : ""; *p && w + 8 < outsz; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { out[w++] = '\\'; out[w++] = (char)c; }
        else if (c == '\n') { out[w++] = '\\'; out[w++] = 'n'; }
        else if (c == '\t') { out[w++] = '\\'; out[w++] = 't'; }
        else if (c == '\r') { out[w++] = '\\'; out[w++] = 'r'; }
        else if (c < 0x20) { w += (size_t)snprintf(out + w, outsz - w, "\\u%04x", c); }
        else out[w++] = (char)c;
    }
    out[w] = '\0';
}

/* Python repr of a float as used by json.dumps: integral floats keep ".0",
 * non-integral use shortest round-trip (%g with enough digits). */
static void py_double(char *out, size_t outsz, double v)
{
    if (v == (double)(long long)v &&
        v >= -(double)(1LL << 53) && v <= (double)(1LL << 53))
        snprintf(out, outsz, "%.1f", v);
    else
        snprintf(out, outsz, "%.17g", v);
}

static const char *HEX = "0123456789abcdef";
static void sha256_hex(const unsigned char *data, size_t len, char out[65])
{
    unsigned char h[32];
    SHA256(data, len, h);
    for (int i = 0; i < 32; i++) {
        out[i * 2] = HEX[h[i] >> 4];
        out[i * 2 + 1] = HEX[h[i] & 0xf];
    }
    out[64] = '\0';
}

/* ── PoP: uuid.uuid4().hex ─────────────────────────────────────────────── */

char *approval_uuid4_hex(void)
{
    unsigned char b[16];
    if (getentropy(b, sizeof b) != 0) {
        /* fallback: /dev/urandom */
        FILE *f = fopen("/dev/urandom", "rb");
        if (!f || fread(b, 1, sizeof b, f) != sizeof b) {
            if (f) fclose(f);
            return NULL;
        }
        fclose(f);
    }
    b[6] = (unsigned char)((b[6] & 0x0f) | 0x40); /* version 4 */
    b[8] = (unsigned char)((b[8] & 0x3f) | 0x80); /* variant 10xx */
    char *out = malloc(33);
    for (int i = 0; i < 16; i++) {
        out[i * 2] = HEX[b[i] >> 4];
        out[i * 2 + 1] = HEX[b[i] & 0xf];
    }
    out[32] = '\0';
    return out;
}

/* ── bounded worker capacity (threading.BoundedSemaphore(8)) ───────────── */

static pthread_mutex_t g_cap_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_active_workers = 0;

int approval_transport_active_workers(void)
{
    /* mirrors threading.BoundedSemaphore(MAX)._value — free slots */
    pthread_mutex_lock(&g_cap_lock);
    int free_slots = APPROVAL_MAX_ACTIVE_TRANSPORT_WORKERS - g_active_workers;
    pthread_mutex_unlock(&g_cap_lock);
    return free_slots;
}

static int capacity_acquire(void)
{
    pthread_mutex_lock(&g_cap_lock);
    if (g_active_workers >= APPROVAL_MAX_ACTIVE_TRANSPORT_WORKERS) {
        pthread_mutex_unlock(&g_cap_lock);
        return 0;
    }
    g_active_workers++;
    pthread_mutex_unlock(&g_cap_lock);
    return 1;
}
static void capacity_release(void)
{
    pthread_mutex_lock(&g_cap_lock);
    g_active_workers--;
    pthread_mutex_unlock(&g_cap_lock);
}

/* ── canonical form + digest ───────────────────────────────────────────── */

/* PoP: canonical dict in ApprovalRequest.create — json.dumps(canonical,
 * sort_keys=True, separators=(",", ":")). Keys sorted:
 * allowed_choices, command, description, pattern_key, pattern_keys,
 * request_id, schema_version, session_key, surface, timeout_seconds. */
static char *canonical_json(const char *request_id, const char *command,
                            const char *description, const char *pattern_key,
                            const char *const *pattern_keys, size_t n_pk,
                            const char *session_key, const char *surface,
                            double timeout_seconds,
                            const char *const *choices, size_t n_ch)
{
    size_t cap = 4096;
    for (size_t i = 0; i < n_pk; i++) cap += strlen(pattern_keys[i]) * 2 + 8;
    for (size_t i = 0; i < n_ch; i++) cap += strlen(choices[i]) + 8;
    char *s = malloc(cap);
    if (!s) return NULL;
    size_t w = 0;
#define EMIT(...) w += (size_t)snprintf(s + w, cap - w, __VA_ARGS__)
    char esc[2048];
    EMIT("{");
    EMIT("\"allowed_choices\":[");
    for (size_t i = 0; i < n_ch; i++) {
        json_escape_buf(choices[i], esc, sizeof esc);
        EMIT("%s%s", i ? "," : "", esc);
    }
    EMIT("],");
    json_escape_buf(command, esc, sizeof esc);
    EMIT("\"command\":%s,", esc);
    json_escape_buf(description, esc, sizeof esc);
    EMIT("\"description\":%s,", esc);
    json_escape_buf(pattern_key, esc, sizeof esc);
    EMIT("\"pattern_key\":%s,", esc);
    EMIT("\"pattern_keys\":[");
    for (size_t i = 0; i < n_pk; i++) {
        json_escape_buf(pattern_keys[i], esc, sizeof esc);
        EMIT("%s%s", i ? "," : "", esc);
    }
    EMIT("],");
    json_escape_buf(request_id, esc, sizeof esc);
    EMIT("\"request_id\":%s,", esc);
    EMIT("\"schema_version\":1,");
    json_escape_buf(session_key, esc, sizeof esc);
    EMIT("\"session_key\":%s,", esc);
    json_escape_buf(surface, esc, sizeof esc);
    EMIT("\"surface\":%s,", esc);
    char numbuf[64];
    py_double(numbuf, sizeof numbuf, timeout_seconds);
    EMIT("\"timeout_seconds\":%s}", numbuf);
#undef EMIT
    return s;
}

/* ── request lifecycle ─────────────────────────────────────────────────── */

approval_request_t *approval_request_create(
    const char *command, const char *description,
    const char *pattern_key, const char *const *pattern_keys,
    size_t n_pattern_keys, const char *session_key, const char *surface,
    int allow_session, int allow_permanent, double timeout_seconds)
{
    char *request_id = approval_uuid4_hex();
    if (!request_id) return NULL;

    /* choices = ["once"(,"session"(,"always")),"deny"] */
    size_t n_ch = 2 + (allow_session ? 1 : 0) + (allow_permanent ? 1 : 0);
    const char **choices = malloc(n_ch * sizeof(char *));
    size_t k = 0;
    choices[k++] = "once";
    if (allow_session) choices[k++] = "session";
    if (allow_permanent) choices[k++] = "always";
    choices[k++] = "deny";

    char *canon = canonical_json(request_id, command, description, pattern_key,
                                 pattern_keys, n_pattern_keys, session_key,
                                 surface, timeout_seconds, choices, n_ch);
    if (!canon) { free(request_id); free(choices); return NULL; }
    char digest[65];
    sha256_hex((const unsigned char *)canon, strlen(canon), digest);
    free(canon);

    approval_request_t *req = calloc(1, sizeof *req);
    req->schema_version = 1;
    req->request_id = request_id;
    req->digest = xstrdup(digest);
    req->command = xstrdup(command);
    req->description = xstrdup(description);
    req->pattern_key = xstrdup(pattern_key);
    req->n_pattern_keys = n_pattern_keys;
    req->pattern_keys = malloc((n_pattern_keys ? n_pattern_keys : 1) * sizeof(char *));
    for (size_t i = 0; i < n_pattern_keys; i++)
        req->pattern_keys[i] = xstrdup(pattern_keys[i]);
    req->session_key = xstrdup(session_key);
    req->surface = xstrdup(surface);
    req->timeout_seconds = timeout_seconds;
    req->n_allowed_choices = n_ch;
    req->allowed_choices = malloc(n_ch * sizeof(char *));
    for (size_t i = 0; i < n_ch; i++)
        req->allowed_choices[i] = xstrdup(choices[i]);
    free(choices);
    return req;
}

int approval_request_verify_digest(const approval_request_t *req)
{
    if (!req || !req->digest) return 0;
    char *canon = canonical_json(req->request_id, req->command,
                                 req->description, req->pattern_key,
                                 (const char *const *)req->pattern_keys,
                                 req->n_pattern_keys, req->session_key,
                                 req->surface, req->timeout_seconds,
                                 (const char *const *)req->allowed_choices,
                                 req->n_allowed_choices);
    if (!canon) return 0;
    char digest[65];
    sha256_hex((const unsigned char *)canon, strlen(canon), digest);
    free(canon);
    return strcmp(digest, req->digest) == 0;
}

void approval_request_free(approval_request_t *req)
{
    if (!req) return;
    free(req->request_id); free(req->digest); free(req->command);
    free(req->description); free(req->pattern_key); free(req->session_key);
    free(req->surface);
    for (size_t i = 0; i < req->n_pattern_keys; i++) free(req->pattern_keys[i]);
    free(req->pattern_keys);
    for (size_t i = 0; i < req->n_allowed_choices; i++) free(req->allowed_choices[i]);
    free(req->allowed_choices);
    free(req);
}

approval_decision_t *approval_request_respond(const approval_request_t *req,
                                              const char *choice)
{
    if (!req || !choice) return NULL;
    approval_decision_t *d = malloc(sizeof *d);
    d->request_id = xstrdup(req->request_id);
    d->request_digest = xstrdup(req->digest);
    d->choice = xstrdup(choice);
    return d;
}

void approval_decision_free(approval_decision_t *d)
{
    if (!d) return;
    free(d->request_id); free(d->request_digest); free(d->choice);
    free(d);
}

static int choice_allowed(const approval_request_t *req, const char *choice)
{
    for (size_t i = 0; i < req->n_allowed_choices; i++)
        if (strcmp(req->allowed_choices[i], choice) == 0) return 1;
    return 0;
}

/* ── invoke_approval_transport ─────────────────────────────────────────── */

typedef struct at_job {
    approval_present_fn present;
    void *present_ctx;
    const approval_request_t *request;
    /* mailbox (queue.Queue(maxsize=1)) */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int has_result;      /* kind: 0 none, 1 decision, 2 exception, 3 non-decision */
    approval_decision_t *decision;
    int abandoned;       /* host stopped listening — late results discarded */
    int refs;            /* host frame + worker; last one frees */
} at_job_t;

static void job_unref(at_job_t *job)
{
    if (__atomic_sub_fetch(&job->refs, 1, __ATOMIC_ACQ_REL) == 0) {
        pthread_mutex_destroy(&job->lock);
        pthread_cond_destroy(&job->cond);
        free(job);
    }
}

static void at_worker(at_job_t *job)
{
    approval_decision_t *dec = NULL;
    int rc = job->present(job->present_ctx, job->request, &dec);
    pthread_mutex_lock(&job->lock);
    if (!job->has_result && !job->abandoned) { /* put_nowait — maxsize=1 */
        job->has_result = (rc == 0) ? 1 : (rc == 1) ? 3 : 2;
        job->decision = dec;
        pthread_cond_signal(&job->cond);
        pthread_mutex_unlock(&job->lock);
    } else {
        /* mailbox full or abandoned: discard the late value entirely —
         * it can never authorize another request */
        pthread_mutex_unlock(&job->lock);
        if (dec) approval_decision_free(dec);
    }
    capacity_release();
    job_unref(job);
}

approval_transport_result_t invoke_approval_transport(
    approval_present_fn present, void *present_ctx,
    const approval_request_t *request,
    double timeout_seconds, double poll_interval,
    approval_on_poll_fn on_poll, void *on_poll_ctx,
    approval_is_interrupted_fn is_interrupted, void *interrupted_ctx)
{
    approval_transport_result_t r;
    memset(&r, 0, sizeof r);
    strcpy(r.choice, "deny");

    /* _transport_worker_slots.acquire(blocking=False) */
    if (!capacity_acquire()) {
        strcpy(r.failure, "busy");   /* logger.warning: capacity exhausted */
        return r;
    }

    at_job_t *job = calloc(1, sizeof *job);
    job->present = present;
    job->present_ctx = present_ctx;
    job->request = request;
    job->refs = 2; /* host frame + worker */
    pthread_mutex_init(&job->lock, NULL);
    pthread_cond_init(&job->cond, NULL);

    double deadline = monotonic_now() + (timeout_seconds > 0 ? timeout_seconds : 0);

    pthread_t worker;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int started = pthread_create(&worker, &attr, (void *(*)(void *))at_worker, job) == 0;
    pthread_attr_destroy(&attr);
    if (!started) {
        capacity_release();          /* finally: release in the except path */
        job->refs = 1;
        job_unref(job);              /* worker never took its reference */
        strcpy(r.failure, "error");  /* logger.warning: could not start worker */
        return r;
    }

    int done = 0;
    while (!done) {
        if (is_interrupted && is_interrupted(interrupted_ctx)) {
            /* interrupted: leave the worker running detached; it cannot
             * authorize anything once we stop listening */
            strcpy(r.failure, "interrupted");
            break;
        }
        double remaining = deadline - monotonic_now();
        if (remaining <= 0) {
            strcpy(r.failure, "timeout"); /* logger.warning: timed out */
            break;
        }
        double wait_s = poll_interval > 0.001 ? poll_interval : 0.001;
        if (wait_s > remaining) wait_s = remaining;
        pthread_mutex_lock(&job->lock);
        if (!job->has_result) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (time_t)wait_s;
            ts.tv_nsec += (long)((wait_s - (double)(time_t)wait_s) * 1e9);
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
            pthread_cond_timedwait(&job->cond, &job->lock, &ts);
        }
        int got = job->has_result;
        approval_decision_t *dec = job->decision;
        pthread_mutex_unlock(&job->lock);
        if (got) {
            done = 1;
            if (monotonic_now() > deadline) {
                strcpy(r.failure, "timeout"); /* completed_at > deadline */
            } else if (got == 2) {
                strcpy(r.failure, "error");   /* logger.warning: failed */
            } else if (got == 3 || !dec) {
                strcpy(r.failure, "invalid"); /* invalid decision type */
            } else if (strcmp(dec->request_id, request->request_id) != 0 ||
                       strcmp(dec->request_digest, request->digest) != 0) {
                strcpy(r.failure, "stale");   /* stale or mismatched decision */
            } else if (!choice_allowed(request, dec->choice)) {
                strcpy(r.failure, "invalid"); /* disallowed choice */
            } else {
                snprintf(r.choice, sizeof r.choice, "%s", dec->choice);
                r.failure[0] = '\0';
            }
            if (dec) approval_decision_free(dec);
            break;
        }
        if (!got && on_poll)
            on_poll(on_poll_ctx); /* exceptions swallowed like Python's debug log */
    }

    if (!done) {
        /* Timeout/interrupt path: mark the mailbox abandoned so the
         * detached worker's eventual late result is discarded — matching
         * the Python semantics where a late result cannot authorize
         * another request. The worker releases its own slot and its
         * reference; the last reference frees the mailbox. */
        pthread_mutex_lock(&job->lock);
        job->abandoned = 1;
        pthread_mutex_unlock(&job->lock);
    }
    job_unref(job);
    return r;
}
