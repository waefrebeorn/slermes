/*
 * runtime_footer.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway_runtime_footer.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <pthread.h>

/* ================================================================
 *  Runtime footer — compact model/context status line
 *  Port of Python gateway/runtime_footer.py.
 * ================================================================ */

/* Collapse $HOME to ~ in a path.
 * Port of Python gateway/runtime_footer.py _home_relative_cwd().
 * AG26: Port of Python gateway/runtime_footer.py:_home_relative_cwd().
 */
/* PoP: home_relative_cwd @ gateway/runtime_footer.py:_home_relative_cwd */
char *home_relative_cwd(const char *cwd) {
    if (!cwd || !*cwd) return strdup("");
    const char *home = getenv("HOME");
    if (!home) return strdup(cwd);
    size_t home_len = strlen(home);
    if (strncmp(cwd, home, home_len) == 0) {
        const char *suffix = cwd + home_len;
        if (*suffix == '/' || *suffix == '\0') {
            char buf[1024];
            snprintf(buf, sizeof(buf), "~%s", suffix);
            return strdup(buf);
        }
    }
    return strdup(cwd);
}


/* Drop vendor/ prefix from model name.
 * Port of Python gateway/runtime_footer.py _model_short().
 * AG26: Port of Python gateway/runtime_footer.py:_model_short().
 */
/* PoP: model_short @ gateway/runtime_footer.py:_model_short */
char *model_short(const char *model) {
    if (!model || !*model) return strdup("");
    const char *slash = strrchr(model, '/');
    if (slash) return strdup(slash + 1);
    return strdup(model);
}

/* Default footer fields, in display order. */
static const char *RF_DEFAULT_FIELDS[] = {"model", "context_pct", "cwd"};
#define RF_N_DEFAULT_FIELDS 3
static const char *RF_SEP = " \xc2\xb7 ";  /* " · " (U+00B7 middle dot, UTF-8) */

/* Build a JSON array of the default field names. */
static json_node_t *rf_default_fields_array(void) {
    json_node_t *arr = json_new_array();
    for (int i = 0; i < RF_N_DEFAULT_FIELDS; i++)
        json_array_append(arr, json_new_string(RF_DEFAULT_FIELDS[i]));
    return arr;
}

/* Copy a JSON array of strings (coercing each element to string), returning a
 * fresh array. Returns NULL if the input is not a non-empty array of usable
 * values. Mirrors Python's `[str(f) for f in fields]`. */
static json_node_t *rf_copy_string_fields(json_node_t *src) {
    if (!json_is_array(src) || json_array_size(src) == 0) return NULL;
    json_node_t *out = json_new_array();
    for (size_t i = 0; i < json_array_size(src); i++) {
        json_node_t *e = json_array_get(src, i);
        const char *s = json_string_value(e);
        json_array_append(out, json_new_string(s ? s : ""));
    }
    return out;
}

/* PoP: resolve_footer_config @ gateway/runtime_footer.py:resolve_footer_config */
json_node_t *resolve_footer_config(json_node_t *user_config,
                                    const char *platform_key) {
    json_node_t *resolved = json_new_object();
    json_object_set(resolved, "enabled", json_new_bool(false));
    json_object_set(resolved, "fields", rf_default_fields_array());

    json_node_t *display = json_is_object(user_config)
                               ? json_object_get(user_config, "display")
                               : NULL;
    if (!json_is_object(display)) return resolved;

    /* 2. display.runtime_footer */
    json_node_t *global_cfg = json_object_get(display, "runtime_footer");
    if (json_is_object(global_cfg)) {
        json_node_t *en = json_object_get(global_cfg, "enabled");
        if (en) json_object_set(resolved, "enabled", json_new_bool(json_bool_value(en)));
        json_node_t *fields = json_object_get(global_cfg, "fields");
        json_node_t *copy = rf_copy_string_fields(fields);
        if (copy) json_object_set(resolved, "fields", copy);
    }

    /* 3. display.platforms.<platform_key>.runtime_footer */
    if (platform_key && platform_key[0]) {
        json_node_t *platforms = json_object_get(display, "platforms");
        if (json_is_object(platforms)) {
            json_node_t *plat_cfg = json_object_get(platforms, platform_key);
            if (json_is_object(plat_cfg)) {
                json_node_t *plat_footer = json_object_get(plat_cfg, "runtime_footer");
                if (json_is_object(plat_footer)) {
                    json_node_t *en = json_object_get(plat_footer, "enabled");
                    if (en) json_object_set(resolved, "enabled", json_new_bool(json_bool_value(en)));
                    json_node_t *fields = json_object_get(plat_footer, "fields");
                    json_node_t *copy = rf_copy_string_fields(fields);
                    if (copy) json_object_set(resolved, "fields", copy);
                }
            }
        }
    }

    return resolved;
}

/* PoP: format_latency @ gateway/runtime_footer.py:_format_latency */
const char *format_latency(double seconds)
{
    static char buf[16];
    if (seconds < 1.0) return "<1s";
    int total = (int)round(seconds);
    if (total < 60) { snprintf(buf, sizeof(buf), "%ds", total); return buf; }
    int m = total / 60;
    int s = total % 60;
    snprintf(buf, sizeof(buf), "%dm%02ds", m, s);
    return buf;
}

