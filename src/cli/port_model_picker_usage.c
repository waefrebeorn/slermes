/*
 * port_model_picker_usage.c — Port of hermes_cli/model_picker_usage.py.
 *
 * The reusable plan-usage chrome helpers: fraction clamping, the allowance
 * gauge (style / filled cells / bar / percent), reset countdowns, the
 * recommended-first list split, the credits-pay rule and the free-model test.
 * Shared by the CLI radiolist and the gateway model.options inventory.
 */

#include "port_model_picker_usage.h"
#include "port_models_pricing.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BAR_WIDTH 10
#define BAR_FILL '|'
#define BAR_EMPTY ' '
#define YELLOW_BELOW 0.5
#define RED_BELOW 0.2
#define BP_FULL 10000.0

/* Python 3 round(): banker's rounding (half to even). */
static double py_round(double x)
{
    double f = floor(x);
    double diff = x - f;
    if (diff < 0.5)
        return f;
    if (diff > 0.5)
        return f + 1.0;
    return fmod(f, 2.0) == 0.0 ? f : f + 1.0;
}

static double py_round_to(double x, int prec)
{
    double scale = pow(10.0, (double)prec);
    return py_round(x * scale) / scale;
}

/* Python isinstance(v, int): a JSON bool counts, an integral number counts. */
static int json_as_long(const json_t *v, long *out)
{
    if (json_is_bool(v)) {
        *out = json_bool_value(v) ? 1 : 0;
        return 1;
    }
    if (json_is_number(v)) {
        double d = json_number_value(v);
        if (d != floor(d))
            return 0;
        *out = (long)d;
        return 1;
    }
    return 0;
}

/* PoP: clamp_fraction @ hermes_cli/model_picker_usage.py:clamp_fraction */
double mpu_clamp_fraction(double value)
{
    if (value < 0.0)
        return 0.0;
    return value < 1.0 ? value : 1.0;
}

/* PoP: gauge_style @ hermes_cli/model_picker_usage.py:gauge_style */
const char *mpu_gauge_style(double left_fraction)
{
    double left = mpu_clamp_fraction(left_fraction);
    if (left < RED_BELOW)
        return "red";
    if (left < YELLOW_BELOW)
        return "yellow";
    return "green";
}

/* PoP: _filled_cells @ hermes_cli/model_picker_usage.py:_filled_cells */
long mpu_filled_cells(double fraction, long width)
{
    double value = mpu_clamp_fraction(fraction);
    long filled = (long)py_round(value * (double)width);
    if (value > 0.0 && filled == 0)
        filled = 1;
    if (value < 1.0 && filled == width)
        filled = width - 1;
    return filled;
}

/* PoP: render_bar @ hermes_cli/model_picker_usage.py:render_bar */
void mpu_render_bar(double fraction, long width, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    if (width < 0)
        width = 0;
    long filled = mpu_filled_cells(fraction, width);
    size_t cap = outsz - 1;
    size_t p = 0;
    if (p < cap)
        out[p++] = '[';
    for (long i = 0; i < filled && p < cap; i++)
        out[p++] = BAR_FILL;
    for (long i = filled; i < width && p < cap; i++)
        out[p++] = BAR_EMPTY;
    if (p < cap)
        out[p++] = ']';
    out[p] = '\0';
}

/* PoP: format_percent_used @ hermes_cli/model_picker_usage.py:format_percent_used */
void mpu_format_percent_used(double used_fraction, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    long bp = (long)py_round_to(mpu_clamp_fraction(used_fraction) * BP_FULL, 6);
    if (bp <= 0)
        snprintf(out, outsz, "0%%");
    else if (bp < 100)
        snprintf(out, outsz, "<1%%");
    else
        snprintf(out, outsz, "%ld%%", bp / 100);
}

/* PoP: compact_duration @ hermes_cli/model_picker_usage.py:compact_duration */
void mpu_compact_duration(json_t *resets_at_ms, json_t *now_ms, char *out,
                          size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';
    long resets;
    if (!json_as_long(resets_at_ms, &resets))
        return;
    long now;
    if (!now_ms)
        now = (long)(time(NULL) * 1000);
    else if (!json_as_long(now_ms, &now))
        now = (long)(time(NULL) * 1000);
    long seconds = (resets - now) / 1000;
    if (seconds < 0)
        seconds = 0;
    if (seconds < 60)
        return;
    long days = seconds / 86400;
    long rem = seconds % 86400;
    long hours = rem / 3600;
    long minutes = (rem % 3600) / 60;
    if (days)
        snprintf(out, outsz, "%ldd %ldh", days, hours);
    else if (hours)
        snprintf(out, outsz, "%ldh", hours);
    else
        snprintf(out, outsz, "%ldm", minutes);
}

/* PoP: format_resets_in @ hermes_cli/model_picker_usage.py:format_resets_in */
void mpu_format_resets_in(json_t *resets_at_ms, json_t *now_ms, char *out,
                          size_t outsz)
{
    if (!out || outsz == 0)
        return;
    char dur[64];
    mpu_compact_duration(resets_at_ms, now_ms, dur, sizeof(dur));
    if (dur[0])
        snprintf(out, outsz, "resets in %s", dur);
    else
        snprintf(out, outsz, "resets soon");
}

