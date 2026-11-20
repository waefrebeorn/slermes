/* pet_atlas_flow2.c — FLOW port of the UNCLAIMED remainder of
 * agent/pet/generate/atlas.py (pet_atlas_flow2). A CRLF-aware PoP grep of
 * agent/pet/generate/atlas.py function names over src/ found 33 claimed
 * (src/pet/atlas.c + src/agent/pet/generate/atlas.c) of 42 gaps; the 16
 * functions below carry NO PoP claim and are the only ones annotated here.
 * Claimed siblings and every PIL operation ride the injected universal seam
 * pa2_call, which the harness scripts and records as an ordered trace the
 * oracle diffs against LIVE atlas.py under the same scripts.
 *
 * Representation contract (shared with tests/oracle_pet_atlas_flow2.py):
 *   - None -> JSON null; image/channel handles are opaque JSON objects whose
 *     scalar fields (size, width, height, bbox, handle) are read directly on
 *     BOTH sides (free reads, NOT trace events).
 *   - every PIL method / claimed sibling is a scripted seam pa2_call(name,
 *     args); {"$seq":[...]} per-call queue, {"__raise__":cls,...} raises,
 *     absent -> null. None-valued seam args are dropped on BOTH sides.
 *   - an uncaught exception -> {"__raise__":cls}; an exception used as a seam
 *     arg projects to {"__raise__":cls,"msg":...}.
 *   - FULL builtin exception hierarchy: `except Exception` never catches the
 *     BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit).
 *   - Python round() == banker's rounding: pa2_round uses nearbyint().
 *
 * libjson-only. Self-contained: the harness #includes this file.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + primitives ──────────────────────────────────────────────── */
typedef struct pa2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *state;     /* owned: mutable state window (unused -> {}) */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} pa2_ctx;

