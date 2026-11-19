/* vision_analysis_flow.c — FLOW + leaf port of the tools/vision_tools.py
 * vision-analysis surface (the live module that replaced the old
 * tools/vision_analysis.py; PoP credits the DEFINING module tools/vision_tools.py).
 *
 * Ports the UNCLAIMED vision decision chains: the config/env setting readers,
 * the secure-cache / private-write filesystem flows, the native-fast-path gate
 * cluster, the aux-LLM call plumbing, the image prepare / native-analyze /
 * analysis-skeleton orchestration and the vision_analyze handler. Every foreign
 * sub-call (config, registry, providers, image_source, image_prep, history
 * budget, url_safety, website_policy, interrupt, PIL/pm, asyncio, logger,
 * tool_error, json.dumps, debug session) and every CLAIMED sibling
 * (_profile_rejects_tool_media, _supports_media_in_tool_results,
 * _is_image_size_error, _build_scale_note, _detect_video_mime_type,
 * _image_to_base64_data_url, _resize_image_for_vision, _download_image,
 * check_vision_requirements) is INJECTED through the va_io_t universal seam:
 * io->call(io, name, args) appends {"name","args"} to io->trace and returns the
 * scripted result (NULL/json_null == None; {"__raise__":cls,"message":msg} == a
 * raised exception). Unclaimed siblings that are themselves ports here are
 * seamed when composed and driven REAL by their own direct cases; the
 * analysis skeleton (_run_analysis) runs REAL as the backbone of the
 * vision_analyze_tool wrapper (its stage closure is transcribed inline).
 *
 * Seam model / ownership / representation contract lives in the harness
 * (tests/t_port_vision_analysis_flow.c) and oracle
 * (tests/oracle_display_flow.py shape). Summary:
 *  - va_call ADOPTS args (frees them; records a copy on the trace) and returns
 *    a NEW owned result. NULL args record {}.
 *  - io->trace / io->state are harness-owned. Inputs borrowed.
 *  - Integral floats normalize to ints on both sides; fixtures ASCII, no
 *    integral-float literals in str()-computed positions, no non-finite floats.
 *  - async functions run synchronously here; awaits are seam calls in source
 *    order (the oracle drives the live coroutine with asyncio.run + async
 *    recorders fed the SAME scripts).
 *  - BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) +
 *    asyncio.CancelledError PASS THROUGH every `except Exception` site.
 *  - Fail-closed: a missing/exhausted seam script is a RuntimeError raise spec
 *    returned by the dispatcher (recorded on the trace before propagation).
 *
 * Seam name table (name -> live call):
 *   load_config                 hermes_cli.config.load_config()
 *   cfg_get                     hermes_cli.config.cfg_get(cfg, *path, default=)
 *   is_managed                  hermes_cli.config.is_managed()
 *   secure_dir                  hermes_cli.config._secure_dir(dir)
 *   import_pillow               from PIL import Image  (module attr double)
 *   ensure_import               pm.ensure_import("vision")
 *   get_hermes_dir              hermes_constants.get_hermes_dir(sub, old)
 *   make_user_dir               hermes_dir_acl.make_user_dir(dir, ...)
 *   path.mkdir                  Path.mkdir(parents=, exist_ok=)
 *   os.open                     os.open(path, flags, mode)
 *   os.fdopen                   os.fdopen(fd, "wb") context manager
 *   handle.write                file handle .write(data)
 *   path.write_bytes            Path.write_bytes(data)
 *   path.unlink                 Path.unlink(missing_ok=)
 *   path.exists                 Path.exists()
 *   path.stat                   Path.stat().st_size
 *   os.getenv                   os.getenv(var, default)
 *   cast                        int(raw)/float(raw) in _read_vision_setting
 *   cfg_auxiliary               _cfg_auxiliary(*keys, default=)  (composed sibling)
 *   managed_install             _managed_install()  (composed sibling)
 *   profile_rejects_tool_media  _profile_rejects_tool_media(provider, model)
 *   supports_media_in_tool_results _supports_media_in_tool_results(provider, model)
 *   lookup_supports_vision      agent.image_routing._lookup_supports_vision(p,m,cfg)
 *   decide_image_input_mode     agent.image_routing.decide_image_input_mode(p,m,cfg)
 *   read_main_provider          agent.auxiliary_client._read_main_provider()
 *   read_main_model             agent.auxiliary_client._read_main_model()
 *   aux.async_call_llm          auxiliary_client module attr read (_load_auxiliary_client)
 *   aux.extract_content_or_reasoning  auxiliary_client module attr read
 *   aux_probe_mode              agent.auxiliary_client.aux_probe_mode()
 *   aux_probe_mode.__enter__    probe CM enter
 *   aux_probe_mode.__exit__     probe CM exit
 *   resolve_vision_provider_client  agent.auxiliary_client.resolve_vision_provider_client(**kw)
 *   async_is_safe_url           tools.url_safety.async_is_safe_url(url)
 *   redirect_target_from_response  tools.url_safety.redirect_target_from_response(resp)
 *   host_feature_enabled        tools.hosted_tool_policy.host_feature_enabled(name)
 *   resolve_image_source        tools.image_source.resolve_image_source(src, ctx, ...)
 *   secure_cache_dir            _secure_cache_dir(sub, old)  (composed sibling)
 *   write_private_bytes         _write_private_bytes(path, data)  (composed sibling)
 *   normalize_to_supported_image  _normalize_to_supported_image(path, mime)
 *   crop_image_region           _crop_image_region(path, region, offset_out=)
 *   run_encode_on_cpu_executor  _run_encode_on_cpu_executor(fn, *a, **k)
 *   validate_raster_image_decodable  _validate_raster_image_decodable(...)
 *   image_exceeds_dimension     _image_exceeds_dimension(path, max_dim)
 *   image_to_base64_data_url    _image_to_base64_data_url(path, mime_type=)
 *   resize_prepared             _resize_prepared(prepared, scale_info, **kw)
 *   native_turn_duplicate       native_turn_duplicate(image_url, region)
 *   repeat_refusal              repeat_refusal(image_url)
 *   resolve_repeat_cap          resolve_repeat_cap()
 *   resolve_embed_target_bytes  resolve_embed_target_bytes()
 *   record_embed                record_embed(image_url)
 *   release_embed               release_embed(image_url)
 *   is_interrupted              tools.interrupt.is_interrupted()
 *   tool_error                  tools.registry.tool_error(message, **extra)
 *   prepare_image               _prepare_image(...)  (composed sibling)
 *   media_messages              _media_messages(prompt, part_type, data_url)
 *   aux_call_kwargs             _aux_call_kwargs(messages, model, timeout, ...)
 *   load_auxiliary_client       _load_auxiliary_client()  (composed sibling)
 *   async_call_llm              module global async_call_llm(**kwargs)
 *   is_image_size_error         _is_image_size_error(err)
 *   call_vision_llm             _call_vision_llm(kwargs, empty_log, response)
 *   build_scale_note            _build_scale_note(scale_info, crop_offset)
 *   debug_call_data             _debug_call_data(kind, source, prompt, model)
 *   debug.log_call              _debug.log_call(tool_name, data)
 *   debug.save                  _debug.save()
 *   json.dumps                  json.dumps(result, indent=2, ensure_ascii=False)
 *   stage                       _run_analysis's injected stage callback
 *   should_use_native_vision_fast_path  _should_use_native_vision_fast_path()
 *   vision_analyze_native       _vision_analyze_native(...)  (composed sibling)
 *   configured_aux_model        _configured_aux_model(sections, env_vars)
 *   vision_analyze_tool         vision_analyze_tool(...)  (composed sibling)
 *   logger.debug                logger.debug(fmt, *args, **kw)
 */
#ifndef VISION_ANALYSIS_FLOW_C
#define VISION_ANALYSIS_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct va_io_t {
    json_t *(*call)(struct va_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ordered array of {"name","args"} */
    json_t *state;   /* mutable module-global mirror (harness-owned) */
    void   *ctx;
} va_io_t;

/* ── universal seam dispatch (adopts args, returns owned result) ───────── */
static json_t *va_call(va_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    json_free(args);
    return r ? r : json_null();
}

static bool va_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *va_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* ── FULL builtin exception hierarchy (child -> parent) ────────────────── */
typedef struct { const char *child; const char *parent; } va_exc_edge_t;
static const va_exc_edge_t VA_EXC_EDGES[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"ZipImportError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    /* module-local exception classes */
    {"ImageResolutionError", "Exception"},
    {"_ImagePrepError", "ValueError"},
};

static const char *va_exc_parent(const char *kind) {
    if (!kind) return NULL;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    for (size_t i = 0; i < sizeof(VA_EXC_EDGES) / sizeof(VA_EXC_EDGES[0]); i++)
        if (strcmp(VA_EXC_EDGES[i].child, kind) == 0) return VA_EXC_EDGES[i].parent;
    /* unknown kind == a synthetic Exception subclass (oracle make_exc fallback) */
    return "Exception";
}

/* True when kind is ancestor or a subclass of ancestor. */
static bool va_exc_is_a(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    const char *cur = kind;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, ancestor) == 0) return true;
        cur = va_exc_parent(cur);
    }
    return false;
}

static const char *va_exc_kind(const json_t *r) {
    if (!va_raised(r)) return "";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}

static const char *va_exc_msg(const json_t *r) {
    if (!va_raised(r)) return "";
    return json_get_str(r, "message", "");
}

/* `except Exception` swallows Exception-family; the BaseException-only trio +
 * CancelledError pass through. */
static bool va_exc_is_exception(const json_t *r) {
    return va_raised(r) && va_exc_is_a(va_exc_kind(r), "Exception");
}

/* ── truthiness / type predicates ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }

static json_t *va_get(const json_t *obj, const char *key) { return json_obj_get(obj, key); }
static const char *va_get_str(const json_t *obj, const char *key, const char *def) {
    return json_get_str(obj, key, def);
}
static double va_get_num(const json_t *obj, const char *key, double def) {
    return json_get_num(obj, key, def);
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static json_t *copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* Python str(v) for f-string interpolation of scalars (null -> "None"). */
static char *va_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(sval(v));
    return strdup("");
}

static bool va_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *va_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s; while (*b && va_isspace((unsigned char)*b)) b++;
    const char *e = b + strlen(b); while (e > b && va_isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1); if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0'; return r;
}
static char *va_lower(const char *s) {
    char *r = strdup(s ? s : ""); if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static bool va_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
/* Python str[:n] (ASCII). Caller frees. */
static char *va_slice(const char *s, size_t n) {
    if (!s) return strdup("");
    size_t l = strlen(s); if (n < l) l = n;
    char *r = (char *)malloc(l + 1); if (!r) return NULL;
    memcpy(r, s, l); r[l] = '\0'; return r;
}
static size_t va_strlen(const json_t *v) {
    if (j_is_str(v)) return strlen(sval(v));
    if (j_is_arr(v) || j_is_obj(v)) return v->c.count;
    return 0;
}

/* malloc'd concatenation helpers */
static char *va_cat2(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b);
    char *r = (char *)malloc(n + 1); if (!r) return NULL;
    strcpy(r, a); strcat(r, b); return r;
}
static char *va_cat3(const char *a, const char *b, const char *c) {
    char *t = va_cat2(a, b); char *r = va_cat2(t, c); free(t); return r;
}

