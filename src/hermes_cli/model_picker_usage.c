/*
 * model_picker_usage.c -- faithful C11 port of the reusable plan-usage chrome
 * helpers in hermes_cli/model_picker_usage.py (wave17 gate w17_modelpicke_b36083).
 *
 * PoP: w17_modelpicke_b36083_clamp_fraction @ hermes_cli/model_picker_usage.py:clamp_fraction
 * PoP: w17_modelpicke_b36083_gauge_style @ hermes_cli/model_picker_usage.py:gauge_style
 * PoP: w17_modelpicke_b36083_filled_cells @ hermes_cli/model_picker_usage.py:_filled_cells
 * PoP: w17_modelpicke_b36083_render_bar @ hermes_cli/model_picker_usage.py:render_bar
 * PoP: w17_modelpicke_b36083_format_percent_used @ hermes_cli/model_picker_usage.py:format_percent_used
 * PoP: w17_modelpicke_b36083_compact_duration @ hermes_cli/model_picker_usage.py:compact_duration
 * PoP: w17_modelpicke_b36083_format_resets_in @ hermes_cli/model_picker_usage.py:format_resets_in
 * PoP: w17_modelpicke_b36083_split_recommended @ hermes_cli/model_picker_usage.py:split_recommended
 * PoP: w17_modelpicke_b36083_featured_first @ hermes_cli/model_picker_usage.py:featured_first
 * PoP: w17_modelpicke_b36083_credits_pay @ hermes_cli/model_picker_usage.py:_credits_pay
 * PoP: w17_modelpicke_b36083_is_free_model @ hermes_cli/model_picker_usage.py:is_free_model
 * PoP: w17_modelpicke_b36083_capitalize @ hermes_cli/model_picker_usage.py:_capitalize
 * PoP: w17_modelpicke_b36083_obj_get @ hermes_cli/model_picker_usage.py:_get
 */
#include "noc_w17_modelpicke_b36083.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAR_WIDTH 10
#define BAR_FILL '|'
#define BAR_EMPTY ' '
#define YELLOW_BELOW 0.5
#define RED_BELOW 0.2
#define BP_FULL 10000.0

/* Python round(): banker's rounding, half to even. Inputs here are >= 0. */
static double py_round(double x)
{
    double f = (double)(long long)x;
    double diff = x - f;
    if (diff < 0.5) return f;
    if (diff > 0.5) return f + 1.0;
    return (((long long)f) % 2 == 0) ? f : f + 1.0;
}

static double py_round_to(double x, double scale)
{
    return py_round(x * scale) / scale;
}

/* JSON scalar -> long when it is an int-like value (bool counts as int in Python). */
static int json_as_long(const json_t *v, long long *out)
{
    if (json_is_bool(v)) { *out = json_bool_value(v) ? 1 : 0; return 1; }
    if (json_is_number(v)) {
        double d = json_number_value(v);
        if (d != (double)(long long)d) return 0;
        *out = (long long)d;
        return 1;
    }
    return 0;
}

/* PoP: w17_modelpicke_b36083_clamp_fraction @ hermes_cli/model_picker_usage.py:clamp_fraction */
double w17_modelpicke_b36083_clamp_fraction(double value)
{
    if (value < 0.0) return 0.0;
    return value < 1.0 ? value : 1.0;
}

/* PoP: w17_modelpicke_b36083_gauge_style @ hermes_cli/model_picker_usage.py:gauge_style */
const char *w17_modelpicke_b36083_gauge_style(double left_fraction)
{
    double left = w17_modelpicke_b36083_clamp_fraction(left_fraction);
    if (left < RED_BELOW) return "red";
    if (left < YELLOW_BELOW) return "yellow";
    return "green";
}

/* PoP: w17_modelpicke_b36083_filled_cells @ hermes_cli/model_picker_usage.py:_filled_cells */
long w17_modelpicke_b36083_filled_cells(double fraction, long width)
{
    double value = w17_modelpicke_b36083_clamp_fraction(fraction);
    long filled = (long)py_round(value * (double)width);
    if (value > 0.0 && filled == 0) filled = 1;
    if (value < 1.0 && filled == width) filled = width - 1;
    return filled;
}