static bool pa2_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool pa2_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool pa2_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pa2_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pa2_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pa2_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool pa2_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool pa2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *pa2_jstr(const json_t *v) {
    return (pa2_str(v) && v->str_val) ? v->str_val : "";
}
static double pa2_jnum(const json_t *v, double d) {
    return pa2_num(v) ? v->num_val : d;
}
static json_t *pa2_get(const json_t *obj, const char *key) {
    return pa2_obj(obj) ? json_obj_get(obj, key) : NULL;
}
static int pa2_round(double x) { return (int)nearbyint(x); }
static int __cdecl pa2_QNUM(const void *a, const void *b) {
    double x = (*(json_t *const *)a)->num_val, y = (*(json_t *const *)b)->num_val;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

/* ── exception plumbing: FULL builtin hierarchy ────────────────────────── */
static bool pa2_pending(const pa2_ctx *c) { return c && c->exc != NULL; }
static void pa2_clear(pa2_ctx *c) {
    if (c && c->exc) { json_free(c->exc); c->exc = NULL; }
}
static void pa2_raise(pa2_ctx *c, const char *cls, const char *msg) {
    if (!c) return;
    pa2_clear(c);
    c->exc = json_object();
    json_set(c->exc, "__raise__", json_string(cls));
    json_set(c->exc, "msg", json_string(msg ? msg : ""));
}
static const char *pa2_exc_class(const pa2_ctx *c) {
    return json_get_str(c->exc, "__raise__", "");
}
static const char *pa2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"MemoryError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pa2_eq(T[i].c, c)) return T[i].p;
    return "Exception";
}
static bool pa2_exc_matches(const pa2_ctx *c, const char *const *classes, size_t n) {
    if (!pa2_pending(c)) return false;
    const char *cls = pa2_exc_class(c);
    for (int d = 0; d < 32; d++) {
        for (size_t i = 0; i < n; i++) if (pa2_eq(cls, classes[i])) return true;
        const char *p = pa2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool pa2_exc_is_exception(const pa2_ctx *c) {
    static const char *E[] = {"Exception"};
    return pa2_exc_matches(c, E, 1);
}
static json_t *pa2_exc_arg(const pa2_ctx *c) {
    json_t *a = json_object();
    json_set(a, "__raise__", json_string(pa2_exc_class(c)));
    json_set(a, "msg", json_string(json_get_str(c->exc, "msg", "")));
    return a;
}

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *pa2_script_result(pa2_ctx *c, const json_t *s) {
    if (!s || s->type == JSON_NULL) return NULL;
    if (s->type == JSON_OBJECT) {
        json_t *r = json_obj_get(s, "__raise__");
        if (r && r->type == JSON_STRING) {
            pa2_clear(c);
            c->exc = json_copy(s);
            return NULL;
        }
    }
    return json_copy(s);
}
/* args ownership transferred; result NEW. */
static json_t *pa2_call(pa2_ctx *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = c->io ? json_obj_get(c->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(c->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                pa2_raise(c, "RuntimeError", "script exhausted: <name>");
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(c->exc, "msg", json_string(msg));
                chosen = c->exc;
            } else {
                json_set(c->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != c->exc) ret = pa2_script_result(c, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (c->exc && (chosen == c->exc ||
        (chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))))
        json_set(ev, "ret", json_copy(c->exc));
    else
        json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(c->trace, ev);
    return ret;
}
static void pa2_arg(json_t *args, const char *key, const json_t *v) {
    if (!v || v->type == JSON_NULL) return;
    json_set(args, key, json_copy(v));
}
static void pa2_arg_i(json_t *args, const char *key, int v) {
    json_set(args, key, json_number((double)v));
}
static void pa2_arg_str(json_t *args, const char *key, const char *v) {
    if (!v) return;
    json_set(args, key, json_string(v));
}
static json_t *pa2_size_of(const json_t *img) {
    json_t *s = pa2_get(img, "size");
    if (pa2_arr(s) && s->c.count >= 2) return json_copy(s);
    json_t *a = json_array();
    json_append(a, json_number(pa2_jnum(pa2_get(img, "width"), 0)));
    json_append(a, json_number(pa2_jnum(pa2_get(img, "height"), 0)));
    return a;
}
static void pa2_box_args(json_t *args, const char *key, int l, int t, int r, int b) {
    json_t *a = json_array();
    json_append(a, json_number(l)); json_append(a, json_number(t));
    json_append(a, json_number(r)); json_append(a, json_number(b));
    json_set(args, key, a);
}
static json_t *pa2_box_from(const json_t *v, int i) {
    return (pa2_arr(v) && i < (int)v->c.count) ? v->c.items[i] : NULL;
}

/* ── ported functions (all UNCLAIMED at claim time) ────────────────────── */

/* PoP: pa2_median @ agent/pet/generate/atlas.py:_median */
static json_t *pa2_median(pa2_ctx *c, json_t *in) {
    (void)c;
    json_t *vals = pa2_get(in, "values");
    if (!pa2_arr(vals) || vals->c.count == 0) {
        pa2_raise(c ? c : NULL, "IndexError", "list index out of range");
        return NULL;
    }
    size_t n = vals->c.count;
    json_t *cp = json_copy(vals);
    qsort(cp->c.items, n, sizeof(json_t *), pa2_QNUM);
    json_t *ret = json_copy(cp->c.items[n / 2]);
    json_free(cp);
    return ret;
}

/* PoP: pa2_median_box_size @ agent/pet/generate/atlas.py:_median_box_size */
static json_t *pa2_median_box_size(pa2_ctx *c, json_t *in) {
    json_t *boxes = pa2_get(in, "boxes");
    json_t *ws = json_array(), *hs = json_array();
    if (pa2_arr(boxes)) {
        for (size_t i = 0; i < boxes->c.count; i++) {
            json_t *b = boxes->c.items[i];
            json_append(ws, json_number(pa2_jnum(pa2_box_from(b, 2), 0) - pa2_jnum(pa2_box_from(b, 0), 0)));
            json_append(hs, json_number(pa2_jnum(pa2_box_from(b, 3), 0) - pa2_jnum(pa2_box_from(b, 1), 0)));
        }
    }
    json_t *wi = json_object(), *hi = json_object();
    json_set(wi, "values", ws); json_set(hi, "values", hs);
    json_t *mw = pa2_median(c, wi), *mh = pa2_median(c, hi);
    json_t *ret = json_array();
    json_append(ret, json_number(fmax(1.0, pa2_jnum(mw, 0))));
    json_append(ret, json_number(fmax(1.0, pa2_jnum(mh, 0))));
    json_free(mw); json_free(mh); json_free(wi); json_free(hi);
    return ret;
}

/* PoP: pa2_blank @ agent/pet/generate/atlas.py:_blank */
static json_t *pa2_blank(pa2_ctx *c, json_t *in) {
    json_t *s = pa2_get(in, "size");
    json_t *args = json_object();
    json_set(args, "mode", json_string("RGBA"));
    json_set(args, "size", s ? json_copy(s) : json_array());
    json_t *col = json_array();
    json_append(col, json_number(0)); json_append(col, json_number(0));
    json_append(col, json_number(0)); json_append(col, json_number(0));
    json_set(args, "color", col);
    return pa2_call(c, "Image.new", args);
}

/* PoP: pa2_place @ agent/pet/generate/atlas.py:_place */
static json_t *pa2_place(pa2_ctx *c, json_t *in) {
    json_t *sprite = pa2_get(in, "sprite");
    json_t *size = pa2_get(in, "size");
    json_t *offset = pa2_get(in, "offset");
    json_t *bi = json_object();
    json_set(bi, "size", size ? json_copy(size) : json_array());
    json_t *canvas = pa2_blank(c, bi);
    json_free(bi);
    if (pa2_pending(c)) return NULL;
    json_t *args = json_object();
    json_set(args, "canvas", canvas ? json_copy(canvas) : json_null());
    json_set(args, "sprite", sprite ? json_copy(sprite) : json_null());
    json_set(args, "offset", offset ? json_copy(offset) : json_array());
    json_free(pa2_call(c, "alpha_composite", args));
    return canvas;
}

/* PoP: pa2_clear_region @ agent/pet/generate/atlas.py:_clear_region */
static json_t *pa2_clear_region(pa2_ctx *c, json_t *in) {
    json_t *box = pa2_get(in, "box");
    int l = (int)pa2_jnum(pa2_box_from(box, 0), 0);
    int t = (int)pa2_jnum(pa2_box_from(box, 1), 0);
    int r = (int)pa2_jnum(pa2_box_from(box, 2), 0);
    int b = (int)pa2_jnum(pa2_box_from(box, 3), 0);
    json_t *bi = json_object();
    json_t *sz = json_array();
    json_append(sz, json_number(r - l)); json_append(sz, json_number(b - t));
    json_set(bi, "size", sz);
    json_t *blank = pa2_blank(c, bi);
    json_free(bi);
    if (pa2_pending(c)) return NULL;
    json_t *args = json_object();
    json_set(args, "image", json_copy(pa2_get(in, "image")));
    json_set(args, "src", blank ? json_copy(blank) : json_null());
    json_t *off = json_array();
    json_append(off, json_number(l)); json_append(off, json_number(t));
    json_set(args, "box", off);
    json_free(pa2_call(c, "paste", args));
    json_free(blank);
    return json_null();
}

/* PoP: pa2_load_rgba @ agent/pet/generate/atlas.py:_load_rgba */
static json_t *pa2_load_rgba(pa2_ctx *c, json_t *in) {
    json_t *img = pa2_get(in, "image");
    json_t *target = img;
    if (pa2_str(img)) {  /* isinstance(image, (str, Path)) */
        json_t *args = json_object();
        json_set(args, "path", json_copy(img));
        target = pa2_call(c, "Image.open", args);
        if (pa2_pending(c)) return NULL;
    }
    json_t *args = json_object();
    json_set(args, "image", target ? json_copy(target) : json_null());
    json_set(args, "mode", json_string("RGBA"));
    json_t *ret = pa2_call(c, "convert", args);
    if (pa2_str(img)) json_free(target);
    return ret;
}

/* PoP: pa2_remove_masked @ agent/pet/generate/atlas.py:_remove_masked */
static json_t *pa2_remove_masked(pa2_ctx *c, json_t *in) {
    json_t *rgba = pa2_get(in, "rgba");
    json_t *mask = pa2_get(in, "mask");
    json_t *bi = json_object();
    json_set(bi, "size", pa2_size_of(rgba));
    json_t *blank = pa2_blank(c, bi);
    json_free(bi);
    if (pa2_pending(c)) return NULL;
    json_t *args = json_object();
    json_set(args, "image1", blank ? json_copy(blank) : json_null());
    json_set(args, "image2", rgba ? json_copy(rgba) : json_null());
    json_set(args, "mask", mask ? json_copy(mask) : json_null());
    json_t *out = pa2_call(c, "composite", args);
    json_free(blank);
    if (pa2_pending(c)) return NULL;
    args = json_object();
    json_set(args, "image", json_copy(out));
    json_set(args, "channel", json_string("A"));
    json_t *ch = pa2_call(c, "getchannel", args);
    if (pa2_pending(c)) { json_free(out); json_free(ch); return NULL; }
    args = json_object();
    json_set(args, "image", ch ? json_copy(ch) : json_null());
    json_set(args, "filter", json_string("MinFilter3"));
    json_t *flt = pa2_call(c, "filter", args);
    if (pa2_pending(c)) { json_free(out); json_free(ch); json_free(flt); return NULL; }
    args = json_object();
    json_set(args, "image", json_copy(out));
    json_set(args, "alpha", flt ? json_copy(flt) : json_null());
    json_free(pa2_call(c, "putalpha", args));
    json_free(ch); json_free(flt);
    return out;
}

/* internal flood: vis owned by caller (mutated), seeds JSON array. */
static json_t *pa2_flood_arr(pa2_ctx *c, int w, int h, unsigned char *vis,
                             json_t *seeds) {
    if (w <= 0 || h <= 0 || !vis) return json_array();
    static const int DX[4] = {1, -1, 0, 0};
    static const int DY[4] = {0, 0, 1, -1};
    size_t qcap = (seeds && pa2_arr(seeds)) ? seeds->c.count + w * h + 8 : w * h + 8;
    if (qcap < 8) qcap = 8;
    int *qx = (int *)malloc(qcap * sizeof(int));
    int *qy = (int *)malloc(qcap * sizeof(int));
    size_t head = 0, tail = 0;
    if (seeds && pa2_arr(seeds)) {
        for (size_t i = 0; i < seeds->c.count && tail < qcap; i++) {
            qx[tail] = (int)pa2_jnum(pa2_box_from(seeds->c.items[i], 0), 0);
            qy[tail] = (int)pa2_jnum(pa2_box_from(seeds->c.items[i], 1), 0);
            tail++;
        }
    }
    json_t *pixels = json_array();
    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        json_t *p = json_array();
        json_append(p, json_number(x)); json_append(p, json_number(y));
        json_append(pixels, p);
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (vis[ny * w + nx]) continue;
            vis[ny * w + nx] = 1;
            json_t *args = json_object();
            pa2_arg_i(args, "x", nx); pa2_arg_i(args, "y", ny);
            json_t *acc = pa2_call(c, "accept", args);
            if (pa2_pending(c)) { json_free(acc); goto done; }
            bool ok = pa2_truthy(acc);
            json_free(acc);
            if (ok && tail < qcap) { qx[tail] = nx; qy[tail] = ny; tail++; }
        }
    }
done:
    free(qx); free(qy);
    return pixels;
}

/* PoP: pa2_flood @ agent/pet/generate/atlas.py:_flood */
static json_t *pa2_flood(pa2_ctx *c, json_t *in) {
    int w = (int)pa2_jnum(pa2_get(in, "w"), 0);
    int h = (int)pa2_jnum(pa2_get(in, "h"), 0);
    json_t *vin = pa2_get(in, "visited");
    json_t *seeds = pa2_get(in, "seeds");
    if (w <= 0 || h <= 0) return json_array();
    unsigned char *vis = (unsigned char *)calloc((size_t)w * h, 1);
    if (vin && pa2_arr(vin)) {
        for (size_t i = 0; i < (size_t)w * h && i < vin->c.count; i++)
            vis[i] = pa2_truthy(vin->c.items[i]) ? 1 : 0;
    }
    json_t *r = pa2_flood_arr(c, w, h, vis, seeds);
    free(vis);
    return r;
}

/* PoP: pa2_border_flood @ agent/pet/generate/atlas.py:_border_flood */
static json_t *pa2_border_flood(pa2_ctx *c, json_t *in) {
    int w = (int)pa2_jnum(pa2_get(in, "w"), 0);
    int h = (int)pa2_jnum(pa2_get(in, "h"), 0);
    json_t *vin = pa2_get(in, "visited");
    if (w <= 0 || h <= 0) return json_array();
    unsigned char *vis = (unsigned char *)calloc((size_t)w * h, 1);
    if (vin && pa2_arr(vin)) {
        for (size_t i = 0; i < (size_t)w * h && i < vin->c.count; i++)
            vis[i] = pa2_truthy(vin->c.items[i]) ? 1 : 0;
    }
    json_t *seeds = json_array();
    /* border = [(x,y) for x in range(w) for y in (0,h-1)]
     *          + [(x,y) for y in range(h) for x in (0,w-1)] */
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            for (int x = 0; x < w; x++) {
                int ys[2] = {0, h - 1};
                for (int j = 0; j < 2; j++) {
                    int y = ys[j];
                    if (vis[y * w + x]) continue;
                    json_t *a = json_object();
                    pa2_arg_i(a, "x", x); pa2_arg_i(a, "y", y);
                    json_t *acc = pa2_call(c, "accept", a);
                    if (pa2_pending(c)) { json_free(acc); free(vis); return json_null(); }
                    bool ok = pa2_truthy(acc); json_free(acc);
                    if (ok) {
                        vis[y * w + x] = 1;
                        json_t *p = json_array();
                        json_append(p, json_number(x)); json_append(p, json_number(y));
                        json_append(seeds, p);
                    }
                }
            }
        } else {
            for (int y = 0; y < h; y++) {
                int xs[2] = {0, w - 1};
                for (int j = 0; j < 2; j++) {
                    int x = xs[j];
                    if (vis[y * w + x]) continue;
                    json_t *a = json_object();
                    pa2_arg_i(a, "x", x); pa2_arg_i(a, "y", y);
                    json_t *acc = pa2_call(c, "accept", a);
                    if (pa2_pending(c)) { json_free(acc); free(vis); return json_null(); }
                    bool ok = pa2_truthy(acc); json_free(acc);
                    if (ok) {
                        vis[y * w + x] = 1;
                        json_t *p = json_array();
                        json_append(p, json_number(x)); json_append(p, json_number(y));
                        json_append(seeds, p);
                    }
                }
            }
        }
    }
    json_t *r = pa2_flood_arr(c, w, h, vis, seeds);
    free(vis);
    return r;
}