/* single-pass {e}/{model} template substitution (Python str.format subset). */
static char *va_format_tpl(const char *tpl, const char *e_msg, const json_t *model) {
    char *mstr = va_pystr(model);
    size_t cap = strlen(tpl) + strlen(e_msg) + strlen(mstr) + 16;
    char *out = (char *)malloc(cap); if (!out) { free(mstr); return NULL; }
    size_t o = 0;
    for (const char *p = tpl; *p; ) {
        if (p[0] == '{' && p[1] == 'e' && p[2] == '}') {
            size_t l = strlen(e_msg); memcpy(out + o, e_msg, l); o += l; p += 3;
        } else if (p[0] == '{' && p[1] == 'm' && p[2] == 'o' && p[3] == 'd'
                   && p[4] == 'e' && p[5] == 'l' && p[6] == '}') {
            size_t l = strlen(mstr); memcpy(out + o, mstr, l); o += l; p += 7;
        } else { out[o++] = *p++; }
    }
    out[o] = '\0'; free(mstr); return out;
}

/* Python float(v) for a scalar: number -> itself; string -> strtod (stripped,
 * full consumption); other -> TypeError. Returns a NEW number node or a raise
 * spec. Underscore-digit separators are outside the fixture domain here. */
static json_t *va_float(const json_t *v) {
    if (j_is_num(v)) return json_number(v->num_val);
    if (j_is_str(v)) {
        char *s = va_strip(sval(v));
        if (!s[0]) { free(s); return va_raise("ValueError", "could not convert string to float: ''"); }
        char *end = NULL;
        double d = strtod(s, &end);
        bool ok = end && *end == '\0';
        free(s);
        if (!ok) return va_raise("ValueError", "could not convert string to float");
        return json_number(d);
    }
    return va_raise("TypeError", "float() argument must be a string or a real number");
}

/* ── module constants (mirrors of the quarry Assigns) ──────────────────── */
#define VA_MAX_BASE64_BYTES      (20.0 * 1024 * 1024)
#define VA_RESIZE_TARGET_BYTES   (5.0 * 1024 * 1024)
#define VA_EMBED_MAX_DIMENSION   1568.0
#define VA_MAX_VALIDATED_FRAME_COUNT 100.0
#define VA_MAX_VALIDATED_AGGREGATE_PIXELS 100000000.0

static const char *VA_VIDEO_MIME_KEYS[] = {
    ".avi", ".mkv", ".mov", ".mp4", ".mpeg", ".mpg", ".webm"
}; /* sorted(_VIDEO_MIME_TYPES.keys()) */

static const char *VA_BILLING_HINTS[] = {
    "402", "insufficient", "payment required", "credits", "billing"
};
static const char *VA_IMAGE_CAP_HINTS[] = {
    "does not support", "not support image", "content_policy", "multimodal",
    "unrecognized request argument", "image input"
};
static const char *VA_IMAGE_FMT_HINTS[] = { "invalid_request", "image_url" };
static const char *VA_VIDEO_CAP_HINTS[] = {
    "does not support", "not support video", "content_policy", "multimodal",
    "unrecognized request argument", "video input", "video_url"
};
static const char *VA_SIZE_HINTS[] = {
    "too large", "payload", "413", "content_too_large", "request_too_large",
    "exceeds", "size limit"
};
static const char *VA_IMAGE_TPL_BILLING =
    "Insufficient credits or payment required. Please top up your "
    "API provider account and try again. Error: {e}";
static const char *VA_IMAGE_TPL_CAP =
    "{model} does not support vision or our request was not "
    "accepted by the server. Error: {e}";
static const char *VA_IMAGE_TPL_FMT =
    "The vision API rejected the image. This can happen when the "
    "image is in an unsupported format, corrupted, or still too "
    "large after auto-resize. Try a smaller JPEG/PNG and retry. "
    "Error: {e}";
static const char *VA_VIDEO_TPL_CAP =
    "The model does not support video analysis or the request was "
    "rejected. Ensure you're using a video-capable model "
    "(e.g. google/gemini-2.5-flash). Error: {e}";
static const char *VA_VIDEO_TPL_SIZE =
    "The video is too large for the API. Try compressing or trimming "
    "the video (max ~50 MB). Error: {e}";

static bool va_any_hint(const char *const *hints, size_t n, const char *hay) {
    for (size_t i = 0; i < n; i++) if (strstr(hay, hints[i])) return true;
    return false;
}

/* ═══════════════════════════ LEAVES ═══════════════════════════════════ */

/* PoP: va_media_messages @ tools/vision_tools.py:_media_messages */
static json_t *va_media_messages(va_io_t *io, json_t *in) {
    (void)io;
    const char *prompt = va_get_str(in, "user_prompt", "");
    const char *part = va_get_str(in, "part_type", "");
    const char *url = va_get_str(in, "data_url", "");
    json_t *text = json_object();
    json_set_hdr(text, "type", "text");
    json_set_hdr(text, "text", prompt);
    json_t *inner = json_object();
    json_set_hdr(inner, "url", url);
    json_t *media = json_object();
    json_set_hdr(media, "type", part);
    json_set(media, part, inner);
    json_t *content = json_array();
    json_append(content, text);
    json_append(content, media);
    json_t *msg = json_object();
    json_set_hdr(msg, "role", "user");
    json_set(msg, "content", content);
    json_t *out = json_array();
    json_append(out, msg);
    return out;
}

/* PoP: va_debug_call_data @ tools/vision_tools.py:_debug_call_data */
static json_t *va_debug_call_data(va_io_t *io, json_t *in) {
    (void)io;
    const char *kind = va_get_str(in, "kind", "");
    json_t *source = va_get(in, "source");
    json_t *up = va_get(in, "user_prompt");
    json_t *model = va_get(in, "model");
    const char *ups = sval(up);
    char *prompt;
    if (strlen(ups) > 200) { char *s = va_slice(ups, 200); prompt = va_cat2(s, "..."); free(s); }
    else prompt = strdup(ups);
    char *urlkey = va_cat2(kind, "_url");
    char *sizekey = va_cat2(kind, "_size_bytes");
    json_t *params = json_object();
    json_set(params, urlkey, copy_or_null(source));
    json_set_hdr(params, "user_prompt", prompt);
    json_set(params, "model", copy_or_null(model));
    json_t *out = json_object();
    json_set(out, "parameters", params);
    json_set(out, "error", json_null());
    json_set(out, "success", json_bool(false));
    json_set(out, "analysis_length", json_number(0));
    json_set(out, "model_used", copy_or_null(model));
    json_set(out, sizekey, json_number(0));
    free(prompt); free(urlkey); free(sizekey);
    return out;
}

/* PoP: va_too_large_message @ tools/vision_tools.py:_too_large_message */
static json_t *va_too_large_message(va_io_t *io, json_t *in) {
    (void)io;
    const char *url = va_get_str(in, "image_data_url", "");
    double mb = (double)strlen(url) / (1024.0 * 1024.0);
    double limit = VA_MAX_BASE64_BYTES / (1024.0 * 1024.0);
    char buf[512];
    snprintf(buf, sizeof buf,
        "Image too large for vision API: base64 payload is %.1f MB "
        "(limit %.0f MB) even after resizing. Run `hermes pm repair` "
        "to restore Pillow for auto-resize, or compress the image manually.",
        mb, limit);
    return json_string(buf);
}

/* PoP: va_unsupported_video_format @ tools/vision_tools.py:_unsupported_video_format */
static json_t *va_unsupported_video_format(va_io_t *io, json_t *in) {
    (void)io;
    const char *suffix = va_get_str(in, "suffix", "");
    char *joined = strdup("");
    for (size_t i = 0; i < sizeof(VA_VIDEO_MIME_KEYS) / sizeof(char *); i++) {
        char *t = joined;
        joined = (i == 0) ? strdup(VA_VIDEO_MIME_KEYS[i])
                          : va_cat3(t, ", ", VA_VIDEO_MIME_KEYS[i]);
        free(t);
    }
    char *a = va_cat3("Unsupported video format: '", suffix, "'. Supported: ");
    char *msg = va_cat2(a, joined);
    free(a); free(joined);
    json_t *out = json_string(msg);
    free(msg);
    return out;
}

/* PoP: va_build_native_vision_tool_result @ tools/vision_tools.py:_build_native_vision_tool_result */
static json_t *va_build_native_vision_tool_result(va_io_t *io, json_t *in) {
    (void)io;
    json_t *image_url = va_get(in, "image_url");
    json_t *question = va_get(in, "question");
    const char *data_url = va_get_str(in, "image_data_url", "");
    double size = va_get_num(in, "image_size_bytes", 0);
    json_t *scale_note = va_get(in, "scale_note");
    char *text = strdup(
        "Image loaded into your context — you can see it natively now. "
        "Use your built-in vision to answer the user.");
    if (j_is_str(question)) {
        char *q = va_strip(sval(question));
        if (q[0]) {
            char *t = text; text = va_cat3(t, "\n\nQuestion: ", q); free(t);
        }
        free(q);
    }
    if (jtruth(scale_note)) {
        char *t = text; text = va_cat3(t, "\n\nNote: ", sval(scale_note)); free(t);
    }
    json_t *text_part = json_object();
    json_set_hdr(text_part, "type", "text");
    json_set_hdr(text_part, "text", text);
    free(text);
    json_t *iu_inner = json_object();
    json_set_hdr(iu_inner, "url", data_url);
    json_t *img_part = json_object();
    json_set_hdr(img_part, "type", "image_url");
    json_set(img_part, "image_url", iu_inner);
    json_t *content = json_array();
    json_append(content, text_part);
    json_append(content, img_part);
    char summary[256];
    snprintf(summary, sizeof summary,
        "Image attached natively for the main model (%.1f KB). "
        "Answer using built-in vision.", size / 1024.0);
    char *url200 = va_slice(sval(image_url), 200);
    json_t *meta = json_object();
    json_set_hdr(meta, "image_url", url200);
    json_set(meta, "size_bytes", json_number(size));
    json_set(meta, "native_vision", json_bool(true));
    free(url200);
    json_t *out = json_object();
    json_set(out, "_multimodal", json_bool(true));
    json_set(out, "content", content);
    json_set_hdr(out, "text_summary", summary);
    json_set(out, "meta", meta);
    return out;
}

/* ═══════════════════════ SYNC CONFIG / FS FLOWS ═══════════════════════ */

/* PoP: va_cfg_auxiliary @ tools/vision_tools.py:_cfg_auxiliary */
static json_t *va_cfg_auxiliary(va_io_t *io, json_t *in) {
    json_t *def = va_get(in, "default");
    /* from hermes_cli.config import cfg_get, load_config (silent import);
     * return cfg_get(load_config(), "auxiliary", *keys, default=default) */
    json_t *cfg = va_call(io, "load_config", json_object());
    if (va_raised(cfg)) {
        if (va_exc_is_exception(cfg)) { json_free(cfg); return copy_or_null(def); }
        return cfg;
    }
    json_t *keys = json_array();
    json_append(keys, json_string("auxiliary"));
    json_t *ks = va_get(in, "keys");
    if (j_is_arr(ks)) for (size_t i = 0; i < json_len(ks); i++) json_append(keys, json_copy(json_get(ks, i)));
    json_t *args = json_object();
    json_set(args, "config", cfg);
    json_set(args, "keys", keys);
    json_set(args, "default", copy_or_null(def));
    json_t *r = va_call(io, "cfg_get", args);
    if (va_raised(r)) {
        if (va_exc_is_exception(r)) { json_free(r); return copy_or_null(def); }
        return r;
    }
    return r;
}

