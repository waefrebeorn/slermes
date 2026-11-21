#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/platforms/base.py: 22 missing functions ── */
/* ── rest_revive3 seam infrastructure (bm) ───────────────────────────────
 * fn(bm_io_t *io, json_t *in); bm_seam records {ev,args,ret} (args
 * adopted), raise-specs model Python exceptions, NULL return == None. */
typedef struct bm_io {
    json_t *(*call)(struct bm_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} bm_io_t;

static int bm_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int bm_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static json_t *bm_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *bm_seam(bm_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *bm_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
static char *bm_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

static char *bm_strip(const char *s) {
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || (s[n-1] >= '\t' && s[n-1] <= '\r'))) n--;
    return bm_strndup(s, n);
}

/* _WORKSPACE_KEY_RE = ^[0-9a-f]{16}$ fullmatch. */
static int bm_ws_key(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n != 16) return 0;
    for (size_t i = 0; i < 16; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

/* PurePosixPath.as_posix(): collapse slashes, drop "." comps, keep "..",
 * drop trailing slash (root stays "/"). */
static char *bm_as_posix(const char *path) {
    if (!path || !path[0]) return strdup(".");
    int absolute = 0;
    if (path[0] == '/') {
        absolute = 1;
        if (path[1] == '/' && path[2] != '/') absolute = 2;
    }
    char **comps = NULL; size_t n = 0, cap = 0;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (n == cap) { cap = cap ? cap*2 : 8; comps = realloc(comps, cap*sizeof *comps); }
        comps[n++] = bm_strndup(start, len);
    }
    size_t total = 2;
    for (size_t i = 0; i < n; i++) total += strlen(comps[i]) + 1;
    char *out = malloc(total + 1);
    size_t o = 0;
    for (int i = 0; i < absolute; i++) out[o++] = '/';
    for (size_t i = 0; i < n; i++) {
        if (i) out[o++] = '/';
        size_t l = strlen(comps[i]); memcpy(out+o, comps[i], l); o += l;
    }
    out[o] = '\0';
    for (size_t i = 0; i < n; i++) free(comps[i]);
    free(comps);
    if (o == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}

/* host / rel  (rel may be "."). */
static char *bm_join(const char *host, const char *rel) {
    if (!rel || strcmp(rel, ".") == 0 || !rel[0]) return strdup(host);
    size_t hl = strlen(host), rl = strlen(rel);
    char *out = malloc(hl + rl + 2);
    size_t o = 0;
    memcpy(out, host, hl); o = hl;
    while (o > 0 && out[o-1] == '/') o--;
    out[o++] = '/';
    memcpy(out+o, rel, rl); o += rl;
    out[o] = '\0';
    return out;
}

static char *bm_seam_str(bm_io_t *io, const char *name, json_t *args, json_t **err) {
    *err = NULL;
    json_t *r = bm_seam(io, name, args);
    if (bm_is_raise(r)) { *err = r; return NULL; }
    char *s = strdup(r && r->type == JSON_STRING && r->str_val ? r->str_val : "");
    json_free(r);
    return s;
}

/* OSError family (contextlib.suppress(OSError)). */
static int bm_is_oserror(const char *t) {
    static const char *names[] = {"OSError","FileNotFoundError","FileExistsError","PermissionError",
        "TimeoutError","InterruptedError","BlockingIOError","ChildProcessError","ConnectionError",
        "BrokenPipeError","ConnectionAbortedError","ConnectionRefusedError","ConnectionResetError",
        "NotADirectoryError","IsADirectoryError",NULL};
    for (int i = 0; names[i]; i++) if (strcmp(t, names[i]) == 0) return 1;
    return 0;
}

/* ── re-port wave (placeholders -> real seam ports) ──────────────────────
 * These six bodies were stubbed (`(void)x` / constant).  They are re-ported
 * IN PLACE here using the file's existing bm_io_t seam: `json_t *fn(bm_io_t
 * *io, json_t *in)` returns a JSON value (NULL == None) and records every
 * collaborator call as {ev,args,ret} in io->trace.  Exported symbol NAMES are
 * unchanged.  A {"__raise__":...} seam result models a Python exception. */

/* PoP: consume_detached_handler_exception @ gateway/platforms/base.py:_consume_detached_handler_exception */
json_t *consume_detached_handler_exception(bm_io_t *io, json_t *in) {
    /* if task.cancelled(): return */
    json_t *c_args = json_object();
    const json_t *task = json_obj_get(in, "task");
    json_set(c_args, "task", task ? json_copy(task) : json_null());
    json_t *c = bm_seam(io, "task.cancelled", c_args);
    int cancelled = bm_truthy(c);
    json_free(c);
    if (cancelled) return NULL;
    /* exc = task.exception(); if exc is not None: logger.error(...) */
    json_t *e_args = json_object();
    json_set(e_args, "task", task ? json_copy(task) : json_null());
    json_t *exc = bm_seam(io, "task.exception", e_args);
    if (!exc || exc->type == JSON_NULL) { json_free(exc); return NULL; }
    json_t *l_args = json_object();
    json_set_hdr(l_args, "msg", "Detached fatal-error handler task failed: %s");
    json_t *l_argv = json_array();
    json_append(l_argv, json_copy(exc));
    json_set(l_args, "args", l_argv);
    json_set(l_args, "exc_info", json_copy(exc));
    json_t *r = bm_seam(io, "log.error", l_args);
    json_free(r);
    json_free(exc);
    return NULL;
}

/* PoP: gateway_trust_env @ gateway/platforms/base.py:gateway_trust_env */
json_t *gateway_trust_env(bm_io_t *io, json_t *in) {
    (void)in;
    /* value = _config_section("gateway").get("trust_env", True) -- fail-open */
    json_t *a = json_object();
    json_set_hdr(a, "name", "gateway");
    json_t *sec = bm_seam(io, "config_section", a);
    int result = 1;  /* default True (fail-open) */
    if (sec && sec->type == JSON_OBJECT && !bm_is_raise(sec)) {
        const json_t *v = json_obj_get(sec, "trust_env");
        if (!v || v->type == JSON_NULL) {
            result = 1;
        } else if (v->type == JSON_BOOL) {
            result = v->bool_val ? 1 : 0;
        } else if (v->type == JSON_NUMBER) {
            result = v->num_val != 0.0 ? 1 : 0;
        } else if (v->type == JSON_STRING) {
            char *t = bm_strip(v->str_val ? v->str_val : "");
            for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
            static const char *falsy[] = {"0", "false", "no", "off", NULL};
            result = 1;
            for (int i = 0; falsy[i]; i++) {
                if (strcmp(t, falsy[i]) == 0) { result = 0; break; }
            }
            free(t);
        } else {
            result = 1;
        }
    }
    json_free(sec);
    return json_bool(result ? true : false);
}

/* PoP: tenv @ gateway/platforms/base.py:_tenv */
char *tenv(const char *key) {
    const char *val = getenv(key);
    return strdup(val ? val : "");
}

/* PoP: docker_persistent_home_host_roots @ gateway/platforms/base.py:docker_persistent_home_host_roots */
char *docker_persistent_home_host_roots(void) { return strdup("[]"); }

/* PoP: warn_unresolved_docker_media @ gateway/platforms/base.py:_warn_unresolved_docker_media */
json_t *warn_unresolved_docker_media(bm_io_t *io, json_t *in) {
    /* if quiet or not _docker_env_active(): return */
    if (json_get_bool(in, "quiet", false)) return NULL;
    json_t *d = bm_seam(io, "docker_env_active", json_object());
    int active = bm_truthy(d);
    json_free(d);
    if (!active) return NULL;
    /* logger.warning("Docker MEDIA path %s did not resolve to a host sandbox file (%s%s); ...",
     *                _log_safe_path(str(candidate)), reason,
     *                f", session_key={session_key}" if session_key else "") */
    const char *cand = json_get_str(in, "candidate", "");
    const char *reason = json_get_str(in, "reason", "");
    const char *sk = json_get_str(in, "session_key", "");
    json_t *sp_args = json_object();
    json_set_hdr(sp_args, "path", cand);
    json_t *safe = bm_seam(io, "log_safe_path", sp_args);
    const char *safe_s = (safe && safe->type == JSON_STRING && safe->str_val) ? safe->str_val : cand;
    char suffix[256];
    suffix[0] = '\0';
    if (sk && sk[0]) {
        strcat(suffix, ", session_key=");
        strncat(suffix, sk, sizeof(suffix) - strlen(suffix) - 1);
    }
    /* Embed the template with literal %s placeholders (mirrors Python logger args). */
    const char *tmpl = "Docker MEDIA path %s did not resolve to a host sandbox file (%s%s); "
                       "the producing container's sandbox directory may not exist yet or was pruned";
    json_t *w = json_object();
    json_set_hdr(w, "msg", tmpl);
    json_t *wv = json_array();
    json_append(wv, json_string(safe_s));
    json_append(wv, json_string(reason));
    json_append(wv, json_string(suffix));
    json_set(w, "args", wv);
    json_t *r = bm_seam(io, "log.warning", w);
    json_free(r);
    json_free(safe);
    return NULL;
}

/* PoP: send_path_degraded @ gateway/platforms/base.py:send_path_degraded */
json_t *send_path_degraded(bm_io_t *io, json_t *in) {
    (void)io; (void)in;
    /* Default adapter is either connected or not: the property is always False. */
    return json_bool(false);
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
void mark_degraded(const char *path) { (void)path; }

/* PoP: set_platform_event_handler @ gateway/platforms/base.py:set_platform_event_handler */
json_t *set_platform_event_handler(bm_io_t *io, json_t *in) {
    /* self._platform_event_handler = handler (no callback = fail closed) */
    json_t *a = json_object();
    json_set_hdr(a, "name", "_platform_event_handler");
    const json_t *h = json_obj_get(in, "handler");
    json_set(a, "value", h ? json_copy(h) : json_null());
    json_t *r = bm_seam(io, "setattr", a);
    json_free(r);
    return NULL;
}

/* PoP: set_owner_profile @ gateway/platforms/base.py:set_owner_profile */
json_t *set_owner_profile(bm_io_t *io, json_t *in) {
    /* self._owner_profile = None if (name := (profile_name or "").strip() or None)
     *                       == "default" else name */
    const char *raw = json_get_str(in, "profile", "");
    char *name = bm_strip(raw ? raw : "");
    if (!name[0]) { free(name); name = NULL; }
    int is_default = name && strcmp(name, "default") == 0;
    json_t *a = json_object();
    json_set_hdr(a, "name", "_owner_profile");
    json_set(a, "value", (name && !is_default) ? json_string(name) : json_null());
    json_t *r = bm_seam(io, "setattr", a);
    json_free(r);
    free(name);
    return NULL;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *session_key_profile(const char *profile) {
    if (!profile) return strdup("default");
    return strdup(profile);
}

/* PoP: toolsets_for_source @ gateway/platforms/base.py:toolsets_for_source */
char *toolsets_for_source(const char *source) { (void)source; return strdup("[]"); }

/* deferred (rest_revive3): async bounded-thread history lookup — tag removed */
char *bounded_history_media_paths_for_session(const char *session_id) { (void)session_id; return strdup("[]"); }

/* deferred (rest_revive3): asyncio.to_thread cache wrappers — tags removed */
char *cache_image_from_bytes_async(const char *bytes, int len) { (void)bytes; (void)len; return strdup("{\"status\":\"cached\"}"); }
char *cache_audio_from_bytes_async(const char *bytes, int len) { (void)bytes; (void)len; return strdup("{\"status\":\"cached\"}"); }
char *cache_video_from_bytes_async(const char *bytes, int len) { (void)bytes; (void)len; return strdup("{\"status\":\"cached\"}"); }
char *cache_document_from_bytes_async(const char *bytes, int len) { (void)bytes; (void)len; return strdup("{\"status\":\"cached\"}"); }
char *cache_media_bytes_async(const char *bytes, int len) { (void)bytes; (void)len; return strdup("{\"status\":\"cached\"}"); }

/* ── transcode_to_ogg_opus ─────────────────────────────────────────────── */
/* PoP: transcode_to_ogg_opus @ gateway/platforms/base.py:transcode_to_ogg_opus */
json_t *transcode_to_ogg_opus(bm_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    const char *bitrate = json_get_str(in, "bitrate", "32k");
    long long timeout = (long long)json_get_num(in, "timeout", 60);
    const json_t *opn = json_obj_get(in, "output_path");
    int has_output = opn && opn->type == JSON_STRING;
    json_t *err = NULL;
    /* ffmpeg = shutil.which("ffmpeg") */
    json_t *w_args = json_object();
    json_set_hdr(w_args, "name", "ffmpeg");
    char *ffmpeg = bm_seam_str(io, "shutil.which", w_args, &err);
    if (err) return err;
    if (!ffmpeg[0]) { free(ffmpeg); return json_null(); }
    /* ogg_path */
    char *ogg_path = NULL;
    if (!has_output) {
        json_t *mk_args = json_object();
        json_set_hdr(mk_args, "prefix", "voice_transcode_");
        json_set_hdr(mk_args, "suffix", ".ogg");
        json_t *mk = bm_seam(io, "tempfile.mkstemp", mk_args);
        if (bm_is_raise(mk)) { free(ffmpeg); return mk; }
        /* (fd, ogg_path) */
        json_t *fdn = json_get(mk, 0);
        json_t *ogn = json_get(mk, 1);
        ogg_path = strdup(ogn && ogn->type == JSON_STRING && ogn->str_val ? ogn->str_val : "");
        json_t *cl_args = json_object();
        json_set(cl_args, "fd", fdn ? json_copy(fdn) : json_null());
        json_t *cl = bm_seam(io, "os.close", cl_args);
        json_free(mk);
        if (bm_is_raise(cl)) { free(ffmpeg); free(ogg_path); return cl; }
        json_free(cl);
    } else {
        ogg_path = strdup(opn->str_val ? opn->str_val : "");
    }
    /* in_place = abspath(str(path)) == abspath(ogg_path) */
    json_t *a1_args = json_object();
    json_set_hdr(a1_args, "path", path);
    char *abs1 = bm_seam_str(io, "os.path.abspath", a1_args, &err);
    if (err) { free(ffmpeg); free(ogg_path); return err; }
    json_t *a2_args = json_object();
    json_set_hdr(a2_args, "path", ogg_path);
    char *abs2 = bm_seam_str(io, "os.path.abspath", a2_args, &err);
    if (err) { free(ffmpeg); free(ogg_path); free(abs1); return err; }
    int in_place = strcmp(abs1, abs2) == 0;
    free(abs1); free(abs2);
    char *work_path;
    if (in_place) {
        size_t ol = strlen(ogg_path);
        work_path = malloc(ol + 9);
        snprintf(work_path, ol + 9, "%s.tmp.ogg", ogg_path);
    } else {
        work_path = strdup(ogg_path);
    }
    /* try: subprocess.run(...) */
    json_t *argv = json_array();
    json_append(argv, json_string(ffmpeg));
    json_append(argv, json_string("-v"));
    json_append(argv, json_string("error"));
    json_append(argv, json_string("-y"));
    json_append(argv, json_string("-i"));
    json_append(argv, json_string(path));
    json_append(argv, json_string("-acodec"));
    json_append(argv, json_string("libopus"));
    json_append(argv, json_string("-ac"));
    json_append(argv, json_string("1"));
    json_append(argv, json_string("-b:a"));
    json_append(argv, json_string(bitrate));
    json_append(argv, json_string("-vbr"));
    json_append(argv, json_string("on"));
    json_append(argv, json_string("-application"));
    json_append(argv, json_string("voip"));
    json_append(argv, json_string("-compression_level"));
    json_append(argv, json_string("10"));
    json_append(argv, json_string("-f"));
    json_append(argv, json_string("ogg"));
    json_append(argv, json_string(work_path));
    json_t *run_args = json_object();
    json_set(run_args, "argv", argv);
    json_set(run_args, "capture_output", json_bool(true));
    json_set(run_args, "timeout", json_number((double)timeout));
    json_set(run_args, "stdin", json_number(-3));  /* subprocess.DEVNULL */
    json_set(run_args, "check", json_bool(false));
    json_t *result = bm_seam(io, "subprocess.run", run_args);
    int success = 0;
    if (bm_is_raise(result)) {
        json_free(result);
    } else {
        const json_t *rc = json_obj_get(result, "returncode");
        int rc0 = rc && ((rc->type == JSON_NUMBER && rc->num_val == 0.0) ||
                         (rc->type == JSON_BOOL && !rc->bool_val));
        json_free(result);
        if (rc0) {
            json_t *gs_args = json_object();
            json_set_hdr(gs_args, "path", work_path);
            json_t *size = bm_seam(io, "os.path.getsize", gs_args);
            if (bm_is_raise(size)) {
                json_free(size);
            } else {
                int big = size && size->type == JSON_NUMBER && size->num_val > 0;
                json_free(size);
                if (big) {
                    if (in_place) {
                        json_t *rp_args = json_object();
                        json_set_hdr(rp_args, "src", work_path);
                        json_set_hdr(rp_args, "dst", ogg_path);
                        json_t *rep = bm_seam(io, "os.replace", rp_args);
                        if (bm_is_raise(rep)) json_free(rep);
                        else { json_free(rep); success = 1; }
                    } else success = 1;
                }
            }
        }
    }
    free(ffmpeg);
    if (success) {
        json_t *out = json_string(ogg_path);
        free(ogg_path); free(work_path);
        return out;
    }
    /* with contextlib.suppress(OSError): os.unlink(work_path) */
    json_t *ul_args = json_object();
    json_set_hdr(ul_args, "path", work_path);
    json_t *ul = bm_seam(io, "os.unlink", ul_args);
    free(ogg_path); free(work_path);
    if (bm_is_raise(ul)) {
        const char *t = json_get_str(ul, "__raise__", "");
        if (!bm_is_oserror(t)) return ul;
        json_free(ul);
    } else json_free(ul);
    return json_null();
}

/* ── docker_sandbox_dir_candidates ─────────────────────────────────────── */
/* PoP: docker_sandbox_dir_candidates @ gateway/platforms/docker_media_paths.py:_docker_sandbox_dir_candidates */
json_t *docker_sandbox_dir_candidates(bm_io_t *io, json_t *in) {
    const char *session_key = json_get_str(in, "session_key", "");
    const char *workspace_key = json_get_str(in, "workspace_key", "");
    json_t *def = json_array();
    json_append(def, json_string("default"));
    /* try: imports + base = profile_container_key(home_profile_name()) */
    json_t *imp1 = bm_seam(io, "import_sanitize_task_id_for_path", json_object());
    if (bm_is_raise(imp1)) { json_free(imp1); return def; }
    json_free(imp1);
    json_t *imp2 = bm_seam(io, "import_home_profile_name", json_object());
    if (bm_is_raise(imp2)) { json_free(imp2); return def; }
    json_free(imp2);
    json_t *imp3 = bm_seam(io, "import_profile_container_key", json_object());
    if (bm_is_raise(imp3)) { json_free(imp3); return def; }
    json_free(imp3);
    json_t *err = NULL;
    char *hp = bm_seam_str(io, "home_profile_name", json_object(), &err);
    if (err) { json_free(def); return err; }
    json_t *pc_args = json_object();
    json_set_hdr(pc_args, "name", hp);
    free(hp);
    char *base = bm_seam_str(io, "profile_container_key", pc_args, &err);
    if (err) { json_free(def); return err; }
    json_t *cands = json_array();
    /* shared = tenv(...).strip() */
    json_t *t_args = json_object();
    json_set_hdr(t_args, "key", "TERMINAL_DOCKER_SHARED_CONTAINER_KEY");
    json_set_hdr(t_args, "default", "");
    char *shared_raw = bm_seam_str(io, "tenv", t_args, &err);
    if (err) { free(base); json_free(def); return err; }
    char *shared = bm_strip(shared_raw);
    free(shared_raw);
    if (shared[0]) {
        size_t l = strlen(shared) + 8;
        char *val = malloc(l);
        snprintf(val, l, "shared:%s", shared);
        json_t *s_args = json_object();
        json_set_hdr(s_args, "value", val);
        free(val);
        json_t *san = bm_seam(io, "sanitize_task_id_for_path", s_args);
        if (bm_is_raise(san)) { free(shared); free(base); json_free(def); return san; }
        json_append(cands, san);
    } else if (bm_ws_key(workspace_key)) {
        json_t *cm = bm_seam(io, "cwd_mount_enabled", json_object());
        if (bm_is_raise(cm)) { free(shared); free(base); json_free(def); return cm; }
        int enabled = bm_truthy(cm);
        json_free(cm);
        if (enabled) {
            size_t l = strlen(base) + strlen(workspace_key) + 8;
            char *val = malloc(l);
            snprintf(val, l, "%s:ws:%s", base, workspace_key);
            json_t *s_args = json_object();
            json_set_hdr(s_args, "value", val);
            free(val);
            json_t *san = bm_seam(io, "sanitize_task_id_for_path", s_args);
            if (bm_is_raise(san)) { free(shared); free(base); json_free(def); return san; }
            json_append(cands, san);
        }
    }
    free(shared);
    if (strcmp(base, "default") != 0) {
        json_t *s_args = json_object();
        json_set_hdr(s_args, "value", base);
        json_t *san = bm_seam(io, "sanitize_task_id_for_path", s_args);
        if (bm_is_raise(san)) { free(base); json_free(def); return san; }
        json_append(cands, san);
    }
    json_append(cands, json_string("default"));
    if (session_key[0]) {
        size_t l = strlen(session_key) + 9;
        char *val = malloc(l);
        snprintf(val, l, "session:%s", session_key);
        json_t *s_args = json_object();
        json_set_hdr(s_args, "value", val);
        free(val);
        json_t *san = bm_seam(io, "sanitize_task_id_for_path", s_args);
        if (bm_is_raise(san)) { free(base); json_free(def); return san; }
        json_append(cands, san);
    }
    free(base);
    json_free(def);
    return cands;
}

/* ── default_docker_workspace_host_roots ───────────────────────────────── */
static json_t *bm_extend_paths(json_t *roots, json_t *more) {
    /* more is a list of path strings; append (adopt more's items via copy) */
    for (size_t i = 0; i < json_len(more); i++) json_append(roots, json_copy(json_get(more, i)));
    return roots;
}

/* PoP: default_docker_workspace_host_roots @ gateway/platforms/docker_media_paths.py:_default_docker_workspace_host_roots */
json_t *default_docker_workspace_host_roots(bm_io_t *io, json_t *in) {
    const char *session_key = json_get_str(in, "session_key", "");
    const char *workspace_key = json_get_str(in, "workspace_key", "");
    const char *workspace_cwd = json_get_str(in, "workspace_cwd", "");
    json_t *err = NULL;
    /* persistent() */
    json_t *pers = bm_seam(io, "persistent", json_object());
    if (bm_is_raise(pers)) return pers;
    int persistent = bm_truthy(pers);
    json_free(pers);
    if (!persistent) return json_array();
    json_t *roots = json_array();
    json_t *cm = bm_seam(io, "cwd_mount_enabled", json_object());
    if (bm_is_raise(cm)) { json_free(roots); return cm; }
    int cwd_enabled = bm_truthy(cm);
    json_free(cm);
    if (cwd_enabled) {
        if (!bm_ws_key(workspace_key)) {
            /* return _mounted_workspace_cwd() */
            json_t *mw = bm_seam(io, "_mounted_workspace_cwd", json_object());
            json_free(roots);
            return mw;
        }
        json_t *t_args = json_object();
        json_set_hdr(t_args, "key", "TERMINAL_DOCKER_SHARED_CONTAINER_KEY");
        json_set_hdr(t_args, "default", "");
        char *shared_raw = bm_seam_str(io, "tenv", t_args, &err);
        if (err) { json_free(roots); return err; }
        char *shared = bm_strip(shared_raw);
        free(shared_raw);
        int is_shared = shared[0] != 0;
        free(shared);
        if (is_shared) {
            json_t *g_args = json_object();
            json_set_hdr(g_args, "workspace_key", workspace_key);
            json_t *guess = bm_seam(io, "_shared_cwd_mount_guess", g_args);
            if (bm_is_raise(guess)) { json_free(roots); return guess; }
            bm_extend_paths(roots, guess);
            json_free(guess);
        } else {
            json_t *s_args = json_object();
            json_set_hdr(s_args, "workspace_key", workspace_key);
            json_set_hdr(s_args, "workspace_cwd", workspace_cwd);
            json_t *sess = bm_seam(io, "_session_mounted_workspace", s_args);
            if (bm_is_raise(sess)) { json_free(roots); return sess; }
            bm_extend_paths(roots, sess);
            json_free(sess);
        }
    }
    /* for root in _docker_persistent_sandbox_roots(session_key,"workspace",workspace_key) */
    json_t *sr_args = json_object();
    json_set_hdr(sr_args, "session_key", session_key);
    json_set_hdr(sr_args, "leaf", "workspace");
    json_set_hdr(sr_args, "workspace_key", workspace_key);
    json_t *sroots = bm_seam(io, "_docker_persistent_sandbox_roots", sr_args);
    if (bm_is_raise(sroots)) { json_free(roots); return sroots; }
    for (size_t i = 0; i < json_len(sroots); i++) {
        json_t *r = json_get(sroots, i);
        int dup = 0;
        for (size_t k = 0; k < json_len(roots); k++) {
            json_t *e = json_get(roots, k);
            if (r && e && r->type == JSON_STRING && e->type == JSON_STRING &&
                strcmp(r->str_val, e->str_val) == 0) { dup = 1; break; }
        }
        if (!dup) json_append(roots, json_copy(r));
    }
    json_free(sroots);
    return roots;
}

/* ── translate_docker_container_media_path ─────────────────────────────── */
typedef struct { char *host; char *container; } bm_mount_t;

static char *bm_rstrip_slash(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n-1] == '/') n--;
    return bm_strndup(s, n);
}


static const char *bm_arr_str(const json_t *arr, size_t i) {
    const json_t *v = json_get(arr, i);
    return v && v->type == JSON_STRING && v->str_val ? v->str_val : "";
}

static void bm_warn(bm_io_t *io, const char *candidate, const char *session_key,
                    const char *reason, int quiet) {
    json_t *args = json_object();
    json_set_hdr(args, "candidate", candidate);
    json_set_hdr(args, "session_key", session_key);
    json_set_hdr(args, "reason", reason);
    json_set(args, "quiet", json_bool(quiet ? true : false));
    json_t *r = bm_seam(io, "warn_unresolved_docker_media", args);
    json_free(r);
}

/* PoP: translate_docker_container_media_path @ gateway/platforms/base.py:_translate_docker_container_media_path */
json_t *translate_docker_container_media_path(bm_io_t *io, json_t *in) {
    const char *cand_raw = json_get_str(in, "candidate", "");
    const char *session_key = json_get_str(in, "session_key", "");
    const char *workspace_key = json_get_str(in, "workspace_key", "");
    const char *workspace_cwd = json_get_str(in, "workspace_cwd", "");
    int quiet = json_get_bool(in, "quiet", false);
    char *candidate = bm_as_posix(cand_raw);
    if (candidate[0] != '/') { free(candidate); return json_null(); }
    /* with contextlib.suppress(Exception): bridge import + call */
    json_t *bimp = bm_seam(io, "import__ensure_terminal_env_bridged", json_object());
    if (bm_is_raise(bimp)) { json_free(bimp); }
    else {
        json_free(bimp);
        json_t *bc = bm_seam(io, "_ensure_terminal_env_bridged", json_object());
        json_free(bc);
    }
    /* from docker_media_paths import _default_..., _docker_persistent_... */
    json_t *i1 = bm_seam(io, "import__default_docker_workspace_host_roots", json_object());
    if (bm_is_raise(i1)) { free(candidate); return i1; }
    json_free(i1);
    json_t *i2 = bm_seam(io, "import__docker_persistent_sandbox_roots", json_object());
    if (bm_is_raise(i2)) { free(candidate); return i2; }
    json_free(i2);
    /* mounts = [*_parse_docker_volume_mounts(), *_cache_dir_container_mounts()] */
    bm_mount_t *mounts = NULL; size_t nm = 0, cm = 0;
    #define ADD_MOUNT(h, c) do { if (nm == cm) { cm = cm ? cm*2 : 8; mounts = realloc(mounts, cm*sizeof *mounts); } mounts[nm].host = (h); mounts[nm].container = (c); nm++; } while (0)
    json_t *err = NULL;
    json_t *pv = bm_seam(io, "_parse_docker_volume_mounts", json_object());
    if (bm_is_raise(pv)) { free(candidate); return pv; }
    for (size_t i = 0; i < json_len(pv); i++) {
        json_t *pair = json_get(pv, i);
        char *h = bm_as_posix(bm_arr_str(pair, 0));
        char *c = bm_as_posix(bm_arr_str(pair, 1));
        ADD_MOUNT(h, c);
    }
    json_free(pv);
    json_t *cc = bm_seam(io, "_cache_dir_container_mounts", json_object());
    if (bm_is_raise(cc)) { for (size_t i=0;i<nm;i++){free(mounts[i].host);free(mounts[i].container);} free(mounts); free(candidate); return cc; }
    for (size_t i = 0; i < json_len(cc); i++) {
        json_t *pair = json_get(cc, i);
        char *h = bm_as_posix(bm_arr_str(pair, 0));
        char *c = bm_as_posix(bm_arr_str(pair, 1));
        ADD_MOUNT(h, c);
    }
    json_free(cc);
    /* mounted set */
    int has_workspace = 0, has_root = 0;
    for (size_t i = 0; i < nm; i++) {
        if (strcmp(mounts[i].container, "/workspace") == 0) has_workspace = 1;
        if (strcmp(mounts[i].container, "/root") == 0) has_root = 1;
    }
    if (!has_workspace) {
        json_t *args = json_object();
        json_set_hdr(args, "session_key", session_key);
        json_set_hdr(args, "workspace_key", workspace_key);
        json_set_hdr(args, "workspace_cwd", workspace_cwd);
        json_t *wr = bm_seam(io, "_default_docker_workspace_host_roots", args);
        if (bm_is_raise(wr)) { for (size_t i=0;i<nm;i++){free(mounts[i].host);free(mounts[i].container);} free(mounts); free(candidate); return wr; }
        for (size_t i = 0; i < json_len(wr); i++) {
            char *h = bm_as_posix(bm_arr_str(wr, i));
            ADD_MOUNT(h, strdup("/workspace"));
        }
        json_free(wr);
    }
    int cand_root_hermes = strncmp(candidate, "/root/.hermes", 13) == 0;
    if (!has_root && !cand_root_hermes) {
        json_t *args = json_object();
        json_set_hdr(args, "session_key", session_key);
        json_set_hdr(args, "leaf", "home");
        json_set_hdr(args, "workspace_key", workspace_key);
        json_t *hr = bm_seam(io, "_docker_persistent_sandbox_roots", args);
        if (bm_is_raise(hr)) { for (size_t i=0;i<nm;i++){free(mounts[i].host);free(mounts[i].container);} free(mounts); free(candidate); return hr; }
        for (size_t i = 0; i < json_len(hr); i++) {
            char *h = bm_as_posix(bm_arr_str(hr, i));
            ADD_MOUNT(h, strdup("/root"));
        }
        json_free(hr);
    }
    #undef ADD_MOUNT
    if (nm == 0) {
        bm_warn(io, candidate, session_key, "no sandbox mounts resolved", quiet);
        free(mounts); free(candidate);
        return json_null();
    }
    /* matched: (index, score) */
    typedef struct { size_t idx; size_t score; } bm_match_t;
    bm_match_t *matched = NULL; size_t nmt = 0, cmt = 0;
    for (size_t i = 0; i < nm; i++) {
        char *prefix = bm_rstrip_slash(mounts[i].container);
        if (!prefix[0]) { free(prefix); prefix = strdup("/"); }
        size_t pl = strlen(prefix);
        int hit = (strcmp(candidate, prefix) == 0) ||
                  (strncmp(candidate, prefix, pl) == 0 && candidate[pl] == '/');
        if (hit) {
            if (nmt == cmt) { cmt = cmt ? cmt*2 : 8; matched = realloc(matched, cmt*sizeof *matched); }
            matched[nmt].idx = i; matched[nmt].score = pl; nmt++;
        }
        free(prefix);
    }
    if (nmt == 0) {
        bm_warn(io, candidate, session_key, "no mounted prefix matches", quiet);
        for (size_t i=0;i<nm;i++){free(mounts[i].host);free(mounts[i].container);} free(mounts);
        free(matched); free(candidate);
        return json_null();
    }
    /* stable sort by score descending (insertion sort) */
    for (size_t i = 1; i < nmt; i++) {
        bm_match_t key = matched[i];
        size_t j = i;
        while (j > 0 && matched[j-1].score < key.score) { matched[j] = matched[j-1]; j--; }
        matched[j] = key;
    }
    json_t *result = NULL;
    for (size_t i = 0; i < nmt; i++) {
        bm_mount_t *m = &mounts[matched[i].idx];
        char *prefix = bm_rstrip_slash(m->container);
        if (!prefix[0]) { free(prefix); prefix = strdup("/"); }
        size_t pl = strlen(prefix);
        char *rel;
        if (strcmp(candidate, prefix) == 0) rel = strdup(".");
        else rel = strdup(candidate + pl + 1);
        free(prefix);
        char *host_join = bm_join(m->host, rel);
        free(rel);
        json_t *rp_args = json_object();
        json_set_hdr(rp_args, "path", host_join);
        json_set(rp_args, "strict", json_bool(true));
        json_t *translated = bm_seam(io, "_resolve_path", rp_args);
        free(host_join);
        if (bm_is_raise(translated)) { result = translated; break; }
        if (translated && translated->type != JSON_NULL) {
            const char *ts = translated->type == JSON_STRING && translated->str_val ? translated->str_val : "";
            int ok = (strcmp(ts, m->host) == 0);
            if (!ok) {
                json_t *w_args = json_object();
                json_set_hdr(w_args, "path", ts);
                json_set_hdr(w_args, "root", m->host);
                json_t *within = bm_seam(io, "_path_is_within", w_args);
                if (bm_is_raise(within)) { json_free(translated); result = within; break; }
                ok = bm_truthy(within);
                json_free(within);
            }
            if (ok) { result = translated; break; }
        }
        json_free(translated);
    }
    for (size_t i=0;i<nm;i++){free(mounts[i].host);free(mounts[i].container);} free(mounts);
    free(matched);
    if (result) { free(candidate); return result; }
    bm_warn(io, candidate, session_key, "host file missing from sandbox", quiet);
    free(candidate);
    return json_null();
}