/* PoP: pa2_unvisited_components @ agent/pet/generate/atlas.py:_unvisited_components */
static json_t *pa2_unvisited_components(pa2_ctx *c, json_t *in) {
    int w = (int)pa2_jnum(pa2_get(in, "w"), 0);
    int h = (int)pa2_jnum(pa2_get(in, "h"), 0);
    json_t *vin = pa2_get(in, "visited");
    if (w <= 0 || h <= 0) return json_array();
    unsigned char *vis = (unsigned char *)calloc((size_t)w * h, 1);
    if (vin && pa2_arr(vin)) {
        for (size_t i = 0; i < (size_t)w * h && i < vin->c.count; i++)
            vis[i] = pa2_truthy(vin->c.items[i]) ? 1 : 0;
    }
    json_t *comps = json_array();
    for (int start = 0; start < w * h; start++) {
        if (vis[start]) continue;
        vis[start] = 1;
        int x = start % w, y = start / w;
        json_t *a = json_object();
        pa2_arg_i(a, "x", x); pa2_arg_i(a, "y", y);
        json_t *acc = pa2_call(c, "accept", a);
        if (pa2_pending(c)) { json_free(acc); free(vis); return json_null(); }
        bool ok = pa2_truthy(acc); json_free(acc);
        if (ok) {
            json_t *sd = json_array();
            json_t *p = json_array();
            json_append(p, json_number(x)); json_append(p, json_number(y));
            json_append(sd, p);
            json_t *comp = pa2_flood_arr(c, w, h, vis, sd);
            json_append(comps, comp);
        }
    }
    free(vis);
    return comps;
}