/* _read_vision_setting._accept: cast(raw) with (TypeError,ValueError) swallow
 * and a minimum floor. Returns owned value or json_null (None). */
static json_t *va_accept_setting(va_io_t *io, json_t *raw, const char *cast, json_t *minimum) {
    json_t *args = json_object();
    json_set(args, "raw", copy_or_null(raw));
    json_set_hdr(args, "cast", cast);
    json_t *val = va_call(io, "cast", args);
    if (va_raised(val)) {
        const char *k = va_exc_kind(val);
        if (va_exc_is_a(k, "TypeError") || va_exc_is_a(k, "ValueError")) {
            json_free(val); return json_null();
        }
        return val;   /* propagate other raises */
    }
    if (jpresent(minimum) && j_is_num(val) && j_is_num(minimum)) {
        if (val->num_val < minimum->num_val) { json_free(val); return json_null(); }
    }
    return val;
}

/* PoP: va_read_vision_setting @ tools/vision_tools.py:_read_vision_setting */
static json_t *va_read_vision_setting(va_io_t *io, json_t *in) {
    const char *env_var = va_get_str(in, "env_var", "");
    const char *key = va_get_str(in, "key", "");
    const char *cast = va_get_str(in, "cast", "int");
    json_t *minimum = va_get(in, "minimum");
    /* os.getenv(env_var, "").strip() or None */
    json_t *ga = json_object();
    json_set_hdr(ga, "var", env_var);
    json_set_hdr(ga, "default", "");
    json_t *raw = va_call(io, "os.getenv", ga);
    if (va_raised(raw)) return raw;
    char *stripped = va_strip(sval(raw));
    json_free(raw);
    json_t *env_val = stripped[0] ? json_string(stripped) : json_null();
    free(stripped);
    json_t *val = va_accept_setting(io, env_val, cast, minimum);
    json_free(env_val);
    if (va_raised(val)) return val;
    if (jpresent(val)) return val;
    json_free(val);
    /* _cfg_auxiliary("vision", key) */
    json_t *ca = json_object();
    json_t *ks = json_array();
    json_append(ks, json_string("vision"));
    json_append(ks, json_string(key));
    json_set(ca, "keys", ks);
    json_set(ca, "default", json_null());
    json_t *cfg = va_call(io, "cfg_auxiliary", ca);
    if (va_raised(cfg)) return cfg;
    json_t *val2 = va_accept_setting(io, cfg, cast, minimum);
    json_free(cfg);
    return val2;
}

/* PoP: va_managed_install @ tools/vision_tools.py:_managed_install */
static json_t *va_managed_install(va_io_t *io, json_t *in) {
    (void)in;
    json_t *r = va_call(io, "is_managed", json_object());
    if (va_raised(r)) {
        if (va_exc_is_exception(r)) { json_free(r); return json_bool(false); }
        return r;
    }
    bool b = jtruth(r);
    json_free(r);
    return json_bool(b);
}

/* PoP: va_secure_cache_dir @ tools/vision_tools.py:_secure_cache_dir */
static json_t *va_secure_cache_dir(va_io_t *io, json_t *in) {
    const char *sub = va_get_str(in, "new_subpath", "");
    const char *old = va_get_str(in, "old_name", "");
    json_t *ga = json_object();
    json_set_hdr(ga, "new_subpath", sub);
    json_set_hdr(ga, "old_name", old);
    json_t *cache_dir = va_call(io, "get_hermes_dir", ga);
    if (va_raised(cache_dir)) return cache_dir;
    /* if _managed_install(): cache_dir.mkdir(parents=True, exist_ok=True); return */
    json_t *mi = va_call(io, "managed_install", json_object());
    if (va_raised(mi)) { json_free(cache_dir); return mi; }
    bool managed = jtruth(mi);
    json_free(mi);
    if (managed) {
        json_t *ma = json_object();
        json_set(ma, "path", json_copy(cache_dir));
        json_set(ma, "parents", json_bool(true));
        json_set(ma, "exist_ok", json_bool(true));
        json_t *mr = va_call(io, "path.mkdir", ma);
        if (va_raised(mr)) { json_free(cache_dir); return mr; }
        json_free(mr);
        return cache_dir;
    }
    /* make_user_dir(cache_dir, parents=True, exist_ok=True, mode=0o700) */
    json_t *ua = json_object();
    json_set(ua, "path", json_copy(cache_dir));
    json_set(ua, "parents", json_bool(true));
    json_set(ua, "exist_ok", json_bool(true));
    json_set(ua, "mode", json_number(0700));
    json_t *ur = va_call(io, "make_user_dir", ua);
    if (va_raised(ur)) { json_free(cache_dir); return ur; }
    json_free(ur);
    /* try: from hermes_cli.config import _secure_dir; _secure_dir(cache_dir)
     * except Exception as exc: logger.debug(...) */
    json_t *sa = json_object();
    json_set(sa, "path", json_copy(cache_dir));
    json_t *sr = va_call(io, "secure_dir", sa);
    if (va_raised(sr)) {
        if (va_exc_is_exception(sr)) {
            json_t *la = json_object();
            json_set_hdr(la, "fmt", "vision: cache dir chmod skipped: %s");
            json_t *aa = json_array();
            json_append(aa, json_string(va_exc_msg(sr)));
            json_set(la, "args", aa);
            json_t *lr = va_call(io, "logger.debug", la);
            json_free(lr);
            json_free(sr);
        } else { json_free(cache_dir); return sr; }
    } else json_free(sr);
    return cache_dir;
}

/* PoP: va_write_private_bytes @ tools/vision_tools.py:_write_private_bytes */
static json_t *va_write_private_bytes(va_io_t *io, json_t *in) {
    json_t *path = va_get(in, "path");
    json_t *data = va_get(in, "data");
    /* flags = O_WRONLY|O_CREAT|O_TRUNC; try os.open(str(path), flags, 0o600)
     * except OSError: path.write_bytes(data); return */
    json_t *oa = json_object();
    json_set(oa, "path", copy_or_null(path));
    json_set(oa, "flags", json_number(577)); /* O_WRONLY|O_CREAT|O_TRUNC = 1|64|512 */
    json_set(oa, "mode", json_number(0600));
    json_t *fd = va_call(io, "os.open", oa);
    if (va_raised(fd)) {
        if (va_exc_is_a(va_exc_kind(fd), "OSError")) {
            json_free(fd);
            json_t *wa = json_object();
            json_set(wa, "path", copy_or_null(path));
            json_set(wa, "data", copy_or_null(data));
            json_t *wr = va_call(io, "path.write_bytes", wa);
            if (va_raised(wr)) return wr;
            json_free(wr);
            return json_null();
        }
        return fd;
    }
    /* with os.fdopen(fd, "wb") as handle: handle.write(data) */
    json_t *fa = json_object();
    json_set(fa, "fd", json_copy(fd));
    json_set_hdr(fa, "mode", "wb");
    json_t *handle = va_call(io, "os.fdopen", fa);
    if (va_raised(handle)) { json_free(fd); return handle; }
    json_t *wra = json_object();
    json_set(wra, "data", copy_or_null(data));
    json_t *wr = va_call(io, "handle.write", wra);
    /* with-block: __exit__ fires even when the body raises (exc info passed);
     * a truthy __exit__ suppresses the body raise. */
    json_t *ea = json_object();
    if (va_raised(wr)) {
        json_t *e = json_object();
        json_set_hdr(e, "type", va_exc_kind(wr));
        json_set_hdr(e, "message", va_exc_msg(wr));
        json_set(ea, "exc", e);
    } else {
        json_set(ea, "exc", json_null());
    }
    json_t *er = va_call(io, "os.fdopen.__exit__", ea);
    bool er_raised = va_raised(er);
    bool suppress = jtruth(er);
    json_free(handle); json_free(fd);
    /* an __exit__ raise propagates (replacing any body raise); only a normal
     * truthy __exit__ return suppresses the body raise. */
    if (er_raised) { if (wr) json_free(wr); return er; }
    json_free(er);
    if (va_raised(wr)) {
        if (suppress) { json_free(wr); return json_null(); }
        return wr;
    }
    json_free(wr);
    return json_null();
}

/* PoP: va_import_pillow_for_resize @ tools/vision_tools.py:_import_pillow_for_resize */
static json_t *va_import_pillow_for_resize(va_io_t *io, json_t *in) {
    (void)in;
    /* try: from PIL import Image  except ImportError: ... */
    json_t *img = va_call(io, "import_pillow", json_object());
    if (va_raised(img)) {
        if (!va_exc_is_a(va_exc_kind(img), "ImportError")) return img;
        json_free(img);
        /* try: from pm import ensure_import; ensure_import("vision");
         *      from PIL import Image  except Exception: return None */
        json_t *ea = json_object();
        json_set_hdr(ea, "name", "vision");
        json_t *er = va_call(io, "ensure_import", ea);
        if (va_raised(er)) {
            if (va_exc_is_exception(er)) { json_free(er); return json_null(); }
            return er;
        }
        json_free(er);
        json_t *img2 = va_call(io, "import_pillow", json_object());
        if (va_raised(img2)) {
            if (va_exc_is_exception(img2)) { json_free(img2); return json_null(); }
            return img2;
        }
        return img2;
    }
    return img;
}

/* PoP: va_accepts_tool_result_images @ tools/vision_tools.py:_accepts_tool_result_images */
static json_t *va_accepts_tool_result_images(va_io_t *io, json_t *in) {
    json_t *provider = va_get(in, "provider");
    json_t *model = va_get(in, "model");
    json_t *cfg = va_get(in, "cfg");
    json_t *pa = json_object();
    json_set(pa, "provider", copy_or_null(provider));
    json_set(pa, "model", copy_or_null(model));
    json_t *rej = va_call(io, "profile_rejects_tool_media", pa);
    if (va_raised(rej)) return rej;
    bool rejects = jtruth(rej);
    json_free(rej);
    if (rejects) return json_bool(false);
    json_t *sa = json_object();
    json_set(sa, "provider", copy_or_null(provider));
    json_set(sa, "model", copy_or_null(model));
    json_t *sup = va_call(io, "supports_media_in_tool_results", sa);
    if (va_raised(sup)) return sup;
    bool supports = jtruth(sup);
    json_free(sup);
    if (supports) return json_bool(true);
    /* from agent.image_routing import _lookup_supports_vision;
     * return _lookup_supports_vision(provider, model, cfg) is True */
    json_t *la = json_object();
    json_set(la, "provider", copy_or_null(provider));
    json_set(la, "model", copy_or_null(model));
    json_set(la, "cfg", copy_or_null(cfg));
    json_t *look = va_call(io, "lookup_supports_vision", la);
    if (va_raised(look)) return look;
    bool is_true = look && look->type == JSON_BOOL && look->bool_val;
    json_free(look);
    return json_bool(is_true);
}

/* PoP: va_native_tool_result_images @ tools/vision_tools.py:_native_tool_result_images */
static json_t *va_native_tool_result_images(va_io_t *io, json_t *in) {
    json_t *provider = va_get(in, "provider");
    json_t *model = va_get(in, "model");
    json_t *cfg = va_get(in, "cfg");
    json_t *da = json_object();
    json_set(da, "provider", copy_or_null(provider));
    json_set(da, "model", copy_or_null(model));
    json_set(da, "cfg", copy_or_null(cfg));
    json_t *mode = va_call(io, "decide_image_input_mode", da);
    if (va_raised(mode)) return mode;
    bool native = j_is_str(mode) && strcmp(sval(mode), "native") == 0;
    json_free(mode);
    if (!native) return json_bool(false);
    json_t *aa = json_object();
    json_set(aa, "provider", copy_or_null(provider));
    json_set(aa, "model", copy_or_null(model));
    json_set(aa, "cfg", copy_or_null(cfg));
    return va_call(io, "accepts_tool_result_images", aa);
}

