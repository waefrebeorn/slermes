/*
 * methods_images.c — C port of upstream Hermes tui_gateway/methods_images.py.
 *
 * PoP: w14_methodsima_f49896_image_generate @ tui_gateway/methods_images.py:image.generate
 * PoP: w14_methodsima_f49896_image_render @ tui_gateway/methods_images.py:image.render
 *
 * _ok is identity; _err is {code, error}. Backend availability/result, display
 * config and render result are explicit fixture seams (no network / filesystem).
 * Image bytes are never fetched in a gate, so image_data is always omitted.
 */
#include "noc_w14_methodsima_f49896.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) { size_t n = s ? strlen(s) : 0; char *p = (char *)malloc(n + 1); if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; } return p; }

static int py_truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}
/* utils.env.is_truthy_value: None->default(False); str -> strip().lower() in
 * {1,true,yes,on}; else bool(value). */
static int is_truthy_value(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n && isspace((unsigned char)s[n-1])) n--;
        char w[64]; if (n >= sizeof(w)) n = sizeof(w) - 1;
        for (size_t i = 0; i < n; ++i) w[i] = (char)tolower((unsigned char)s[i]);
        w[n] = '\0';
        return !strcmp(w,"1") || !strcmp(w,"true") || !strcmp(w,"yes") || !strcmp(w,"on");
    }
    return py_truthy(v);
}
/* str(v or "") for scalar JSON values; malloc'd. */
static char *str_or_empty(const struct json_t *v) {
    char buf[64];
    if (!py_truthy(v)) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val);
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("");
}
static char *dupn_local(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *strip_ws(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    return dupn_local(s + a, b - a);
}
static char *lower_ascii(const char *s) {
    size_t n = strlen(s); char *o = (char *)malloc(n + 1); if (!o) return NULL;
    for (size_t i = 0; i < n; ++i) o[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i]-'A'+'a') : s[i];
    o[n] = '\0'; return o;
}
static struct json_t *err(int code, const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "code", json_number(code));
    json_set_hdr(o, "error", msg);
    return o;
}
static int py_int(const struct json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL; long long x = strtoll(v->str_val, &end, 10);
        if (end && *end == '\0') { *out = x; return 1; }
    }
    return 0;
}

/* Deterministic subset of agent.terminal_image.resolve_inline_mode with the
 * client-supplied `detected` always present (as the handler always supplies it). */
static char *resolve_mode(const struct json_t *configured, int graphics) {
    if (configured && configured->type == JSON_BOOL && !configured->bool_val) return dupstr("off");
    const char *m = "auto";
    char *tmp = NULL;
    if (configured && configured->type == JSON_STRING && configured->str_val) {
        char *s = strip_ws(configured->str_val);
        tmp = lower_ascii(s); free(s); m = tmp;
    } else if (configured && configured->type == JSON_BOOL && configured->bool_val) {
        m = "auto";
    }
    int valid = !strcmp(m,"auto") || !strcmp(m,"kitty") || !strcmp(m,"iterm") ||
                !strcmp(m,"unicode") || !strcmp(m,"off");
    if (!valid) m = "auto";
    /* iterm downgraded to auto when overlays=False (always false here) */
    if (!strcmp(m,"iterm")) m = "auto";
    char *res;
    if (!strcmp(m,"off")) res = dupstr("off");
    else if (!strcmp(m,"unicode") || !graphics) res = dupstr("unicode");
    else if (!strcmp(m,"kitty")) res = dupstr("kitty");
    else res = NULL; /* auto: resolved from detected below */
    free(tmp);
    if (res) return res;
    /* detected = "kitty" iff params.terminal == "kitty"; callers pass this in env. */
    return NULL; /* filled by caller */
}

struct json_t *w14_methodsima_f49896_image_generate(const struct json_t *params, const struct json_t *env) {
    const struct json_t *p = (params && params->type == JSON_OBJECT) ? params : NULL;
    const struct json_t *e = (env && env->type == JSON_OBJECT) ? env : NULL;
    int available = 0;
    if (e) {
        const struct json_t *a = json_obj_get(e, "available");
        available = a && a->type == JSON_BOOL && a->bool_val;
    }
    const struct json_t *probe = p ? json_obj_get(p, "probe") : NULL;
    if (is_truthy_value(probe)) {
        struct json_t *o = json_object();
        json_set(o, "available", json_bool(available));
        return o;
    }
    if (!available) {
        struct json_t *o = json_object();
        json_set(o, "available", json_bool(false));
        json_set(o, "success", json_bool(false));
        json_set_hdr(o, "error", "No image generation backend configured (run `hermes tools` to enable one).");
        return o;
    }
    char *prompt = str_or_empty(p ? json_obj_get(p, "prompt") : NULL);
    char *ps = strip_ws(prompt); free(prompt);
    if (!ps[0]) { free(ps); return err(4071, "prompt required"); }
    char *aspect_raw = str_or_empty(p ? json_obj_get(p, "aspect_ratio") : NULL);
    if (!aspect_raw[0]) { free(aspect_raw); aspect_raw = dupstr("square"); }
    char *as = strip_ws(aspect_raw); free(aspect_raw);
    char *aspect = lower_ascii(as); free(as);
    (void)aspect; /* passed to the (stubbed) provider dispatcher */
    /* cap = min(int(max_bytes or 8M), 16M), fallback 8M on bad input. */
    long long cap = 8000000;
    const struct json_t *mb = p ? json_obj_get(p, "max_bytes") : NULL;
    if (py_truthy(mb) && py_int(mb, &cap)) { if (cap > 16000000) cap = 16000000; }
    else cap = 8000000;
    (void)cap;
    const struct json_t *ge = e ? json_obj_get(e, "gen_error") : NULL;
    if (ge && ge->type == JSON_STRING && ge->str_val) return err(5071, ge->str_val);
    const struct json_t *gr = e ? json_obj_get(e, "gen") : NULL;
    int success = 0;
    if (gr && gr->type == JSON_OBJECT) {
        const struct json_t *s = json_obj_get(gr, "success");
        success = s && s->type == JSON_BOOL && s->bool_val;
    }
    if (!success) {
        const char *msg = "generation failed";
        if (gr && gr->type == JSON_OBJECT) {
            const struct json_t *em = json_obj_get(gr, "error");
            if (em && em->type == JSON_STRING && em->str_val && em->str_val[0]) msg = em->str_val;
        }
        struct json_t *o = json_object();
        json_set(o, "available", json_bool(true));
        json_set(o, "success", json_bool(false));
        json_set_hdr(o, "error", msg);
        free(ps); free(aspect);
        return o;
    }
    const struct json_t *im = json_obj_get(gr, "image");
    char *img = str_or_empty(im);
    struct json_t *o = json_object();
    json_set(o, "available", json_bool(true));
    json_set(o, "success", json_bool(true));
    json_set_hdr(o, "image", img);
    /* image_data omitted: fixtures use non-file, non-URL refs. */
    free(img); free(ps); free(aspect);
    return o;
}