/* PoP: w17_modelpicke_b36083_render_bar @ hermes_cli/model_picker_usage.py:render_bar */
void w17_modelpicke_b36083_render_bar(double fraction, long width, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    if (width < 0) width = 0;
    long filled = w17_modelpicke_b36083_filled_cells(fraction, width);
    size_t cap = outsz - 1, p = 0;
    if (p < cap) out[p++] = '[';
    for (long i = 0; i < filled && p < cap; i++) out[p++] = BAR_FILL;
    for (long i = filled; i < width && p < cap; i++) out[p++] = BAR_EMPTY;
    if (p < cap) out[p++] = ']';
    out[p] = '\0';
}

/* PoP: w17_modelpicke_b36083_format_percent_used @ hermes_cli/model_picker_usage.py:format_percent_used */
void w17_modelpicke_b36083_format_percent_used(double used_fraction, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    long bp = (long)py_round_to(w17_modelpicke_b36083_clamp_fraction(used_fraction) * BP_FULL, 1e6);
    if (bp <= 0) snprintf(out, outsz, "0%%");
    else if (bp < 100) snprintf(out, outsz, "<1%%");
    else snprintf(out, outsz, "%ld%%", bp / 100);
}

/* PoP: w17_modelpicke_b36083_compact_duration @ hermes_cli/model_picker_usage.py:compact_duration */
void w17_modelpicke_b36083_compact_duration(json_t *resets_at_ms, json_t *now_ms,
                                            char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    out[0] = '\0';
    long long resets;
    if (!json_as_long(resets_at_ms, &resets)) return;   /* not isinstance(int) -> "" */
    long long now = 0;
    if (!now_ms || !json_as_long(now_ms, &now)) return;  /* no wall clock in the port */
    long long seconds = (resets - now) / 1000;
    if (seconds < 0) seconds = 0;
    if (seconds < 60) return;
    long long days = seconds / 86400;
    long long rem = seconds % 86400;
    long long hours = rem / 3600;
    long long minutes = (rem % 3600) / 60;
    if (days) {
        if (hours) snprintf(out, outsz, "%lldd %lldh", days, hours);
        else snprintf(out, outsz, "%lldd", days);
    } else if (hours) {
        snprintf(out, outsz, "%lldh", hours);
    } else {
        snprintf(out, outsz, "%lldm", minutes);
    }
}

/* PoP: w17_modelpicke_b36083_format_resets_in @ hermes_cli/model_picker_usage.py:format_resets_in */
void w17_modelpicke_b36083_format_resets_in(json_t *resets_at_ms, json_t *now_ms,
                                            char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    char dur[96];
    w17_modelpicke_b36083_compact_duration(resets_at_ms, now_ms, dur, sizeof(dur));
    if (dur[0]) snprintf(out, outsz, "resets in %s", dur);
    else snprintf(out, outsz, "resets soon");
}

static int array_contains_string(json_t *arr, const char *s)
{
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *x = json_get(arr, i);
        if (json_is_string(x)) {
            const char *xs = json_string_value(x);
            if (xs && strcmp(xs, s) == 0) return 1;
        }
    }
    return 0;
}

/* PoP: w17_modelpicke_b36083_split_recommended @ hermes_cli/model_picker_usage.py:split_recommended */
json_t *w17_modelpicke_b36083_split_recommended(json_t *model_ids, json_t *featured,
                                                json_t **rest)
{
    json_t *head = json_array();
    json_t *other = json_array();
    if (rest) *rest = other;
    if (!json_is_array(model_ids)) return head;

    /* head = [m for m in dict.fromkeys(featured or ()) if m in listed] */
    if (json_is_array(featured)) {
        for (size_t i = 0; i < json_len(featured); i++) {
            json_t *m = json_get(featured, i);
            if (!json_is_string(m)) continue;
            const char *s = json_string_value(m);
            if (!array_contains_string(model_ids, s)) continue;   /* m in listed */
            if (array_contains_string(head, s)) continue;         /* dict.fromkeys dedupe */
            json_append(head, json_string(s));
        }
    }
    /* seen = set(head); rest = [m for m in model_ids if m not in seen] */
    for (size_t i = 0; i < json_len(model_ids); i++) {
        json_t *m = json_get(model_ids, i);
        const char *s = json_is_string(m) ? json_string_value(m) : NULL;
        if (s && array_contains_string(head, s)) continue;
        json_append(other, json_copy(m));
    }
    return head;
}