/* PoP: pa2_thin_groups @ agent/pet/generate/atlas.py:_thin_groups */
static json_t *pa2_thin_groups(pa2_ctx *c, json_t *in) {
    (void)c;
    json_t *idx = pa2_get(in, "indices");
    json_t *out = json_array();
    if (!pa2_arr(idx) || idx->c.count == 0) return out;
    size_t i = 0, n = idx->c.count;
    while (i < n) {
        int start = (int)pa2_jnum(idx->c.items[i], 0);
        size_t j = i + 1;
        while (j < n && (int)pa2_jnum(idx->c.items[j], 0) ==
               (int)pa2_jnum(idx->c.items[j - 1], 0) + 1) j++;
        if (j - i <= 4) {
            json_t *g = json_array();
            json_append(g, json_number(start));
            json_append(g, json_number((int)pa2_jnum(idx->c.items[j - 1], 0) + 1));
            json_append(out, g);
        }
        i = j;
    }
    return out;
}

/* PoP: pa2_has_margin @ agent/pet/generate/atlas.py:_has_margin */
static json_t *pa2_has_margin(pa2_ctx *c, json_t *in) {
    (void)c;
    json_t *sz = pa2_get(in, "size"), *box = pa2_get(in, "box");
    double w = pa2_jnum(pa2_box_from(sz, 0), 0), h = pa2_jnum(pa2_box_from(sz, 1), 0);
    double l = pa2_jnum(pa2_box_from(box, 0), 0), t = pa2_jnum(pa2_box_from(box, 1), 0);
    double r = pa2_jnum(pa2_box_from(box, 2), 0), b = pa2_jnum(pa2_box_from(box, 3), 0);
    double fx = pa2_jnum(pa2_get(in, "fx"), 0), fy = pa2_jnum(pa2_get(in, "fy"), 0);
    int min_x = (int)fmax(4, fmin(12, pa2_round(w * fx)));
    int min_y = (int)fmax(4, fmin(16, pa2_round(h * fy)));
    bool ok = l >= min_x && t >= min_y && (w - r) >= min_x && (h - b) >= min_y;
    return json_bool(ok);
}