/* PoP: split_recommended @ hermes_cli/model_picker_usage.py:split_recommended */
json_t *mpu_split_recommended(json_t *model_ids, json_t *featured, json_t **rest)
{
    json_t *head = json_new_array();
    json_t *other = json_new_array();
    if (rest)
        *rest = other;

    /* listed = set(model_ids); featured deduped in plan order, kept if listed */
    if (json_is_array(featured) && json_is_array(model_ids)) {
        size_t nf = json_len(featured);
        for (size_t i = 0; i < nf; i++) {
            json_t *m = json_get(featured, i);
            if (!json_is_string(m))
                continue;
            const char *s = json_string_value(m);
            int listed = 0;
            size_t nl = json_len(model_ids);
            for (size_t j = 0; j < nl; j++) {
                json_t *x = json_get(model_ids, j);
                if (json_is_string(x) && strcmp(json_string_value(x), s) == 0) {
                    listed = 1;
                    break;
                }
            }
            if (!listed)
                continue;
            int seen = 0;
            size_t nh = json_len(head);
            for (size_t j = 0; j < nh; j++) {
                json_t *x = json_get(head, j);
                if (json_is_string(x) && strcmp(json_string_value(x), s) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen)
                json_array_append(head, json_new_string(s));
        }
    }
    if (json_is_array(model_ids)) {
        size_t nl = json_len(model_ids);
        for (size_t i = 0; i < nl; i++) {
            json_t *m = json_get(model_ids, i);
            const char *s = json_is_string(m) ? json_string_value(m) : NULL;
            int seen = 0;
            if (s) {
                size_t nh = json_len(head);
                for (size_t j = 0; j < nh; j++) {
                    json_t *x = json_get(head, j);
                    if (json_is_string(x) && strcmp(json_string_value(x), s) == 0) {
                        seen = 1;
                        break;
                    }
                }
            }
            if (!seen)
                json_array_append(other, json_copy(m));
        }
    }
    return head;
}

/* PoP: featured_first @ hermes_cli/model_picker_usage.py:featured_first */
json_t *mpu_featured_first(json_t *model_ids, json_t *featured)
{
    json_t *rest = NULL;
    json_t *head = mpu_split_recommended(model_ids, featured, &rest);
    json_t *out = json_new_array();
    if (json_is_array(head)) {
        size_t n = json_len(head);
        for (size_t i = 0; i < n; i++)
            json_array_append(out, json_copy(json_get(head, i)));
    }
    json_free(head);
    if (json_is_array(rest)) {
        size_t n = json_len(rest);
        for (size_t i = 0; i < n; i++)
            json_array_append(out, json_copy(json_get(rest, i)));
    }
    json_free(rest);
    return out;
}

/* PoP: _credits_pay @ hermes_cli/model_picker_usage.py:_credits_pay */
int mpu_credits_pay(json_t *info, json_t *window)
{
    json_t *keep = mpu_obj_get(window, "keep_going_on_credits", NULL);
    if (json_is_bool(keep) && !json_bool_value(keep))
        return 0;
    json_t *psa = mpu_obj_get(info, "paid_service_access_info", NULL);
    json_t *purchased = mpu_obj_get(psa, "purchased_credits_remaining", NULL);
    if (!json_is_number(purchased) || json_is_bool(purchased))
        return -1;
    return json_number_value(purchased) > 0.0 ? 1 : 0;
}

/* PoP: is_free_model @ hermes_cli/model_picker_usage.py:is_free_model */
int mpu_is_free_model(json_t *pricing_row)
{
    if (!json_is_object(pricing_row))
        return 0;
    double inp = 0, out = 0;
    json_t *p = json_obj_get(pricing_row, "prompt");
    json_t *c = json_obj_get(pricing_row, "completion");
    int has_in = mp_price_float(json_is_string(p) ? json_string_value(p) : NULL,
                                0, &inp);
    int has_out = mp_price_float(json_is_string(c) ? json_string_value(c) : NULL,
                                 0, &out);
    return has_in && has_out && inp <= 0.0 && out <= 0.0;
}

/* PoP: _capitalize @ hermes_cli/model_picker_usage.py:_capitalize */
void mpu_capitalize(const char *text, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    if (!text || !*text) {
        out[0] = '\0';
        return;
    }
    size_t n = strlen(text);
    if (n >= outsz)
        n = outsz - 1;
    out[0] = (char)toupper((unsigned char)text[0]);
    memcpy(out + 1, text + 1, n - 1);
    out[n] = '\0';
}

/* PoP: _get @ hermes_cli/model_picker_usage.py:_get */
json_t *mpu_obj_get(json_t *obj, const char *name, json_t *dflt)
{
    if (!obj || json_is_null(obj))
        return dflt;
    if (json_is_object(obj)) {
        json_t *v = json_obj_get(obj, name);
        return v ? v : dflt;
    }
    return dflt;
}
