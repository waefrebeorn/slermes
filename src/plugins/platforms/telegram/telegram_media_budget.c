/*
 * telegram_media_budget.c -- real C11 port of the size-scaled upload budgets of
 * upstream Hermes plugins/platforms/telegram/telegram_media_budget.py, plus the
 * two gateway.platforms.helpers() functions it calls.
 *
 * PoP: w21_telegramme_52d000_media_sources @ plugins/platforms/telegram/telegram_media_budget.py:media_sources
 * PoP: w21_telegramme_52d000_upload_seconds @ gateway/platforms/helpers.py:media_upload_seconds
 * PoP: w21_telegramme_52d000_scaled_deadline @ gateway/platforms/helpers.py:scaled_upload_deadline
 * PoP: w21_telegramme_52d000_scaled_budgets @ plugins/platforms/telegram/telegram_media_budget.py:scaled_budgets
 *
 * The only seam is `_on_disk_size` (os.stat/os.fstat): the caller hands the
 * resolved byte counts in, exactly the numbers the live helper sums.
 */
#include "noc_w21_telegramme_52d000.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* telegram_media_budget._MEDIA_KEYS, in source order. */
static const char *const MEDIA_KEYS[] = {
    "animation", "audio", "document", "photo", "sticker",
    "video", "video_note", "voice", "thumbnail"
};
static const size_t MEDIA_KEY_COUNT = sizeof(MEDIA_KEYS) / sizeof(MEDIA_KEYS[0]);

json_t *w21_telegramme_52d000_media_sources(const json_t *media_kwargs)
{
    json_t *out = json_array();
    size_t i;

    if (!out) return NULL;
    for (i = 0; i < MEDIA_KEY_COUNT; i++) {
        const json_t *v = (media_kwargs && media_kwargs->type == JSON_OBJECT)
                              ? json_obj_get(media_kwargs, MEDIA_KEYS[i])
                              : NULL;
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}

/* The on-disk byte count of one source; a URL, file id or unmapped value is 0. */
static double on_disk_size(const json_t *v)
{
    if (!v || v->type != JSON_NUMBER) return 0.0;
    if (!isfinite(v->num_val)) return 0.0;
    return v->num_val;
}

json_t *w21_telegramme_52d000_upload_seconds(const json_t *sizes)
{
    double total = 0.0;
    size_t i, n;

    if (sizes && sizes->type == JSON_ARRAY && sizes->c.items) {
        n = sizes->c.count;
        for (i = 0; i < n; i++) total += on_disk_size(sizes->c.items[i]);
    }
    return json_number(total / W21_TELEGRAMME_52D000_MIN_UPLOAD_RATE);
}

json_t *w21_telegramme_52d000_scaled_deadline(double deadline, double upload_seconds)
{
    double widened;

    if (!(upload_seconds > 0.0)) return json_number(deadline);
    widened = deadline + 1.5 * upload_seconds;
    if (widened > W21_TELEGRAMME_52D000_DEADLINE_MAX) widened = W21_TELEGRAMME_52D000_DEADLINE_MAX;
    return json_number(widened > deadline ? widened : deadline);
}

json_t *w21_telegramme_52d000_scaled_budgets(const json_t *sizes, double deadline,
                                             double read_timeout)
{
    json_t *timeouts, *out, *seconds;
    double extra;

    seconds = w21_telegramme_52d000_upload_seconds(sizes);
    extra = seconds ? seconds->num_val : 0.0;
    json_free(seconds);

    timeouts = json_object();
    out = json_object();
    if (!timeouts || !out) {
        json_free(timeouts);
        json_free(out);
        return NULL;
    }
    if (!(extra > 0.0)) {
        json_set(timeouts, "read_timeout", json_number(read_timeout));
        json_set(out, "timeouts", timeouts);
        json_set(out, "deadline", json_number(deadline));
        return out;
    }
    json_set(timeouts, "read_timeout", json_number(read_timeout + extra));
    json_set(timeouts, "write_timeout",
             json_number(W21_TELEGRAMME_52D000_MEDIA_WRITE_TIMEOUT + extra));
    json_set(out, "timeouts", timeouts);
    json_set(out, "deadline", w21_telegramme_52d000_scaled_deadline(deadline, extra));
    return out;
}