/* PoP: pa2_is_multi_pose_outlier @ agent/pet/generate/atlas.py:_is_multi_pose_outlier */
static json_t *pa2_is_multi_pose_outlier(pa2_ctx *c, json_t *in) {
    (void)c;
    double width = pa2_jnum(pa2_get(in, "width"), 0);
    double height = pa2_jnum(pa2_get(in, "height"), 0);
    double mw = pa2_jnum(pa2_get(in, "med_w"), 0);
    double mh = pa2_jnum(pa2_get(in, "med_h"), 0);
    bool ok = width > fmax(mw * 3.0, mw + 96.0) && height <= mh * 1.6;
    return json_bool(ok);
}

/* internal clean_slot takes an image handle directly. */
static json_t *pa2_clean_slot_img(pa2_ctx *c, json_t *image) {
    json_t *a = json_object();
    json_set(a, "image", image ? json_copy(image) : json_null());
    json_t *iso = pa2_call(c, "_isolate_slot_subject", a);
    if (pa2_pending(c)) return NULL;
    a = json_object();
    json_set(a, "image", iso ? json_copy(iso) : json_null());
    json_t *out = pa2_call(c, "_drop_side_bleed", a);
    json_free(iso);
    return out;
}

/* PoP: pa2_clean_slot @ agent/pet/generate/atlas.py:_clean_slot */
static json_t *pa2_clean_slot(pa2_ctx *c, json_t *in) {
    return pa2_clean_slot_img(c, pa2_get(in, "image"));
}