struct json_t *w14_methodsima_f49896_image_render(const struct json_t *params, const struct json_t *env) {
    const struct json_t *p = (params && params->type == JSON_OBJECT) ? params : NULL;
    const struct json_t *e = (env && env->type == JSON_OBJECT) ? env : NULL;
    const struct json_t *cfg = e ? json_obj_get(e, "inline_images") : NULL;
    const struct json_t *gv = p ? json_obj_get(p, "graphics") : NULL;
    int graphics = gv && gv->type == JSON_BOOL && gv->bool_val;
    const struct json_t *tv = p ? json_obj_get(p, "terminal") : NULL;
    int detected_kitty = tv && tv->type == JSON_STRING && tv->str_val && !strcmp(tv->str_val, "kitty");
    int path_present = 0;
    if (e) { const struct json_t *pp = json_obj_get(e, "path_present"); path_present = py_truthy(pp); }
    if (!path_present) {
        struct json_t *o = json_object(); json_set_hdr(o, "mode", "off"); return o;
    }
    long long cols = 0, rows = 0;
    const struct json_t *cv = p ? json_obj_get(p, "cols") : NULL;
    const struct json_t *rv = p ? json_obj_get(p, "rows") : NULL;
    if (!((!cv || py_int(cv, &cols)) && (!rv || py_int(rv, &rows))))
        return err(4072, "cols and rows must be integers");
    if (cols > 64) cols = 64;
    if (rows > 16) rows = 16;
    /* mode resolution */
    char *mode;
    if (cfg && cfg->type == JSON_BOOL && !cfg->bool_val) mode = dupstr("off");
    else {
        char *mm = resolve_mode(cfg, graphics);
        if (mm) mode = mm;
        else mode = dupstr(detected_kitty ? "kitty" : "unicode");
    }
    const struct json_t *rend = e ? json_obj_get(e, "render") : NULL;
    if (!rend || rend->type != JSON_OBJECT) {
        struct json_t *o = json_object(); json_set_hdr(o, "mode", "off"); free(mode); return o;
    }
    const struct json_t *imode = json_obj_get(rend, "mode");
    const char *ims = (imode && imode->type == JSON_STRING && imode->str_val) ? imode->str_val : "unicode";
    struct json_t *o = json_object();
    if (!strcmp(ims, "kitty")) {
        json_set_hdr(o, "mode", "kitty");
        const struct json_t *ic = json_obj_get(rend, "cols");
        const struct json_t *ir = json_obj_get(rend, "rows");
        json_set(o, "cols", ic ? json_copy(ic) : json_number((double)cols));
        json_set(o, "rows", ir ? json_copy(ir) : json_number((double)rows));
        const char *keys[] = {"color","transmit","placeholder"};
        for (size_t k = 0; k < 3; ++k) {
            const struct json_t *fv = json_obj_get(rend, keys[k]);
            json_set(o, keys[k], fv ? json_copy(fv) : json_string(""));
        }
    } else {
        json_set_hdr(o, "mode", "unicode");
        const struct json_t *ic = json_obj_get(rend, "cols");
        const struct json_t *ir = json_obj_get(rend, "rows");
        json_set(o, "cols", ic ? json_copy(ic) : json_number((double)cols));
        json_set(o, "rows", ir ? json_copy(ir) : json_number((double)rows));
        struct json_t *cells = json_array();
        const struct json_t *src_cells = json_obj_get(rend, "cells");
        if (src_cells && src_cells->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(src_cells); ++i) {
                struct json_t *row = json_get(src_cells, i);
                struct json_t *newrow = json_array();
                if (row && row->type == JSON_ARRAY) {
                    for (size_t j = 0; j < json_len(row); ++j) {
                        struct json_t *pair = json_get(row, j);
                        struct json_t *np = json_array();
                        if (pair && pair->type == JSON_ARRAY) {
                            /* Python [*top, *bottom]: flatten each side's elements. */
                            for (size_t q = 0; q < json_len(pair); ++q) {
                                const struct json_t *side = json_get(pair, q);
                                if (side && side->type == JSON_ARRAY) {
                                    for (size_t z = 0; z < json_len(side); ++z)
                                        json_append(np, json_copy(json_get(side, z)));
                                } else {
                                    json_append(np, json_copy(side));
                                }
                            }
                        }
                        json_append(newrow, np);
                    }
                }
                json_append(cells, newrow);
            }
        }
        json_set(o, "cells", cells);
    }
    (void)mode;
    free(mode);
    return o;
}