/* PoP: va_should_use_native_vision_fast_path @ tools/vision_tools.py:_should_use_native_vision_fast_path */
static json_t *va_should_use_native_vision_fast_path(va_io_t *io, json_t *in) {
    (void)in;
    /* try: imports + return _native_tool_result_images(_read_main_provider(),
     *      _read_main_model(), load_config())
     * except Exception as exc: logger.debug(...); return False */
    json_t *exc = NULL;
    json_t *prov = va_call(io, "read_main_provider", json_object());
    if (va_raised(prov)) { exc = prov; goto fail; }
    json_t *mod = va_call(io, "read_main_model", json_object());
    if (va_raised(mod)) { json_free(prov); exc = mod; goto fail; }
    json_t *cfg = va_call(io, "load_config", json_object());
    if (va_raised(cfg)) { json_free(prov); json_free(mod); exc = cfg; goto fail; }
    json_t *na = json_object();
    json_set(na, "provider", prov);
    json_set(na, "model", mod);
    json_set(na, "cfg", cfg);
    json_t *r = va_call(io, "native_tool_result_images", na);
    if (va_raised(r)) { exc = r; goto fail; }
    return r;
fail:
    if (va_exc_is_exception(exc)) {
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "Native vision fast-path check failed: %s");
        json_t *aa = json_array();
        json_append(aa, json_string(va_exc_msg(exc)));
        json_set(la, "args", aa);
        json_t *lr = va_call(io, "logger.debug", la);
        json_free(lr);
        json_free(exc);
        return json_bool(false);
    }
    return exc;   /* BaseException passes through */
}