/* PoP: pa2_salvage_frames @ agent/pet/generate/atlas.py:_salvage_frames */
static json_t *pa2_salvage_frames(pa2_ctx *c, json_t *in) {
    json_t *strip = pa2_get(in, "strip");
    int count = (int)pa2_jnum(pa2_get(in, "frame_count"), 0);
    json_t *a = json_object();
    json_set(a, "strip", json_copy(strip));
    pa2_arg_i(a, "frame_count", count);
    json_t *source = strip;
    json_t *ranges = pa2_call(c, "_frame_x_ranges", a);
    if (pa2_pending(c)) return NULL;
    if (pa2_none(ranges)) {
        json_free(ranges);
        a = json_object();
        json_set(a, "strip", json_copy(source));
        pa2_arg_i(a, "frame_count", count);
        json_t *src2 = pa2_call(c, "_sever_expected_gutters", a);
        if (pa2_pending(c)) { json_free(src2); return NULL; }
        if (source != strip) json_free(source);  /* source may borrow `in` */
        source = src2;
        a = json_object();
        json_set(a, "strip", json_copy(source));
        pa2_arg_i(a, "frame_count", count);
        ranges = pa2_call(c, "_frame_x_ranges", a);
        if (pa2_pending(c)) return NULL;
        if (pa2_none(ranges)) {
            a = json_object();
            json_set(a, "strip", json_copy(source));
            pa2_arg_i(a, "frame_count", count);
            json_set(a, "require_padding", json_bool(false));
            json_t *sc = pa2_call(c, "_slot_crops", a);
            if (pa2_pending(c)) { json_free(sc); return NULL; }
            return sc && pa2_arr(sc) && sc->c.count > 0 ? sc : json_array();
        }
    }
    json_t *sz = pa2_size_of(source);
    int w = (int)pa2_jnum(pa2_box_from(sz, 0), 0);
    int h = (int)pa2_jnum(pa2_box_from(sz, 1), 0);
    int denom = count > 1 ? count : 1;
    int pad = (int)fmax(2, fmin(16, pa2_round(((double)w / denom) * 0.04)));
    json_t *out = json_array();
    if (pa2_arr(ranges)) {
        for (size_t i = 0; i < ranges->c.count; i++) {
            int left = (int)pa2_jnum(pa2_box_from(ranges->c.items[i], 0), 0);
            int right = (int)pa2_jnum(pa2_box_from(ranges->c.items[i], 1), 0);
            int l = left - pad; if (l < 0) l = 0;
            int rr = right + pad; if (rr > w) rr = w;
            json_t *ca = json_object();
            json_set(ca, "image", json_copy(source));
            pa2_box_args(ca, "box", l, 0, rr, h);
            json_t *crop = pa2_call(c, "crop", ca);
            if (pa2_pending(c)) { json_free(crop); return NULL; }
            json_t *cl = pa2_clean_slot_img(c, crop);
            json_free(crop);
            if (pa2_pending(c)) return NULL;
            json_append(out, cl ? cl : json_null());
        }
    }
    json_free(sz);
    return out;
}