/* PoP: format_runtime_footer @ gateway/runtime_footer.py:format_runtime_footer */
char *format_runtime_footer(const char *model,
                            int context_tokens,
                            int context_length,
                            const char *cwd,
                            json_node_t *fields) {
    /* Assemble parts, joined with RF_SEP. */
    char out[2048];
    out[0] = '\0';
    int nparts = 0;

    size_t nfields = (fields && json_is_array(fields)) ? json_array_size(fields)
                                                       : (size_t)RF_N_DEFAULT_FIELDS;
    for (size_t i = 0; i < nfields; i++) {
        const char *field;
        if (fields && json_is_array(fields)) {
            field = json_string_value(json_array_get(fields, i));
            if (!field) continue;
        } else {
            field = RF_DEFAULT_FIELDS[i];
        }

        char piece[1024];
        piece[0] = '\0';

        if (strcmp(field, "model") == 0) {
            char *m = model_short(model);
            if (m && m[0]) snprintf(piece, sizeof(piece), "%s", m);
            free(m);
        } else if (strcmp(field, "context_pct") == 0) {
            if (context_length > 0 && context_tokens >= 0) {
                /* Python round((tokens/length)*100): banker's rounding (half to even),
                 * then clamp to [0,100]. */
                double raw = ((double)context_tokens / (double)context_length) * 100.0;
                double floor_v = (double)(long)raw;
                double frac = raw - floor_v;
                long pct;
                if (frac < 0.5) pct = (long)floor_v;
                else if (frac > 0.5) pct = (long)floor_v + 1;
                else { /* exactly .5 -> round to even */
                    long lo = (long)floor_v;
                    pct = (lo % 2 == 0) ? lo : lo + 1;
                }
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                snprintf(piece, sizeof(piece), "%ld%%", pct);
            }
        } else if (strcmp(field, "cwd") == 0) {
            const char *c = (cwd && cwd[0]) ? cwd : getenv("TERMINAL_CWD");
            char *rel = home_relative_cwd(c ? c : "");
            if (rel && rel[0]) snprintf(piece, sizeof(piece), "%s", rel);
            free(rel);
        }
        /* Unknown field names silently ignored. */

        if (piece[0]) {
            if (nparts > 0)
                strncat(out, RF_SEP, sizeof(out) - strlen(out) - 1);
            strncat(out, piece, sizeof(out) - strlen(out) - 1);
            nparts++;
        }
    }

    return strdup(out);
}

/* PoP: build_footer_line @ gateway/runtime_footer.py:build_footer_line */
char *build_footer_line(json_node_t *user_config,
                        const char *platform_key,
                        const char *model,
                        int context_tokens,
                        int context_length,
                        const char *cwd) {
    json_node_t *cfg = resolve_footer_config(user_config, platform_key);
    json_node_t *en = json_object_get(cfg, "enabled");
    if (!json_bool_value(en)) {
        json_free(cfg);
        return strdup("");
    }
    json_node_t *fields = json_object_get(cfg, "fields");
    char *line = format_runtime_footer(model, context_tokens, context_length, cwd, fields);
    json_free(cfg);
    return line;
}


/* ── tail_revive4 seam infrastructure (runtime_footer) ─────────────────────
 * fn(rft_io_t *io, json_t *in); rft_seam records {ev,args,ret} (args adopted).
 * _env_cwd's `from tools.terminal_scope import terminal_env` becomes the
 * "import_terminal_env" seam (ImportError/ModuleNotFoundError -> the
 * os.environ fallback via "environ_get"); the call itself is "terminal_env".
 * Only ImportError is caught — anything else propagates. */
typedef struct rft_io {
    json_t *(*call)(struct rft_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} rft_io_t;

static int rft_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *rft_seam(rft_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* PoP: _env_cwd @ gateway/runtime_footer.py:_env_cwd */
json_t *gateway_runtime_footer_env_cwd(rft_io_t *io, json_t *in) {
    (void)in;
    json_t *imp = rft_seam(io, "import_terminal_env", NULL);
    if (rft_is_raise(imp)) {
        const char *t = json_get_str(imp, "__raise__", "");
        if (strcmp(t, "ImportError") != 0 && strcmp(t, "ModuleNotFoundError") != 0)
            return imp;  /* only ImportError is caught */
        json_free(imp);
        json_t *args = json_object();
        json_set_hdr(args, "key", "TERMINAL_CWD");
        json_set_hdr(args, "default", "");
        json_t *r = rft_seam(io, "environ_get", args);
        if (rft_is_raise(r)) return r;
        return r ? r : json_null();
    }
    json_free(imp);
    json_t *args = json_object();
    json_set_hdr(args, "name", "TERMINAL_CWD");
    json_set_hdr(args, "default", "");
    json_t *r = rft_seam(io, "terminal_env", args);
    if (rft_is_raise(r)) return r;
    return r ? r : json_null();
}

/* _format_latency is already ported above as format_latency (same live def);
 * this placeholder stays unimplemented and untagged. */
const char *gateway_runtime_footer_format_latency(double ms) { (void)ms; return NULL; }