/* PoP: w17_modelpicke_b36083_featured_first @ hermes_cli/model_picker_usage.py:featured_first */
json_t *w17_modelpicke_b36083_featured_first(json_t *model_ids, json_t *featured)
{
    json_t *rest = NULL;
    json_t *head = w17_modelpicke_b36083_split_recommended(model_ids, featured, &rest);
    json_t *out = json_array();
    if (json_is_array(head))
        for (size_t i = 0; i < json_len(head); i++) json_append(out, json_copy(json_get(head, i)));
    json_free(head);
    if (json_is_array(rest))
        for (size_t i = 0; i < json_len(rest); i++) json_append(out, json_copy(json_get(rest, i)));
    json_free(rest);
    return out;
}

/* PoP: w17_modelpicke_b36083_obj_get @ hermes_cli/model_picker_usage.py:_get */
json_t *w17_modelpicke_b36083_obj_get(json_t *obj, const char *name, json_t *dflt)
{
    if (!obj || json_is_null(obj)) return dflt;
    if (json_is_object(obj)) {
        json_t *v = json_obj_get(obj, name);
        return v ? v : dflt;
    }
    return dflt;
}

/* models_pricing._price_float(raw, positive=False): None for None/""/invalid/NaN/negative. */
static int price_float(json_t *raw, double *out)
{
    if (!raw || json_is_null(raw)) return 0;
    const char *s = NULL;
    char numbuf[64];
    if (json_is_string(raw)) s = json_string_value(raw);
    else if (json_is_number(raw)) { snprintf(numbuf, sizeof(numbuf), "%.17g", json_number_value(raw)); s = numbuf; }
    else if (json_is_bool(raw)) { s = json_bool_value(raw) ? "1" : "0"; }
    if (!s || !*s) return 0;
    char *end = NULL;
    double d = strtod(s, &end);
    if (end == s || (end && *end)) return 0;
    if (d != d || d < 0.0) return 0;
    *out = d;
    return 1;
}

/* PoP: w17_modelpicke_b36083_is_free_model @ hermes_cli/model_picker_usage.py:is_free_model */
int w17_modelpicke_b36083_is_free_model(json_t *pricing_row)
{
    if (!json_is_object(pricing_row) || json_object_size(pricing_row) == 0) return 0;
    double inp = 0.0, out = 0.0;
    int has_in = price_float(json_obj_get(pricing_row, "prompt"), &inp);
    int has_out = price_float(json_obj_get(pricing_row, "completion"), &out);
    return has_in && has_out && inp <= 0.0 && out <= 0.0;
}

/* PoP: w17_modelpicke_b36083_credits_pay @ hermes_cli/model_picker_usage.py:_credits_pay */
int w17_modelpicke_b36083_credits_pay(json_t *info, json_t *window)
{
    json_t *keep = w17_modelpicke_b36083_obj_get(window, "keep_going_on_credits", NULL);
    if (json_is_bool(keep) && !json_bool_value(keep)) return 0;  /* `is False` */
    json_t *psa = w17_modelpicke_b36083_obj_get(info, "paid_service_access_info", NULL);
    json_t *purchased = w17_modelpicke_b36083_obj_get(psa, "purchased_credits_remaining", NULL);
    if (!json_is_number(purchased) || json_is_bool(purchased)) return -1;
    return json_number_value(purchased) > 0.0 ? 1 : 0;
}

/* PoP: w17_modelpicke_b36083_capitalize @ hermes_cli/model_picker_usage.py:_capitalize */
void w17_modelpicke_b36083_capitalize(const char *text, char *out, size_t outsz)
{
    if (!out || outsz == 0) return;
    if (!text || !*text) { out[0] = '\0'; return; }
    size_t n = strlen(text);
    if (n >= outsz) n = outsz - 1;
    out[0] = (char)toupper((unsigned char)text[0]);
    memcpy(out + 1, text + 1, n - 1);
    out[n] = '\0';
}