/* PoP: va_unlink_quietly @ tools/vision_tools.py:_unlink_quietly */
static json_t *va_unlink_quietly(va_io_t *io, json_t *in) {
    json_t *path = va_get(in, "path");
    if (!jpresent(path)) return json_null();
    json_t *ua = json_object();
    json_set(ua, "path", json_copy(path));
    json_set(ua, "missing_ok", json_bool(true));
    json_t *r = va_call(io, "path.unlink", ua);
    if (va_raised(r)) {
        if (va_exc_is_exception(r)) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: va_aux_call_kwargs @ tools/vision_tools.py:_aux_call_kwargs */
static json_t *va_aux_call_kwargs(va_io_t *io, json_t *in) {
    json_t *messages = va_get(in, "messages");
    json_t *model = va_get(in, "model");
    double timeout = va_get_num(in, "default_timeout", 0);
    json_t *min_timeout = va_get(in, "min_timeout");
    double temperature = 0.1;
    /* try: _vision_cfg = _cfg_auxiliary("vision", default={}) or {} ...
     * except Exception: pass */
    json_t *ca = json_object();
    json_t *ks = json_array(); json_append(ks, json_string("vision"));
    json_set(ca, "keys", ks);
    json_set(ca, "default", json_object());
    json_t *vcfg = va_call(io, "cfg_auxiliary", ca);
    if (va_raised(vcfg)) {
        if (va_exc_is_exception(vcfg)) { json_free(vcfg); vcfg = json_object(); }
        else return vcfg;
    }
    if (!jtruth(vcfg)) { json_free(vcfg); vcfg = json_object(); }
    json_t *to = va_get(vcfg, "timeout");
    json_t *temp = va_get(vcfg, "temperature");
    json_t *raise_spec = NULL;
    if (jpresent(to)) {
        /* timeout = max(float(to), float("-inf") if min_timeout is None else min_timeout) */
        json_t *fto = va_float(to);
        if (va_raised(fto)) raise_spec = fto;
        else {
            double floor_v = -INFINITY;
            if (jpresent(min_timeout)) {
                floor_v = j_is_num(min_timeout) ? min_timeout->num_val : 0.0;
            } else {
                json_t *neg_inf = json_string("-inf");
                json_t *finf = va_float(neg_inf);
                json_free(neg_inf);
                if (va_raised(finf)) raise_spec = finf;
                else { floor_v = finf->num_val; json_free(finf); }
            }
            if (!raise_spec) {
                double v = fto->num_val;
                timeout = v > floor_v ? v : floor_v;
            }
            json_free(fto);
        }
    }
    if (!raise_spec && jpresent(temp)) {
        json_t *ftemp = va_float(temp);
        if (va_raised(ftemp)) raise_spec = ftemp;
        else { temperature = ftemp->num_val; json_free(ftemp); }
    }
    json_free(vcfg);
    if (raise_spec) {
        if (va_exc_is_exception(raise_spec)) { json_free(raise_spec); }
        else return raise_spec;
    }
    json_t *out = json_object();
    json_set_hdr(out, "task", "vision");
    json_set(out, "messages", copy_or_null(messages));
    json_set(out, "temperature", json_number(temperature));
    json_set(out, "timeout", json_number(timeout));
    if (jtruth(model)) json_set(out, "model", json_copy(model));
    return out;
}

/* PoP: va_configured_aux_model @ tools/vision_tools.py:_configured_aux_model */
static json_t *va_configured_aux_model(va_io_t *io, json_t *in) {
    json_t *sections = va_get(in, "sections");
    json_t *env_vars = va_get(in, "env_vars");
    if (j_is_arr(sections)) {
        for (size_t i = 0; i < json_len(sections); i++) {
            json_t *sec = json_get(sections, i);
            json_t *ca = json_object();
            json_t *ks = json_array();
            json_append(ks, json_copy(sec));
            json_append(ks, json_string("model"));
            json_set(ca, "keys", ks);
            json_set(ca, "default", json_null());
            json_t *vm = va_call(io, "cfg_auxiliary", ca);
            if (va_raised(vm)) return vm;
            if (jtruth(vm)) {
                char *s = va_pystr(vm);
                char *st = va_strip(s);
                free(s);
                if (st[0]) { json_free(vm); return json_string(st); }
                free(st); json_free(vm);
                break;
            }
            json_free(vm);
        }
    }
    /* next((v for v in (os.getenv(e,"").strip() for e in env_vars) if v), None) */
    if (j_is_arr(env_vars)) {
        for (size_t i = 0; i < json_len(env_vars); i++) {
            json_t *e = json_get(env_vars, i);
            json_t *ga = json_object();
            json_set_hdr(ga, "var", sval(e));
            json_set_hdr(ga, "default", "");
            json_t *raw = va_call(io, "os.getenv", ga);
            if (va_raised(raw)) return raw;
            char *st = va_strip(sval(raw));
            json_free(raw);
            if (st[0]) { json_t *out = json_string(st); free(st); return out; }
            free(st);
        }
    }
    return json_null();
}

/* PoP: va_load_auxiliary_client @ tools/vision_tools.py:_load_auxiliary_client */
static json_t *va_load_auxiliary_client(va_io_t *io, json_t *in) {
    (void)in;
    json_t *acl = va_get(io->state, "async_call_llm");
    json_t *ecr = va_get(io->state, "extract_content_or_reasoning");
    if (jpresent(acl) && jpresent(ecr)) return json_null();
    /* from agent import auxiliary_client as _aux (silent import); attribute
     * reads fire seams only when the `or` short-circuit needs them. */
    if (!jtruth(acl)) {
        json_t *r = va_call(io, "aux.async_call_llm", json_object());
        if (va_raised(r)) return r;
        json_set(io->state, "async_call_llm", r);
    }
    json_t *ecr2 = va_get(io->state, "extract_content_or_reasoning");
    if (!jtruth(ecr2)) {
        json_t *r = va_call(io, "aux.extract_content_or_reasoning", json_object());
        if (va_raised(r)) return r;
        json_set(io->state, "extract_content_or_reasoning", r);
    }
    return json_null();
}

/* PoP: va_check_video_requirements @ tools/vision_tools.py:check_video_requirements */
static json_t *va_check_video_requirements(va_io_t *io, json_t *in) {
    (void)in;
    json_t *cm = va_call(io, "aux_probe_mode", json_object());
    if (va_raised(cm)) return cm;
    json_free(cm);
    json_t *ent = va_call(io, "aux_probe_mode.__enter__", json_object());
    if (va_raised(ent)) return ent;
    json_free(ent);
    /* any(resolve_vision_provider_client(**kw)[1] is not None
     *     for kw in ({}, {"provider":"auto"})) */
    json_t *body_exc = NULL;
    bool result = false;
    const char *kws[2] = { NULL, "auto" };
    for (int i = 0; i < 2; i++) {
        json_t *kw = json_object();
        if (kws[i]) json_set_hdr(kw, "provider", kws[i]);
        json_t *ra = json_object();
        json_set(ra, "kwargs", kw);
        json_t *r = va_call(io, "resolve_vision_provider_client", ra);
        if (va_raised(r)) { body_exc = r; break; }
        bool second_present = j_is_arr(r) && json_len(r) >= 2 && jpresent(json_get(r, 1));
        json_free(r);
        if (second_present) { result = true; break; }
    }
    /* __exit__(exc_type, exc_val, exc_tb) — fires even on raise */
    json_t *ea = json_object();
    if (body_exc) {
        json_t *e = json_object();
        json_set_hdr(e, "type", va_exc_kind(body_exc));
        json_set_hdr(e, "message", va_exc_msg(body_exc));
        json_set(ea, "exc", e);
    } else json_set(ea, "exc", json_null());
    json_t *ex = va_call(io, "aux_probe_mode.__exit__", ea);
    if (va_raised(ex)) { if (body_exc) json_free(body_exc); return ex; }
    bool suppress = jtruth(ex);
    json_free(ex);
    if (body_exc) {
        if (suppress) { json_free(body_exc); return json_null(); }
        return body_exc;
    }
    return json_bool(result);
}

/* ═══════════════════════════ ASYNC FLOWS ══════════════════════════════ */

/* PoP: va_validate_image_url_async @ tools/vision_tools.py:_validate_image_url_async */
static json_t *va_validate_image_url_async(va_io_t *io, json_t *in) {
    json_t *url = va_get(in, "url");
    /* if not (isinstance(url,str) and url.startswith(("http://","https://"))
     *         and urlparse(url).netloc): return False */
    bool ok = j_is_str(url)
        && (va_startswith(sval(url), "http://") || va_startswith(sval(url), "https://"));
    if (ok) {
        /* urlparse(url).netloc — model via seam (urlparse is foreign) */
        json_t *ua = json_object();
        json_set(ua, "url", json_copy(url));
        json_t *nl = va_call(io, "urlparse_netloc", ua);
        if (va_raised(nl)) return nl;
        ok = jtruth(nl);
        json_free(nl);
    }
    if (!ok) return json_bool(false);
    json_t *sa = json_object();
    json_set(sa, "url", json_copy(url));
    return va_call(io, "async_is_safe_url", sa);
}

/* PoP: va_ssrf_redirect_guard @ tools/vision_tools.py:_ssrf_redirect_guard */
static json_t *va_ssrf_redirect_guard(va_io_t *io, json_t *in) {
    json_t *response = va_get(in, "response");
    json_t *ra = json_object();
    json_set(ra, "response", copy_or_null(response));
    json_t *redirect = va_call(io, "redirect_target_from_response", ra);
    if (va_raised(redirect)) return redirect;
    if (jtruth(redirect)) {
        json_t *sa = json_object();
        json_set(sa, "url", json_copy(redirect));
        json_t *safe = va_call(io, "async_is_safe_url", sa);
        if (va_raised(safe)) { json_free(redirect); return safe; }
        bool is_safe = jtruth(safe);
        json_free(safe);
        if (!is_safe) {
            char *msg = va_cat2("Blocked redirect to private/internal address: ", sval(redirect));
            json_free(redirect);
            json_t *e = va_raise("ValueError", msg);
            free(msg);
            return e;
        }
    }
    json_free(redirect);
    return json_null();
}

/* PoP: va_call_vision_llm @ tools/vision_tools.py:_call_vision_llm */
static json_t *va_call_vision_llm(va_io_t *io, json_t *in) {
    json_t *call_kwargs = va_get(in, "call_kwargs");
    const char *empty_log = va_get_str(in, "empty_log", "");
    json_t *response = va_get(in, "response");
    /* _load_auxiliary_client() */
    json_t *lr = va_call(io, "load_auxiliary_client", json_object());
    if (va_raised(lr)) return lr;
    json_free(lr);
    /* if response is None: response = await async_call_llm(**call_kwargs) */
    if (!jpresent(response)) {
        json_t *ka = json_object();
        json_set(ka, "kwargs", copy_or_null(call_kwargs));
        response = va_call(io, "async_call_llm", ka);
        if (va_raised(response)) return response;
    } else {
        response = json_copy(response);
    }
    /* analysis = extract_content_or_reasoning(response) */
    json_t *ea = json_object();
    json_set(ea, "response", json_copy(response));
    json_t *analysis = va_call(io, "extract_content_or_reasoning", ea);
    if (va_raised(analysis)) { json_free(response); return analysis; }
    if (!jtruth(analysis)) {
        json_free(analysis);
        /* logger.warning(empty_log) — silent warning (not seamed) */
        (void)empty_log;
        json_t *ka = json_object();
        json_set(ka, "kwargs", copy_or_null(call_kwargs));
        json_t *resp2 = va_call(io, "async_call_llm", ka);
        if (va_raised(resp2)) { json_free(response); return resp2; }
        json_t *ea2 = json_object();
        json_set(ea2, "response", resp2);
        analysis = va_call(io, "extract_content_or_reasoning", ea2);
        json_free(response);
        return analysis;
    }
    json_free(response);
    return analysis;
}

/* ── _prepare_image ────────────────────────────────────────────────────── */
static json_t *va_path_join(json_t *dir, const char *name);
/* PoP: va_prepare_image @ tools/vision_tools.py:_prepare_image */
static json_t *va_prepare_image(va_io_t *io, json_t *in) {
    json_t *image_url = va_get(in, "image_url");
    json_t *task_id = va_get(in, "task_id");
    json_t *region = va_get(in, "region");
    bool validate_decode = json_get_bool(in, "validate_decode", false);
    /* from tools.image_source import ...; from tools.hosted_tool_policy import ...
     * if not host_feature_enabled("media_processing"): raise _ImagePrepError(...) */
    json_t *ha = json_object();
    json_set_hdr(ha, "name", "media_processing");
    json_t *hf = va_call(io, "host_feature_enabled", ha);
    if (va_raised(hf)) return hf;
    bool enabled = jtruth(hf);
    json_free(hf);
    if (!enabled) return va_raise("_ImagePrepError",
        "Image processing is not available to a hosted agent.");
    /* try: resolved = await resolve_image_source(image_url, ResolveContext(task_id=task_id))
     * except ImageResolutionError as exc: raise _ImagePrepError(str(exc)) from exc */
    json_t *rsa = json_object();
    json_set(rsa, "src", copy_or_null(image_url));
    json_set(rsa, "task_id", copy_or_null(task_id));
    json_t *resolved = va_call(io, "resolve_image_source", rsa);
    if (va_raised(resolved)) {
        if (strcmp(va_exc_kind(resolved), "ImageResolutionError") == 0) {
            json_t *e = va_raise("_ImagePrepError", va_exc_msg(resolved));
            json_free(resolved);
            return e;
        }
        return resolved;
    }
    /* temp_dir = _secure_cache_dir("cache/vision","temp_vision_images") */
    json_t *sca = json_object();
    json_set_hdr(sca, "new_subpath", "cache/vision");
    json_set_hdr(sca, "old_name", "temp_vision_images");
    json_t *temp_dir = va_call(io, "secure_cache_dir", sca);
    if (va_raised(temp_dir)) { json_free(resolved); return temp_dir; }
    /* path = temp_dir / f"temp_image_{uuid.uuid4()}.img" — uuid seam */
    json_t *ua = json_object();
    json_t *uuid = va_call(io, "uuid.uuid4", ua);
    if (va_raised(uuid)) { json_free(resolved); json_free(temp_dir); return uuid; }
    char *fname = va_cat3("temp_image_", sval(uuid), ".img");
    json_t *path = va_path_join(temp_dir, fname);
    free(fname); json_free(uuid); json_free(temp_dir);
    /* await asyncio.to_thread(_write_private_bytes, path, resolved.data) */
    json_t *data = va_get(resolved, "data");
    json_t *wa = json_object();
    json_set(wa, "path", json_copy(path));
    json_set(wa, "data", copy_or_null(data));
    json_t *wr = va_call(io, "write_private_bytes", wa);
    if (va_raised(wr)) { json_free(resolved); json_free(path); return wr; }
    json_free(wr);
    /* mime, size_bytes, crop_offset = resolved.mime, len(resolved.data), {} */
    json_t *mime = copy_or_null(va_get(resolved, "mime"));
    double size_bytes = (double)va_strlen(data);
    json_t *crop_offset = json_object();
    json_free(resolved);
    /* try: normalize / validate / crop ... except BaseException: _unlink_quietly(path); raise */
    json_t *exc = NULL;
    json_t *ret = NULL;
    /* normalized_path, mime, norm_err = await asyncio.to_thread(_normalize_to_supported_image, path, mime) */
    json_t *na = json_object();
    json_set(na, "path", json_copy(path));
    json_set(na, "mime", json_copy(mime));
    json_t *norm = va_call(io, "normalize_to_supported_image", na);
    if (va_raised(norm)) { exc = norm; goto cleanup; }
    json_t *normalized_path = va_get(norm, "normalized_path");
    json_t *norm_mime = va_get(norm, "mime");
    json_t *norm_err = va_get(norm, "norm_err");
    if (jtruth(norm_err) || !jpresent(normalized_path)) {
        const char *m = jtruth(norm_err) ? sval(norm_err) : "Image normalization failed.";
        exc = va_raise("_ImagePrepError", m);
        json_free(norm); goto cleanup;
    }
    json_free(mime);
    mime = copy_or_null(norm_mime);
    /* if normalized_path != path: _unlink_quietly(path); path=normalized_path; size=path.stat().st_size */
    if (strcmp(sval(normalized_path), sval(path)) != 0) {
        json_t *uqa = json_object();
        json_set(uqa, "path", json_copy(path));
        json_t *uq = va_call(io, "unlink_quietly", uqa);
        if (va_raised(uq)) { json_free(norm); exc = uq; goto cleanup; }
        json_free(uq);
        json_free(path);
        path = json_copy(normalized_path);
        json_t *sta = json_object();
        json_set(sta, "path", json_copy(path));
        json_t *st = va_call(io, "path.stat", sta);
        if (va_raised(st)) { json_free(norm); exc = st; goto cleanup; }
        size_bytes = va_get_num(st, "st_size", 0);
        json_free(st);
    }
    json_free(norm);
    /* if validate_decode: decode_error = await _run_encode_on_cpu_executor(_validate_raster_image_decodable, path, FRAME, PIXELS) */
    if (validate_decode) {
        json_t *ea = json_object();
        json_set_hdr(ea, "fn", "_validate_raster_image_decodable");
        json_t *aa = json_array();
        json_append(aa, json_copy(path));
        json_append(aa, json_number(VA_MAX_VALIDATED_FRAME_COUNT));
        json_append(aa, json_number(VA_MAX_VALIDATED_AGGREGATE_PIXELS));
        json_set(ea, "args", aa);
        json_set(ea, "kwargs", json_object());
        json_t *de = va_call(io, "run_encode_on_cpu_executor", ea);
        if (va_raised(de)) { exc = de; goto cleanup; }
        if (jtruth(de)) {
            exc = va_raise("_ImagePrepError", sval(de));
            json_free(de); goto cleanup;
        }
        json_free(de);
    }
    /* if region is not None: cropped_path, cropped_mime, crop_err = await asyncio.to_thread(_crop_image_region, path, region, offset_out=crop_offset) */
    if (jpresent(region)) {
        json_t *ca = json_object();
        json_set(ca, "path", json_copy(path));
        json_set(ca, "region", json_copy(region));
        json_set(ca, "offset_out", json_copy(crop_offset));
        json_t *crop = va_call(io, "crop_image_region", ca);
        if (va_raised(crop)) { exc = crop; goto cleanup; }
        json_t *cropped_path = va_get(crop, "cropped_path");
        json_t *cropped_mime = va_get(crop, "mime");
        json_t *crop_err = va_get(crop, "crop_err");
        if (jtruth(crop_err) || !jpresent(cropped_path)) {
            const char *m = jtruth(crop_err) ? sval(crop_err) : "Region crop failed.";
            exc = va_raise("_ImagePrepError", m);
            json_free(crop); goto cleanup;
        }
        json_t *uqa = json_object();
        json_set(uqa, "path", json_copy(path));
        json_t *uq = va_call(io, "unlink_quietly", uqa);
        if (va_raised(uq)) { json_free(crop); exc = uq; goto cleanup; }
        json_free(uq);
        json_free(path);
        path = json_copy(cropped_path);
        json_free(mime);
        mime = copy_or_null(cropped_mime);
        json_t *sta = json_object();
        json_set(sta, "path", json_copy(path));
        json_t *st = va_call(io, "path.stat", sta);
        if (va_raised(st)) { json_free(crop); exc = st; goto cleanup; }
        size_bytes = va_get_num(st, "st_size", 0);
        json_free(st);
        /* crop_offset was mutated in place by _crop_image_region (offset_out) */
        json_t *new_off = va_get(crop, "offset_out");
        if (j_is_obj(new_off)) {
            json_free(crop_offset);
            crop_offset = json_copy(new_off);
        }
        json_free(crop);
    }
    /* return _PreparedImage(path, mime, size_bytes, crop_offset) */
    ret = json_object();
    json_set(ret, "path", json_copy(path));
    json_set(ret, "mime", mime); mime = NULL;
    json_set(ret, "size_bytes", json_number(size_bytes));
    json_set(ret, "crop_offset", crop_offset); crop_offset = NULL;
cleanup:
    /* except BaseException: _unlink_quietly(path); raise */
    if (exc) {
        json_t *uqa = json_object();
        json_set(uqa, "path", json_copy(path));
        json_t *uq = va_call(io, "unlink_quietly", uqa);
        json_free(uq);
    }
    json_free(path);
    if (mime) json_free(mime);
    if (crop_offset) json_free(crop_offset);
    if (exc) return exc;
    return ret;
}

/* ── _vision_analyze_native ────────────────────────────────────────────── */
/* PoP: va_vision_analyze_native @ tools/vision_tools.py:_vision_analyze_native */
static json_t *va_vision_analyze_native(va_io_t *io, json_t *in) {
    json_t *image_url = va_get(in, "image_url");
    json_t *question = va_get(in, "question");
    json_t *task_id = va_get(in, "task_id");
    json_t *region = va_get(in, "region");
    /* if not isinstance(image_url,str) or not image_url.strip():
     *     return tool_error("image_url is required", success=False) */
    bool valid = j_is_str(image_url);
    if (valid) { char *s = va_strip(sval(image_url)); valid = s[0] != '\0'; free(s); }
    if (!valid) {
        json_t *ta = json_object();
        json_set_hdr(ta, "message", "image_url is required");
        json_set(ta, "success", json_bool(false));
        return va_call(io, "tool_error", ta);
    }
    /* already_native = _native_turn_duplicate(image_url, region) */
    json_t *da = json_object();
    json_set(da, "image_url", json_copy(image_url));
    json_set(da, "region", copy_or_null(region));
    json_t *dup = va_call(io, "native_turn_duplicate", da);
    if (va_raised(dup)) return dup;
    if (jpresent(dup)) return dup;
    json_free(dup);
    /* refusal = _repeat_refusal(image_url) */
    json_t *rfa = json_object();
    json_set(rfa, "image_url", json_copy(image_url));
    json_t *refusal = va_call(io, "repeat_refusal", rfa);
    if (va_raised(refusal)) return refusal;
    if (jpresent(refusal)) return refusal;
    json_free(refusal);
    /* reserved = _resolve_repeat_cap() > 0 */
    json_t *cap = va_call(io, "resolve_repeat_cap", json_object());
    if (va_raised(cap)) return cap;
    bool reserved = j_is_num(cap) && cap->num_val > 0;
    json_free(cap);
    json_t *prepared = NULL;
    bool embedded = false;
    json_t *ret = NULL;
    json_t *exc = NULL;
    json_t *image_data_url = NULL;
    json_t *scale_info = NULL;
    /* try: from tools.interrupt import is_interrupted; if is_interrupted(): return tool_error("Interrupted") */
    json_t *ir = va_call(io, "is_interrupted", json_object());
    if (va_raised(ir)) { exc = ir; goto finally; }
    if (jtruth(ir)) {
        json_free(ir);
        json_t *ta = json_object();
        json_set_hdr(ta, "message", "Interrupted");
        json_set(ta, "success", json_bool(false));
        ret = va_call(io, "tool_error", ta);
        if (va_raised(ret)) { exc = ret; ret = NULL; }
        goto finally;
    }
    json_free(ir);
    /* try: prepared = await _prepare_image(image_url, task_id, region, validate_decode=True)
     * except _ImagePrepError as exc: return tool_error(str(exc), success=False) */
    {
        json_t *pa = json_object();
        json_set(pa, "image_url", json_copy(image_url));
        json_set(pa, "task_id", copy_or_null(task_id));
        json_set(pa, "region", copy_or_null(region));
        json_set(pa, "validate_decode", json_bool(true));
        prepared = va_call(io, "prepare_image", pa);
        if (va_raised(prepared)) {
            if (strcmp(va_exc_kind(prepared), "_ImagePrepError") == 0) {
                json_t *ta = json_object();
                json_set_hdr(ta, "message", va_exc_msg(prepared));
                json_set(ta, "success", json_bool(false));
                json_free(prepared); prepared = NULL;
                ret = va_call(io, "tool_error", ta);
                if (va_raised(ret)) { exc = ret; ret = NULL; }
                goto finally;
            }
            exc = prepared; prepared = NULL; goto finally;
        }
    }
    /* image_data_url = await _run_encode_on_cpu_executor(_image_to_base64_data_url, prepared.path, mime_type=prepared.mime) */
    {
        json_t *ea = json_object();
        json_set_hdr(ea, "fn", "_image_to_base64_data_url");
        json_t *aa = json_array();
        json_append(aa, json_copy(va_get(prepared, "path")));
        json_set(ea, "args", aa);
        json_t *kk = json_object();
        json_set(kk, "mime_type", copy_or_null(va_get(prepared, "mime")));
        json_set(ea, "kwargs", kk);
        image_data_url = va_call(io, "run_encode_on_cpu_executor", ea);
        if (va_raised(image_data_url)) { exc = image_data_url; image_data_url = NULL; goto finally; }
    }
    scale_info = json_object();
    /* embed_target_bytes = _resolve_embed_target_bytes() */
    json_t *etb = va_call(io, "resolve_embed_target_bytes", json_object());
    if (va_raised(etb)) { exc = etb; goto finally; }
    double embed_target = j_is_num(etb) ? etb->num_val : 0;
    json_free(etb);
    /* _over_dims = await _run_encode_on_cpu_executor(_image_exceeds_dimension, prepared.path, _EMBED_MAX_DIMENSION) */
    json_t *over_dims = NULL;
    {
        json_t *ea = json_object();
        json_set_hdr(ea, "fn", "_image_exceeds_dimension");
        json_t *aa = json_array();
        json_append(aa, json_copy(va_get(prepared, "path")));
        json_append(aa, json_number(VA_EMBED_MAX_DIMENSION));
        json_set(ea, "args", aa);
        json_set(ea, "kwargs", json_object());
        over_dims = va_call(io, "run_encode_on_cpu_executor", ea);
        if (va_raised(over_dims)) { exc = over_dims; over_dims = NULL; goto finally; }
    }
    bool over = jtruth(over_dims);
    json_free(over_dims);
    /* if len(image_data_url) > embed_target_bytes or _over_dims: resize */
    if ((double)va_strlen(image_data_url) > embed_target || over) {
        json_t *ra = json_object();
        json_set(ra, "prepared", json_copy(prepared));
        json_set(ra, "scale_info", json_copy(scale_info));
        json_t *kk = json_object();
        json_set(kk, "max_base64_bytes", json_number(embed_target));
        json_set(kk, "max_dimension", json_number(VA_EMBED_MAX_DIMENSION));
        json_set(kk, "force_jpeg", json_bool(true));
        json_set(ra, "kwargs", kk);
        json_t *rz = va_call(io, "resize_prepared", ra);
        if (va_raised(rz)) { exc = rz; goto finally; }
        /* resize_prepared result {"data_url","scale_out"}; apply scale_out */
        json_t *new_url = va_get(rz, "data_url");
        json_t *scale_out = va_get(rz, "scale_out");
        if (j_is_obj(scale_out)) {
            for (size_t i = 0; i < scale_out->c.count; i++)
                json_set(scale_info, scale_out->c.keys[i], json_copy(scale_out->c.items[i]));
        }
        json_free(image_data_url);
        image_data_url = copy_or_null(new_url);
        json_free(rz);
        /* if len(image_data_url) > _MAX_BASE64_BYTES: return tool_error(_too_large_message(...)) */
        if ((double)va_strlen(image_data_url) > VA_MAX_BASE64_BYTES) {
            json_t *tla = json_object();
            json_set(tla, "image_data_url", json_copy(image_data_url));
            json_t *tlm = va_call(io, "too_large_message", tla);
            if (va_raised(tlm)) { exc = tlm; goto finally; }
            json_t *ta = json_object();
            json_set(ta, "message", tlm);
            json_set(ta, "success", json_bool(false));
            ret = va_call(io, "tool_error", ta);
            if (va_raised(ret)) { exc = ret; ret = NULL; }
            goto finally;
        }
    }
    embedded = true;
    /* if not reserved: _record_embed(image_url) */
    if (!reserved) {
        json_t *rea = json_object();
        json_set(rea, "image_url", json_copy(image_url));
        json_t *re = va_call(io, "record_embed", rea);
        if (va_raised(re)) { exc = re; goto finally; }
        json_free(re);
    }
    /* return _build_native_vision_tool_result(image_url=, question=, image_data_url=,
     *     image_size_bytes=prepared.size_bytes, scale_note=_build_scale_note(_scale_info or None, prepared.crop_offset or None)) */
    {
        json_t *bsa = json_object();
        json_set(bsa, "scale_info", jtruth(scale_info) ? json_copy(scale_info) : json_null());
        json_t *co = va_get(prepared, "crop_offset");
        json_set(bsa, "crop_offset", jtruth(co) ? json_copy(co) : json_null());
        json_t *note = va_call(io, "build_scale_note", bsa);
        if (va_raised(note)) { exc = note; goto finally; }
        json_t *ba = json_object();
        json_set(ba, "image_url", json_copy(image_url));
        json_set(ba, "question", copy_or_null(question));
        json_set(ba, "image_data_url", json_copy(image_data_url));
        json_set(ba, "image_size_bytes", copy_or_null(va_get(prepared, "size_bytes")));
        json_set(ba, "scale_note", note);
        ret = va_call(io, "build_native_vision_tool_result", ba);
        if (va_raised(ret)) { exc = ret; ret = NULL; }
    }
finally:
    /* except Exception as exc: logger.warning(...); return tool_error(f"Native vision failed: {exc}") */
    if (exc && va_exc_is_exception(exc)) {
        char *msg = va_cat2("Native vision failed: ", va_exc_msg(exc));
        json_t *ta = json_object();
        json_set_hdr(ta, "message", msg);
        json_set(ta, "success", json_bool(false));
        free(msg);
        json_free(exc); exc = NULL;
        json_t *tr = va_call(io, "tool_error", ta);
        ret = tr;
        if (va_raised(ret)) { exc = ret; ret = NULL; }
    }
    /* finally: if reserved and not embedded: _release_embed(image_url);
     *          if prepared is not None: _unlink_quietly(prepared.path) */
    if (reserved && !embedded) {
        json_t *rea = json_object();
        json_set(rea, "image_url", json_copy(image_url));
        json_t *re = va_call(io, "release_embed", rea);
        json_free(re);
    }
    if (prepared) {
        json_t *uqa = json_object();
        json_set(uqa, "path", copy_or_null(va_get(prepared, "path")));
        json_t *uq = va_call(io, "unlink_quietly", uqa);
        json_free(uq);
    }
    if (scale_info) json_free(scale_info);
    if (image_data_url) json_free(image_data_url);
    if (prepared) json_free(prepared);
    if (exc) return exc;
    return ret ? ret : json_null();
}

/* ── _run_analysis skeleton (stage callback) ───────────────────────────── */
typedef json_t *(*va_stage_cb)(va_io_t *io, json_t *user_prompt,
                               json_t *debug_call_data, json_t *temp_paths, void *ctx);

/* finish(result): _debug.log_call(tool_name, dcd); _debug.save(); json.dumps(result) */
static json_t *va_finish(va_io_t *io, const char *tool_name, json_t *dcd, json_t *result) {
    json_t *la = json_object();
    json_set_hdr(la, "tool_name", tool_name);
    json_set(la, "debug_call_data", json_copy(dcd));
    json_t *lr = va_call(io, "debug.log_call", la);
    if (va_raised(lr)) return lr;
    json_free(lr);
    json_t *sr = va_call(io, "debug.save", json_object());
    if (va_raised(sr)) return sr;
    json_free(sr);
    json_t *ja = json_object();
    json_set(ja, "obj", json_copy(result));
    json_set(ja, "indent", json_number(2));
    json_set(ja, "ensure_ascii", json_bool(false));
    return va_call(io, "json.dumps", ja);
}

static const char *va_match_template(const char *kind, const char *err_str) {
    if (va_any_hint(VA_BILLING_HINTS, sizeof(VA_BILLING_HINTS)/sizeof(char*), err_str))
        return VA_IMAGE_TPL_BILLING;
    if (strcmp(kind, "image") == 0) {
        if (va_any_hint(VA_IMAGE_CAP_HINTS, sizeof(VA_IMAGE_CAP_HINTS)/sizeof(char*), err_str))
            return VA_IMAGE_TPL_CAP;
        if (va_any_hint(VA_IMAGE_FMT_HINTS, sizeof(VA_IMAGE_FMT_HINTS)/sizeof(char*), err_str))
            return VA_IMAGE_TPL_FMT;
    } else {
        if (va_any_hint(VA_VIDEO_CAP_HINTS, sizeof(VA_VIDEO_CAP_HINTS)/sizeof(char*), err_str))
            return VA_VIDEO_TPL_CAP;
        if (va_any_hint(VA_SIZE_HINTS, sizeof(VA_SIZE_HINTS)/sizeof(char*), err_str))
            return VA_VIDEO_TPL_SIZE;
    }
    return NULL; /* default template */
}

static json_t *va_run_analysis_core(va_io_t *io, const char *kind, json_t *source,
                                    json_t *user_prompt, json_t *model,
                                    va_stage_cb stage, void *sctx) {
    const char *tool_name;
    if (strcmp(kind, "image") == 0) tool_name = "vision_analyze_tool";
    else if (strcmp(kind, "video") == 0) tool_name = "video_analyze_tool";
    else {
        char *km = va_cat3("'", kind, "'");
        json_t *e = va_raise("KeyError", km);
        free(km);
        return e;
    }
    /* if not isinstance(user_prompt,str): user_prompt = str(user_prompt) if not None else "" */
    json_t *up;
    if (j_is_str(user_prompt)) up = json_copy(user_prompt);
    else if (jpresent(user_prompt)) { char *s = va_pystr(user_prompt); up = json_string(s); free(s); }
    else up = json_string("");
    /* debug_call_data = _debug_call_data(kind, source, user_prompt, model) — BEFORE try */
    json_t *dca = json_object();
    json_set_hdr(dca, "kind", kind);
    json_set(dca, "source", copy_or_null(source));
    json_set(dca, "user_prompt", json_copy(up));
    json_set(dca, "model", copy_or_null(model));
    json_t *dcd = va_call(io, "debug_call_data", dca);
    if (va_raised(dcd)) { json_free(up); return dcd; }
    json_t *temp_paths = json_array();
    json_t *ret = NULL, *exc = NULL;
    /* try: is_interrupted */
    json_t *ir = va_call(io, "is_interrupted", json_object());
    if (va_raised(ir)) { exc = ir; goto handle; }
    if (jtruth(ir)) {
        json_free(ir);
        json_t *ta = json_object();
        json_set_hdr(ta, "message", "Interrupted");
        json_set(ta, "success", json_bool(false));
        ret = va_call(io, "tool_error", ta);
        if (va_raised(ret)) { exc = ret; ret = NULL; }
        goto handle;
    }
    json_free(ir);
    /* analysis, scale_note = await stage(user_prompt, debug_call_data, temp_paths) */
    json_t *stage_ret = stage(io, up, dcd, temp_paths, sctx);
    if (va_raised(stage_ret)) { exc = stage_ret; goto handle; }
    json_t *analysis = json_copy(json_get(stage_ret, 0));
    json_t *scale_note = json_copy(json_get(stage_ret, 1));
    json_free(stage_ret);
    double analysis_length = jtruth(analysis) ? (double)va_strlen(analysis) : 0;
    /* analysis = analysis or default */
    if (!jtruth(analysis)) {
        json_free(analysis);
        char *m = va_cat3("There was a problem with the request and the ", kind,
                          " could not be analyzed.");
        analysis = json_string(m); free(m);
    }
    /* result = {"success":True,"analysis": f"[{scale_note}] {analysis}" if scale_note else analysis} */
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    if (jtruth(scale_note)) {
        char *a = va_cat3("[", sval(scale_note), "] ");
        char *full = va_cat2(a, sval(analysis));
        free(a);
        json_set_hdr(result, "analysis", full);
        free(full);
        json_set(result, "scale_note", json_copy(scale_note));
    } else {
        json_set(result, "analysis", json_copy(analysis));
    }
    /* debug_call_data.update(success=True, analysis_length=analysis_length) */
    json_set(dcd, "success", json_bool(true));
    json_set(dcd, "analysis_length", json_number(analysis_length));
    ret = va_finish(io, tool_name, dcd, result);
    json_free(result);
    if (va_raised(ret)) { exc = ret; ret = NULL; }
    json_free(analysis);
    json_free(scale_note);
handle:
    if (exc) {
        if (va_exc_is_exception(exc)) {
            /* error_msg = f"Error analyzing {kind}: {e!s}" */
            char *error_msg = va_cat3("Error analyzing ", kind, ": ");
            char *em2 = va_cat2(error_msg, va_exc_msg(exc));
            free(error_msg);
            char *err_str = va_lower(va_exc_msg(exc));
            const char *tpl = va_match_template(kind, err_str);
            char *analysis_s;
            if (tpl) analysis_s = va_format_tpl(tpl, va_exc_msg(exc), model);
            else {
                char *base = va_cat3("There was a problem with the request and the ", kind,
                                     " could not be analyzed. Error: ");
                analysis_s = va_cat2(base, va_exc_msg(exc));
                free(base);
            }
            free(err_str);
            json_set(dcd, "error", json_string(em2));
            json_t *err_result = json_object();
            json_set(err_result, "success", json_bool(false));
            json_set_hdr(err_result, "error", em2);
            json_set_hdr(err_result, "analysis", analysis_s);
            free(em2); free(analysis_s);
            json_free(exc); exc = NULL;
            ret = va_finish(io, tool_name, dcd, err_result);
            json_free(err_result);
            /* a raise from the except-path finish propagates */
        } else {
            ret = exc; exc = NULL;   /* BaseException passes through */
        }
    }
    /* finally: for path in temp_paths: if path and path.exists(): try unlink ... */
    for (size_t i = 0; i < json_len(temp_paths); i++) {
        json_t *path = json_get(temp_paths, i);
        if (!jtruth(path)) continue;
        json_t *ea = json_object();
        json_set(ea, "path", json_copy(path));
        json_t *exists = va_call(io, "path.exists", ea);
        if (va_raised(exists)) { ret = exists; break; }
        bool ex = jtruth(exists);
        json_free(exists);
        if (!ex) continue;
        json_t *ua = json_object();
        json_set(ua, "path", json_copy(path));
        json_set(ua, "missing_ok", json_bool(false));
        json_t *ur = va_call(io, "path.unlink", ua);
        if (va_raised(ur)) {
            if (va_exc_is_exception(ur)) { json_free(ur); /* logger.warning silent */ }
            else { ret = ur; break; }
        } else {
            json_free(ur);
            /* logger.debug("Cleaned up temporary %s file", kind) — inside the
             * same try; an Exception raise here is swallowed (logger.warning). */
            json_t *lda = json_object();
            json_set_hdr(lda, "fmt", "Cleaned up temporary %s file");
            json_t *laa = json_array();
            json_append(laa, json_string(kind));
            json_set(lda, "args", laa);
            json_t *ldr = va_call(io, "logger.debug", lda);
            if (va_raised(ldr)) {
                if (va_exc_is_exception(ldr)) json_free(ldr);
                else { ret = ldr; break; }
            } else json_free(ldr);
        }
    }
    json_free(temp_paths);
    json_free(dcd);
    json_free(up);
    return ret ? ret : json_null();
}

/* direct _run_analysis: stage is the scripted "stage" seam; stage_paths models
 * the temp_paths.append the live stage performs. */
static json_t *va_stage_seam_cb(va_io_t *io, json_t *up, json_t *dcd, json_t *tp, void *ctx) {
    json_t *in = (json_t *)ctx;
    json_t *args = json_object();
    json_set(args, "user_prompt", json_copy(up));
    json_set(args, "debug_call_data", json_copy(dcd));
    json_set(args, "temp_paths", json_copy(tp));
    json_t *r = va_call(io, "stage", args);
    if (va_raised(r)) return r;
    json_t *paths = va_get(in, "stage_paths");
    if (j_is_arr(paths))
        for (size_t i = 0; i < json_len(paths); i++) json_append(tp, json_copy(json_get(paths, i)));
    return r;
}

/* PoP: va_run_analysis @ tools/vision_tools.py:_run_analysis */
static json_t *va_run_analysis(va_io_t *io, json_t *in) {
    const char *kind = va_get_str(in, "kind", "");
    return va_run_analysis_core(io, kind, va_get(in, "source"), va_get(in, "user_prompt"),
                                va_get(in, "model"), va_stage_seam_cb, in);
}

/* vision_analyze_tool's real stage closure (image path). */
typedef struct { json_t *in; } va_img_stage_ctx_t;

static json_t *va_image_stage_cb(va_io_t *io, json_t *up, json_t *dcd, json_t *tp, void *vctx) {
    va_img_stage_ctx_t *c = (va_img_stage_ctx_t *)vctx;
    json_t *in = c->in;
    json_t *image_url = va_get(in, "image_url");
    json_t *task_id = va_get(in, "task_id");
    json_t *region = va_get(in, "region");
    json_t *model = va_get(in, "model");
    /* prepared = await _prepare_image(image_url, task_id, region, validate_decode=False) */
    json_t *pa = json_object();
    json_set(pa, "image_url", copy_or_null(image_url));
    json_set(pa, "task_id", copy_or_null(task_id));
    json_set(pa, "region", copy_or_null(region));
    json_set(pa, "validate_decode", json_bool(false));
    json_t *prepared = va_call(io, "prepare_image", pa);
    if (va_raised(prepared)) return prepared;
    /* temp_paths.append(prepared.path) */
    json_append(tp, json_copy(va_get(prepared, "path")));
    /* image_data_url = await _run_encode_on_cpu_executor(_image_to_base64_data_url, prepared.path, mime_type=prepared.mime) */
    json_t *ea = json_object();
    json_set_hdr(ea, "fn", "_image_to_base64_data_url");
    json_t *aa = json_array();
    json_append(aa, json_copy(va_get(prepared, "path")));
    json_set(ea, "args", aa);
    json_t *kk = json_object();
    json_set(kk, "mime_type", copy_or_null(va_get(prepared, "mime")));
    json_set(ea, "kwargs", kk);
    json_t *image_data_url = va_call(io, "run_encode_on_cpu_executor", ea);
    if (va_raised(image_data_url)) { json_free(prepared); return image_data_url; }
    json_t *scale_info = json_object();
    /* if len(image_data_url) > _MAX_BASE64_BYTES: resize; if still too large raise ValueError */
    if ((double)va_strlen(image_data_url) > VA_MAX_BASE64_BYTES) {
        json_t *ra = json_object();
        json_set(ra, "prepared", json_copy(prepared));
        json_set(ra, "scale_info", json_copy(scale_info));
        json_set(ra, "kwargs", json_object());
        json_t *rz = va_call(io, "resize_prepared", ra);
        if (va_raised(rz)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); return rz; }
        json_t *so = va_get(rz, "scale_out");
        if (j_is_obj(so)) for (size_t i = 0; i < so->c.count; i++)
            json_set(scale_info, so->c.keys[i], json_copy(so->c.items[i]));
        json_free(image_data_url);
        image_data_url = copy_or_null(va_get(rz, "data_url"));
        json_free(rz);
        if ((double)va_strlen(image_data_url) > VA_MAX_BASE64_BYTES) {
            json_t *tla = json_object();
            json_set(tla, "image_data_url", json_copy(image_data_url));
            json_t *tlm = va_call(io, "too_large_message", tla);
            if (va_raised(tlm)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); return tlm; }
            json_t *e = va_raise("ValueError", sval(tlm));
            json_free(tlm); json_free(prepared); json_free(image_data_url); json_free(scale_info);
            return e;
        }
    }
    /* debug_call_data["image_size_bytes"] = prepared.size_bytes */
    json_set(dcd, "image_size_bytes", copy_or_null(va_get(prepared, "size_bytes")));
    /* messages = _media_messages(prompt, "image_url", image_data_url) */
    json_t *ma = json_object();
    json_set(ma, "user_prompt", json_copy(up));
    json_set_hdr(ma, "part_type", "image_url");
    json_set(ma, "data_url", json_copy(image_data_url));
    json_t *messages = va_call(io, "media_messages", ma);
    if (va_raised(messages)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); return messages; }
    /* call_kwargs = _aux_call_kwargs(messages, model, 120.0) */
    json_t *ka = json_object();
    json_set(ka, "messages", json_copy(messages));
    json_set(ka, "model", copy_or_null(model));
    json_set(ka, "default_timeout", json_number(120.0));
    json_set(ka, "min_timeout", json_null());
    json_t *call_kwargs = va_call(io, "aux_call_kwargs", ka);
    if (va_raised(call_kwargs)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); return call_kwargs; }
    /* _load_auxiliary_client() */
    json_t *lr = va_call(io, "load_auxiliary_client", json_object());
    if (va_raised(lr)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return lr; }
    json_free(lr);
    /* try: response = await async_call_llm(**call_kwargs)
     * except Exception as _api_err: if not (size_error and len>RESIZE_TARGET): raise;
     *   resize; messages[0].content[1].image_url.url = image_data_url; response = await async_call_llm(**call_kwargs) */
    json_t *response = NULL;
    {
        json_t *a1 = json_object();
        json_set(a1, "kwargs", json_copy(call_kwargs));
        response = va_call(io, "async_call_llm", a1);
        if (va_raised(response)) {
            if (!va_exc_is_exception(response)) {
                json_free(prepared); json_free(image_data_url); json_free(scale_info);
                json_free(messages); json_free(call_kwargs);
                return response;
            }
            json_t *sia = json_object();
            json_t *espec = json_object();
            json_set_hdr(espec, "__raise__", va_exc_kind(response));
            json_set_hdr(espec, "message", va_exc_msg(response));
            json_set(sia, "error", espec);
            json_t *sie = va_call(io, "is_image_size_error", sia);
            if (va_raised(sie)) { json_free(response); json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return sie; }
            bool size_err = jtruth(sie);
            json_free(sie);
            if (!(size_err && (double)va_strlen(image_data_url) > VA_RESIZE_TARGET_BYTES)) {
                json_free(prepared); json_free(image_data_url); json_free(scale_info);
                json_free(messages); json_free(call_kwargs);
                return response;   /* re-raise */
            }
            json_free(response);
            /* resize again */
            json_t *ra = json_object();
            json_set(ra, "prepared", json_copy(prepared));
            json_set(ra, "scale_info", json_copy(scale_info));
            json_set(ra, "kwargs", json_object());
            json_t *rz = va_call(io, "resize_prepared", ra);
            if (va_raised(rz)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return rz; }
            json_t *so = va_get(rz, "scale_out");
            if (j_is_obj(so)) for (size_t i = 0; i < so->c.count; i++)
                json_set(scale_info, so->c.keys[i], json_copy(so->c.items[i]));
            json_free(image_data_url);
            image_data_url = copy_or_null(va_get(rz, "data_url"));
            json_free(rz);
            /* messages[0]["content"][1]["image_url"]["url"] = image_data_url */
            if (j_is_arr(messages) && json_len(messages) > 0) {
                json_t *m0 = json_get(messages, 0);
                json_t *content = va_get(m0, "content");
                if (j_is_arr(content) && json_len(content) > 1) {
                    json_t *part1 = json_get(content, 1);
                    json_t *iu = va_get(part1, "image_url");
                    if (j_is_obj(iu)) json_set(iu, "url", json_copy(image_data_url));
                }
            }
            json_t *a2 = json_object();
            json_set(a2, "kwargs", json_copy(call_kwargs));
            response = va_call(io, "async_call_llm", a2);
            if (va_raised(response)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return response; }
        }
    }
    /* analysis = await _call_vision_llm(call_kwargs, "Vision LLM returned empty content, retrying once", response) */
    json_t *cva = json_object();
    json_set(cva, "call_kwargs", json_copy(call_kwargs));
    json_set_hdr(cva, "empty_log", "Vision LLM returned empty content, retrying once");
    json_set(cva, "response", json_copy(response));
    json_t *analysis = va_call(io, "call_vision_llm", cva);
    json_free(response);
    if (va_raised(analysis)) { json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return analysis; }
    /* return analysis, _build_scale_note(_scale_info or None, prepared.crop_offset or None) */
    json_t *bsa = json_object();
    json_set(bsa, "scale_info", jtruth(scale_info) ? json_copy(scale_info) : json_null());
    json_t *co = va_get(prepared, "crop_offset");
    json_set(bsa, "crop_offset", jtruth(co) ? json_copy(co) : json_null());
    json_t *note = va_call(io, "build_scale_note", bsa);
    if (va_raised(note)) { json_free(analysis); json_free(prepared); json_free(image_data_url); json_free(scale_info); json_free(messages); json_free(call_kwargs); return note; }
    json_t *out = json_array();
    json_append(out, analysis);
    json_append(out, note);
    json_free(prepared); json_free(image_data_url); json_free(scale_info);
    json_free(messages); json_free(call_kwargs);
    return out;
}

