/*
 * thumbnail.c -- real C11 port of upstream Hermes tools/bot_desktop/thumbnail.py.
 *
 * PoP: w21_thumbnail_f864e8_thumbnail_data_url @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 * PoP: w21_thumbnail_f864e8_thumbnail_size @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 * PoP: w21_thumbnail_f864e8_data_url @ tools/bot_desktop/thumbnail.py:thumbnail_data_url
 *
 * The runtime probes, the screen grab (PIL ImageGrab on the host, or the
 * sandbox's own grab) and the JPEG encoder are seams. The thumbnail arithmetic
 * is Pillow's `Image.thumbnail`: floor the box, then pick the floor/ceil of the
 * scaled side that lands closest to the source aspect (never below 1), and
 * never upscale.
 */
#include "noc_w21_thumbnail_f864e8.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

static int th_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* Pillow's round_aspect: min(floor(n), ceil(n), key=key) then max(..., 1). */
static long th_round_aspect(double number, int pick_x, double aspect, double other)
{
    long lo = (long)floor(number);
    long hi = (long)ceil(number);
    long pick = lo;

    if (hi != lo) {
        double key_lo = pick_x ? fabs(aspect - (double)lo / other)
                               : (lo == 0 ? 0.0 : fabs(aspect - other / (double)lo));
        double key_hi = pick_x ? fabs(aspect - (double)hi / other)
                               : (hi == 0 ? 0.0 : fabs(aspect - other / (double)hi));
        pick = (key_hi < key_lo) ? hi : lo;
    }
    if (pick < 1) pick = 1;
    return pick;
}

json_t *w21_thumbnail_f864e8_thumbnail_size(long long width, long long height,
                                            long long max_width, long long max_height)
{
    double aspect;
    double box_w = (double)max_width;
    double box_h = (double)max_height;
    long long final_w, final_h;
    json_t *out;

    if (height <= 0 || max_width <= 0 || max_height <= 0) return json_null();
    if (box_w >= (double)width && box_h >= (double)height) {
        final_w = width;
        final_h = height;
    } else {
        aspect = (double)width / (double)height;
        final_w = max_width;
        final_h = max_height;
        if (box_w / box_h >= aspect) {
            final_w = th_round_aspect(box_h * aspect, 1, aspect, box_h);
        } else {
            final_h = th_round_aspect(box_w / aspect, 0, aspect, box_w);
        }
    }
    out = json_array();
    json_append(out, json_number((double)final_w));
    json_append(out, json_number((double)final_h));
    return out;
}

char *w21_thumbnail_f864e8_data_url(const unsigned char *bytes, size_t len)
{
    static const char PREFIX[] = "data:image/jpeg;base64,";
    char *encoded;
    char *out;
    size_t need;

    if (!bytes || len == 0) return NULL;
    encoded = base64_encode(bytes, len);
    if (!encoded) return NULL;
    need = sizeof(PREFIX) + strlen(encoded);
    out = (char *)malloc(need);
    if (!out) { free(encoded); return NULL; }
    snprintf(out, need, "%s%s", PREFIX, encoded);
    free(encoded);
    return out;
}

/* fixture hex -> raw bytes (the grabbed/encoded payload, carried as text). */
static unsigned char *th_hex_decode(const char *hex, size_t *out_len)
{
    size_t n, i;
    unsigned char *out;

    if (!hex) return NULL;
    n = strlen(hex);
    if (n == 0 || (n % 2) != 0) return NULL;
    out = (unsigned char *)malloc(n / 2);
    if (!out) return NULL;
    for (i = 0; i < n / 2; i++) {
        int hi = hex[2 * i], lo = hex[2 * i + 1];
        int hv = (hi >= '0' && hi <= '9') ? hi - '0'
                 : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                 : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
        int lv = (lo >= '0' && lo <= '9') ? lo - '0'
                 : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                 : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
        if (hv < 0 || lv < 0) { free(out); return NULL; }
        out[i] = (unsigned char)((hv << 4) | lv);
    }
    *out_len = n / 2;
    return out;
}

static json_t *th_none(void)
{
    json_t *out = json_object();
    json_set(out, "status", json_string("none"));
    return out;
}

/* The `"data:image/jpeg;base64," + ...` tail once the grab handed us bytes. */
static json_t *th_data_result(const char *hex, const char *branch,
                              const json_t *image_size)
{
    size_t len = 0;
    unsigned char *bytes = th_hex_decode(hex, &len);
    char *url;
    json_t *out;

    if (!bytes) return th_none();
    url = w21_thumbnail_f864e8_data_url(bytes, len);
    free(bytes);
    if (!url) return th_none();
    out = json_object();
    json_set(out, "status", json_string("data"));
    json_set(out, "value", json_string(url));
    json_set(out, "branch", json_string(branch));
    json_set(out, "size", image_size ? json_copy(image_size) : json_null());
    free(url);
    return out;
}

json_t *w21_thumbnail_f864e8_thumbnail_data_url(const json_t *kase)
{
    const json_t *env, *display, *owned, *launcher, *image, *max_size, *jpeg;
    long long max_w = W21_THUMBNAIL_F864E8_THUMB_MAX_W;
    long long max_h = W21_THUMBNAIL_F864E8_THUMB_MAX_H;

    if (!kase || kase->type != JSON_OBJECT) return NULL;
    env = json_obj_get(kase, "env");
    display = (env && env->type == JSON_OBJECT) ? json_obj_get(env, "DISPLAY") : NULL;
    if (!th_truthy(display)) return th_none();

    if (th_truthy(json_obj_get(kase, "sandbox_running"))) {
        owned = json_obj_get(kase, "owned_sandbox");
        jpeg = (owned && owned->type != JSON_NULL) ? json_obj_get(kase, "sandbox_jpeg_hex") : NULL;
        return th_data_result((jpeg && jpeg->type == JSON_STRING) ? jpeg->str_val : NULL,
                              "sandbox", NULL);
    }

    launcher = json_obj_get(kase, "launcher_pid");
    if (!launcher || launcher->type == JSON_NULL) return th_none();

    max_size = json_obj_get(kase, "max_size");
    if (max_size && max_size->type == JSON_ARRAY && max_size->c.count >= 2) {
        if (max_size->c.items[0] && max_size->c.items[0]->type == JSON_NUMBER)
            max_w = (long long)max_size->c.items[0]->num_val;
        if (max_size->c.items[1] && max_size->c.items[1]->type == JSON_NUMBER)
            max_h = (long long)max_size->c.items[1]->num_val;
    }

    image = json_obj_get(kase, "image");
    if (!image || image->type != JSON_OBJECT) return th_none();
    {
        long long width = (long long)json_get_num(image, "width", 0.0);
        long long height = (long long)json_get_num(image, "height", 0.0);
        json_t *size = w21_thumbnail_f864e8_thumbnail_size(width, height, max_w, max_h);
        json_t *out = th_data_result(json_get_str(kase, "jpeg_hex", NULL), "grab", size);
        json_free(size);
        return out;
    }
}