/* ROW_SPECS constants (state,row,count) — order must match atlas.py. */
static const char *PA2_STATES[9] = {
    "idle", "running-right", "running-left", "waving", "jumping",
    "failed", "waiting", "running", "review"
};
static const int PA2_ROWS[9]  = {0, 1, 2, 3, 4, 5, 6, 7, 8};
static const int PA2_COUNTS[9] = {6, 8, 8, 4, 5, 8, 6, 6, 6};
#define PA2_CELL_W 192
#define PA2_CELL_H 208

/* PoP: pa2_check_atlas_cells @ agent/pet/generate/atlas.py:_check_atlas_cells */
static json_t *pa2_check_atlas_cells(pa2_ctx *c, json_t *in) {
    json_t *atlas = pa2_get(in, "atlas");
    json_t *errors = json_array(), *warnings = json_array(), *filled = json_array();
    json_t *boxes_by_state[9];
    for (int i = 0; i < 9; i++) boxes_by_state[i] = json_array();

    for (int s = 0; s < 9; s++) {
        json_t *cells = json_array();
        for (int col = 0; col < PA2_COUNTS[s]; col++) {
            json_t *ca = json_object();
            json_set(ca, "image", json_copy(atlas));
            pa2_box_args(ca, "box", col * PA2_CELL_W, PA2_ROWS[s] * PA2_CELL_H,
                         (col + 1) * PA2_CELL_W, (PA2_ROWS[s] + 1) * PA2_CELL_H);
            json_t *cell = pa2_call(c, "crop", ca);
            if (pa2_pending(c)) return NULL;
            json_append(cells, cell ? cell : json_null());
        }
        bool any_filled = false;
        for (size_t i = 0; i < cells->c.count; i++) {
            json_t *chargs = json_object();
            json_set(chargs, "image", json_copy(cells->c.items[i]));
            json_set(chargs, "channel", json_string("A"));
            json_t *ch = pa2_call(c, "getchannel", chargs);
            if (pa2_pending(c)) { json_free(ch); return NULL; }
            json_t *ha = json_object();
            json_set(ha, "image", ch ? json_copy(ch) : json_null());
            json_t *hist = pa2_call(c, "histogram", ha);
            json_free(ch);
            if (pa2_pending(c)) { json_free(hist); return NULL; }
            long sum = 0;
            if (pa2_arr(hist)) for (size_t k = 1; k < hist->c.count; k++)
                sum += (long)pa2_jnum(hist->c.items[k], 0);
            json_free(hist);
            if (sum > 0) { any_filled = true; break; }
        }
        if (any_filled) {
            json_append(filled, json_string(PA2_STATES[s]));
            for (size_t i = 0; i < cells->c.count; i++) {
                json_t *ba = json_object();
                json_set(ba, "image", json_copy(cells->c.items[i]));
                json_t *bb = pa2_call(c, "getbbox", ba);
                if (pa2_pending(c)) { json_free(bb); return NULL; }
                if (pa2_arr(bb)) json_append(boxes_by_state[s], json_copy(bb));
                json_free(bb);
            }
        } else {
            char msg[128];
            snprintf(msg, sizeof msg, "state '%s' has no frames", PA2_STATES[s]);
            json_append(warnings, json_string(msg));
        }
        json_free(cells);
    }
    if (filled->c.count == 0)
        json_append(errors, json_string("atlas is empty \xe2\x80\x94 no state produced any frames"));

    json_t *all_boxes = json_array();
    for (int s = 0; s < 9; s++)
        for (size_t i = 0; i < boxes_by_state[s]->c.count; i++)
            json_append(all_boxes, json_copy(boxes_by_state[s]->c.items[i]));
    int gw = 0, gh = 0;
    if (all_boxes->c.count > 0) {
        json_t *boxin = json_object();
        json_set(boxin, "boxes", json_copy(all_boxes));  /* keep all_boxes owned here */
        json_t *med = pa2_median_box_size(c, boxin);
        gw = (int)pa2_jnum(pa2_box_from(med, 0), 0);
        gh = (int)pa2_jnum(pa2_box_from(med, 1), 0);
        json_free(med); json_free(boxin);
    }
    int height_floor = (int)fmax(56, pa2_round(PA2_CELL_H * 0.28));
    if (all_boxes->c.count > 0 && gh < height_floor) {
        char msg[192];
        snprintf(msg, sizeof msg,
                 "atlas sprites are too small after normalization (median frame height %dpx)", gh);
        json_append(errors, json_string(msg));
    }
    for (int s = 0; s < 9; s++) {
        json_t *boxes = boxes_by_state[s];
        if (boxes->c.count <= 1) continue;
        json_t *boxin = json_object();
        json_set(boxin, "boxes", json_copy(boxes));
        json_t *med = pa2_median_box_size(c, boxin);
        int mw = (int)pa2_jnum(pa2_box_from(med, 0), 0);
        int mh = (int)pa2_jnum(pa2_box_from(med, 1), 0);
        int maxw = 0, maxh = 0;
        for (size_t i = 0; i < boxes->c.count; i++) {
            json_t *b = boxes->c.items[i];
            int ww = (int)(pa2_jnum(pa2_box_from(b, 2), 0) - pa2_jnum(pa2_box_from(b, 0), 0));
            int hh = (int)(pa2_jnum(pa2_box_from(b, 3), 0) - pa2_jnum(pa2_box_from(b, 1), 0));
            if (ww > maxw) maxw = ww;
            if (hh > maxh) maxh = hh;
        }
        json_t *oi = json_object();
        json_set(oi, "width", json_number(maxw)); json_set(oi, "height", json_number(maxh));
        json_set(oi, "med_w", json_number(mw)); json_set(oi, "med_h", json_number(mh));
        json_t *outlier = pa2_is_multi_pose_outlier(c, oi);
        if (pa2_truthy(outlier)) {
            char msg[160];
            snprintf(msg, sizeof msg, "state '%s' contains a multi-pose frame outlier", PA2_STATES[s]);
            json_append(errors, json_string(msg));
        }
        json_free(outlier); json_free(oi);
        bool collapsed = mw < (int)fmax(32, pa2_round(gw * 0.42)) ||
                         mh < (int)fmax(40, pa2_round(gh * 0.50));
        if (gw && gh && collapsed) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "state '%s' appears collapsed (median %dx%dpx, global median %dx%dpx)",
                     PA2_STATES[s], mw, mh, gw, gh);
            json_append(errors, json_string(msg));
        }
        json_free(med); json_free(boxin);
    }
    json_t *ta = json_object();
    json_set(ta, "image", json_copy(atlas));
    json_t *data = pa2_call(c, "tobytes", ta);
    if (pa2_pending(c)) { json_free(data); return NULL; }
    long residue = 0;
    if (pa2_arr(data)) {
        for (size_t i = 3; i < data->c.count; i += 4) {
            if ((int)pa2_jnum(data->c.items[i], 0) == 0 &&
                (pa2_truthy(data->c.items[i - 3]) || pa2_truthy(data->c.items[i - 2]) ||
                 pa2_truthy(data->c.items[i - 1]))) residue++;
        }
    }
    json_free(data);
    if (residue) {
        char msg[128];
        snprintf(msg, sizeof msg, "%ld transparent pixels retain RGB residue", residue);
        json_append(errors, json_string(msg));
    }
    json_free(all_boxes);
    for (int s = 0; s < 9; s++) json_free(boxes_by_state[s]);
    json_t *ret = json_array();
    json_append(ret, errors); json_append(ret, warnings); json_append(ret, filled);
    return ret;
}