/* PoP: va_vision_analyze_tool @ tools/vision_tools.py:vision_analyze_tool */
static json_t *va_vision_analyze_tool(va_io_t *io, json_t *in) {
    va_img_stage_ctx_t c = { in };
    return va_run_analysis_core(io, "image", va_get(in, "image_url"),
                                va_get(in, "user_prompt"), va_get(in, "model"),
                                va_image_stage_cb, &c);
}

/* PoP: va_handle_vision_analyze @ tools/vision_tools.py:_handle_vision_analyze */
static json_t *va_handle_vision_analyze(va_io_t *io, json_t *in) {
    json_t *args = va_get(in, "args");
    json_t *kw = va_get(in, "kw");
    /* args.get("image_url", ""), args.get("question", ""), args.get("region") */
    json_t *image_url = json_has(args, "image_url") ? json_copy(va_get(args, "image_url")) : json_string("");
    json_t *question = json_has(args, "question") ? json_copy(va_get(args, "question")) : json_string("");
    json_t *region = va_get(args, "region");
    json_t *task_id = va_get(kw, "task_id");
    /* if _should_use_native_vision_fast_path(): return await _vision_analyze_native(...) */
    json_t *fp = va_call(io, "should_use_native_vision_fast_path", json_object());
    if (va_raised(fp)) { json_free(image_url); json_free(question); return fp; }
    bool native = jtruth(fp);
    json_free(fp);
    if (native) {
        json_t *na = json_object();
        json_set(na, "image_url", json_copy(image_url));
        json_set(na, "question", json_copy(question));
        json_set(na, "task_id", copy_or_null(task_id));
        json_set(na, "region", copy_or_null(region));
        json_free(image_url); json_free(question);
        return va_call(io, "vision_analyze_native", na);
    }
    /* full_prompt = "Fully describe ... question:\n\n{question}" */
    char *full_prompt = va_cat2(
        "Fully describe and explain everything about this image, then answer the "
        "following question:\n\n", sval(question));
    /* model = _configured_aux_model(("vision",), ("AUXILIARY_VISION_MODEL",)) */
    json_t *ca = json_object();
    json_t *secs = json_array(); json_append(secs, json_string("vision"));
    json_t *envs = json_array(); json_append(envs, json_string("AUXILIARY_VISION_MODEL"));
    json_set(ca, "sections", secs);
    json_set(ca, "env_vars", envs);
    json_t *model = va_call(io, "configured_aux_model", ca);
    if (va_raised(model)) { free(full_prompt); json_free(image_url); json_free(question); return model; }
    /* return await vision_analyze_tool(image_url, full_prompt, model, task_id=, region=) */
    json_t *va = json_object();
    json_set(va, "image_url", image_url);
    json_set_hdr(va, "user_prompt", full_prompt);
    json_set(va, "model", model);
    json_set(va, "task_id", copy_or_null(task_id));
    json_set(va, "region", copy_or_null(region));
    free(full_prompt); json_free(question);
    return va_call(io, "vision_analyze_tool", va);
}

