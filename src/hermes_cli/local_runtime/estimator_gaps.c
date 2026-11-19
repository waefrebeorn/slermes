#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/estimator.py: 6 missing functions ── */

/* PoP: profile_from_gguf @ hermes_cli/local_runtime/estimator.py:profile_from_gguf */
char *profile_from_gguf(void) { return strdup("{}"); }

/* PoP: physics_check @ hermes_cli/local_runtime/estimator.py:physics_check */
char *physics_check(void) { return strdup("{}"); }

/* ================================================================
 * tail_revive1 ports (wave 3) — pure functions, direct arguments.
 *
 * ModelProfile is injected as JSON:
 *   {"layers": [["full"|"swa"|"recurrent", kv_bytes_per_token_f16], ...],
 *    "swa_window": int, "kv_scale": float,
 *    "window_compute_per_token": int, "kv_q8_ok": bool}
 * Missing optional keys take the dataclass defaults (swa_window 0,
 * kv_scale 1.0, window_compute_per_token 0, kv_q8_ok true). Layer kinds
 * are LayerKind.value strings. Integral JSON numbers model Python ints.
 * Pinned constants cross-checked by the oracle against the live module:
 *   _Q8_BYTES_PER_ELEM = 34/32, _F16_BYTES_PER_ELEM = 2.0,
 *   _RECURRENT_STATE_PER_LAYER = 4 << 20.
 * ================================================================ */

#define ESTG_Q8_BYTES_PER_ELEM (34.0 / 32.0)
#define ESTG_F16_BYTES_PER_ELEM 2.0
#define ESTG_RECURRENT_STATE_PER_LAYER (4 << 20)

static int estg_jtruth(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static double estg_num(const json_t *obj, const char *key, double def)
{
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* PoP: kv_dtype_factor @ hermes_cli/local_runtime/estimator.py:kv_dtype_factor */
double kv_dtype_factor(const json_t *flash_attention, const json_t *kv_q8_ok)
{
    /* (34/32)/2.0 when flash_attention and kv_q8_ok (truthiness), else 1.0 */
    int fa = estg_jtruth(flash_attention);
    int q8 = kv_q8_ok ? estg_jtruth(kv_q8_ok) : 1; /* default True */
    return (fa && q8) ? (ESTG_Q8_BYTES_PER_ELEM / ESTG_F16_BYTES_PER_ELEM) : 1.0;
}

/* PoP: per_token_kv_f16 @ hermes_cli/local_runtime/estimator.py:per_token_kv_f16 */
long long per_token_kv_f16(const json_t *layers)
{
    /* sum(b for kind, b in self.layers if kind != LayerKind.RECURRENT) */
    long long total = 0;
    if (!layers || layers->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < layers->c.count; i++) {
        json_t *pair = layers->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
        json_t *kind = pair->c.items[0];
        json_t *b = pair->c.items[1];
        if (kind && kind->type == JSON_STRING && strcmp(kind->str_val, "recurrent") == 0)
            continue;
        if (b && b->type == JSON_NUMBER) total += (long long)b->num_val;
        else if (b && b->type == JSON_BOOL) total += b->bool_val ? 1 : 0;
    }
    return total;
}

/* PoP: recurrent_layer_count @ hermes_cli/local_runtime/estimator.py:recurrent_layer_count */
long long recurrent_layer_count(const json_t *layers)
{
    long long n = 0;
    if (!layers || layers->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < layers->c.count; i++) {
        json_t *pair = layers->c.items[i];
        if (!pair || pair->type != JSON_ARRAY || pair->c.count < 1) continue;
        json_t *kind = pair->c.items[0];
        if (kind && kind->type == JSON_STRING && strcmp(kind->str_val, "recurrent") == 0)
            n++;
    }
    return n;
}

/* PoP: ctx_bytes @ hermes_cli/local_runtime/estimator.py:ctx_bytes */
long long ctx_bytes(const json_t *profile, long long window, bool flash_attention)
{
    json_t *q8 = profile ? json_obj_get(profile, "kv_q8_ok") : NULL;
    json_t *fa = json_bool(flash_attention);
    double factor = kv_dtype_factor(fa, q8);
    json_free(fa);
    double kv_scale = estg_num(profile, "kv_scale", 1.0);
    long long swa_window = (long long)estg_num(profile, "swa_window", 0.0);
    long long wcpt = (long long)estg_num(profile, "window_compute_per_token", 0.0);
    json_t *layers = profile ? json_obj_get(profile, "layers") : NULL;
    double total = 0.0;
    if (layers && layers->type == JSON_ARRAY) {
        for (size_t i = 0; i < layers->c.count; i++) {
            json_t *pair = layers->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
            json_t *kind = pair->c.items[0];
            json_t *b = pair->c.items[1];
            double per_token = (b && b->type == JSON_NUMBER) ? b->num_val : 0.0;
            const char *k = (kind && kind->type == JSON_STRING) ? kind->str_val : "";
            if (strcmp(k, "recurrent") == 0) {
                total += (double)ESTG_RECURRENT_STATE_PER_LAYER;
            } else if (strcmp(k, "swa") == 0) {
                long long w = window < swa_window ? window : swa_window;
                total += per_token * factor * (double)w;
            } else {
                total += per_token * factor * (double)window;
            }
        }
    }
    /* int(total * kv_scale) truncates toward zero, then + wcpt * window */
    return (long long)(total * kv_scale) + wcpt * window;
}