/* ── dispatch ──────────────────────────────────────────────────────────── */
json_t *pa2_dispatch(pa2_ctx *ctx, const char *func, json_t *in) {
    if (pa2_eq(func, "_median"))                 return pa2_median(ctx, in);
    if (pa2_eq(func, "_median_box_size"))        return pa2_median_box_size(ctx, in);
    if (pa2_eq(func, "_blank"))                  return pa2_blank(ctx, in);
    if (pa2_eq(func, "_place"))                  return pa2_place(ctx, in);
    if (pa2_eq(func, "_clear_region"))           return pa2_clear_region(ctx, in);
    if (pa2_eq(func, "_load_rgba"))              return pa2_load_rgba(ctx, in);
    if (pa2_eq(func, "_flood"))                  return pa2_flood(ctx, in);
    if (pa2_eq(func, "_border_flood"))           return pa2_border_flood(ctx, in);
    if (pa2_eq(func, "_unvisited_components"))   return pa2_unvisited_components(ctx, in);
    if (pa2_eq(func, "_remove_masked"))          return pa2_remove_masked(ctx, in);
    if (pa2_eq(func, "_thin_groups"))            return pa2_thin_groups(ctx, in);
    if (pa2_eq(func, "_has_margin"))             return pa2_has_margin(ctx, in);
    if (pa2_eq(func, "_is_multi_pose_outlier"))  return pa2_is_multi_pose_outlier(ctx, in);
    if (pa2_eq(func, "_clean_slot"))             return pa2_clean_slot(ctx, in);
    if (pa2_eq(func, "_salvage_frames"))         return pa2_salvage_frames(ctx, in);
    if (pa2_eq(func, "_check_atlas_cells"))      return pa2_check_atlas_cells(ctx, in);
    return json_null();
}