/* ── path join helper (Path / name, POSIX) ─────────────────────────────── */
static json_t *va_path_join(json_t *dir, const char *name) {
    const char *d = sval(dir);
    size_t dl = strlen(d);
    bool need_sep = dl > 0 && d[dl - 1] != '/';
    char *r = (char *)malloc(dl + strlen(name) + 2);
    if (!r) return json_string(name);
    strcpy(r, d);
    if (need_sep) strcat(r, "/");
    strcat(r, name);
    json_t *out = json_string(r);
    free(r);
    return out;
}

/* ── dispatch ──────────────────────────────────────────────────────────── */
typedef json_t *(*va_fn_t)(va_io_t *, json_t *);
typedef struct { const char *func; va_fn_t fn; } va_case_t;
static const va_case_t VA_CASES[] = {
    { "_media_messages", va_media_messages },
    { "_debug_call_data", va_debug_call_data },
    { "_too_large_message", va_too_large_message },
    { "_unsupported_video_format", va_unsupported_video_format },
    { "_build_native_vision_tool_result", va_build_native_vision_tool_result },
    { "_cfg_auxiliary", va_cfg_auxiliary },
    { "_read_vision_setting", va_read_vision_setting },
    { "_managed_install", va_managed_install },
    { "_secure_cache_dir", va_secure_cache_dir },
    { "_write_private_bytes", va_write_private_bytes },
    { "_import_pillow_for_resize", va_import_pillow_for_resize },
    { "_accepts_tool_result_images", va_accepts_tool_result_images },
    { "_native_tool_result_images", va_native_tool_result_images },
    { "_should_use_native_vision_fast_path", va_should_use_native_vision_fast_path },
    { "_unlink_quietly", va_unlink_quietly },
    { "_aux_call_kwargs", va_aux_call_kwargs },
    { "_configured_aux_model", va_configured_aux_model },
    { "_load_auxiliary_client", va_load_auxiliary_client },
    { "check_video_requirements", va_check_video_requirements },
    { "_validate_image_url_async", va_validate_image_url_async },
    { "_ssrf_redirect_guard", va_ssrf_redirect_guard },
    { "_call_vision_llm", va_call_vision_llm },
    { "_prepare_image", va_prepare_image },
    { "_vision_analyze_native", va_vision_analyze_native },
    { "_run_analysis", va_run_analysis },
    { "vision_analyze_tool", va_vision_analyze_tool },
    { "_handle_vision_analyze", va_handle_vision_analyze },
};
#define VA_CASE_COUNT (sizeof(VA_CASES) / sizeof(VA_CASES[0]))

static json_t *va_dispatch(va_io_t *io, const char *func, json_t *in) {
    for (size_t i = 0; i < VA_CASE_COUNT; i++)
        if (strcmp(VA_CASES[i].func, func) == 0) return VA_CASES[i].fn(io, in);
    return va_raise("ValueError", "unknown func");
}

#endif /* VISION_ANALYSIS_FLOW_C */