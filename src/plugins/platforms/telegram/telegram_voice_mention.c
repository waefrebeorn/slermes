/*
 * telegram_voice_mention.c -- real C11 port of upstream Hermes
 *   plugins/platforms/telegram/telegram_voice_mention.py.
 *
 * PoP: w21_telegramvo_2f1ab7_admit_media @ plugins/platforms/telegram/telegram_voice_mention.py:admit_media
 * PoP: w21_telegramvo_2f1ab7_cache_inbound_voice @ plugins/platforms/telegram/telegram_voice_mention.py:cache_inbound_voice
 * PoP: w21_telegramvo_2f1ab7_env_probe @ plugins/platforms/telegram/telegram_voice_mention.py:admit_media
 *
 * The two coroutines only orchestrate I/O; every decision they make is here.
 * fetch_telegram_file / cache_audio_from_bytes_async / spoken_mention_clips are
 * the network+STT seams and arrive as `world` fields; the opt-in check is the
 * real is_truthy_value rule on adapter.config.extra["voice_mention_patterns"].
 */
#include "hermes_json.h"
#include "noc_w21_telegramvo_2f1ab7.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness of a decoded JSON value; absent (NULL) reads as None. */
static int tvo_truthy(const json_t *v) {
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

/* utils.is_truthy_value(value, default=False): None -> default, str -> the
 * project's truthy string set, anything else -> bool(value). */
static int tvo_is_truthy_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        char buf[64];
        size_t i, n;
        const char *s = v->str_val ? v->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        n = strlen(s);
        while (n > 0) {
            char c = s[n - 1];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') n--;
            else break;
        }
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        for (i = 0; i < n; i++) {
            char c = s[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            buf[i] = c;
        }
        buf[n] = '\0';
        return !strcmp(buf, "1") || !strcmp(buf, "true") || !strcmp(buf, "yes") || !strcmp(buf, "on");
    }
    return tvo_truthy(v);
}

json_t *w21_telegramvo_2f1ab7_admit_media(const json_t *world) {
    json_t *clips, *out;
    size_t i;

    if (!world || world->type != JSON_OBJECT) return json_null();

    /* if adapter._should_process_message(msg): return [] */
    if (tvo_truthy(json_obj_get(world, "should_process"))) return json_array();

    /* voice is None or caption or not _mention_patterns or not
     * voice_mention_gate_enabled(adapter.config.extra) or not
     * _should_process_message(msg, spoken_mention=True): return None */
    if (!tvo_truthy(json_obj_get(world, "has_voice"))) return json_null();
    if (tvo_truthy(json_obj_get(world, "caption"))) return json_null();
    if (!tvo_truthy(json_obj_get(world, "mention_patterns"))) return json_null();
    if (!tvo_is_truthy_value(json_obj_get(world, "gate_extra"))) return json_null();
    if (!tvo_truthy(json_obj_get(world, "spoken_ok"))) return json_null();

    /* allowed, _note = adapter._telegram_media_size_allowed(voice, "voice message") */
    if (!tvo_truthy(json_obj_get(world, "media_allowed"))) return json_null();

    /* try: fetch + cache except Exception: return None */
    if (!tvo_truthy(json_obj_get(world, "fetch_ok"))) return json_null();

    /* clips = await spoken_mention_clips(...); return clips or None */
    clips = json_obj_get(world, "clips");
    if (!clips || clips->type != JSON_ARRAY || clips->c.count == 0) return json_null();
    out = json_array();
    for (i = 0; i < clips->c.count; i++) {
        /* `clips or None`: any non-empty list is returned as it stands */
        json_append(out, json_copy(clips->c.items[i]));
    }
    return out;
}

json_t *w21_telegramvo_2f1ab7_cache_inbound_voice(const json_t *world) {
    json_t *clips, *out;
    size_t i;

    if (!world || world->type != JSON_OBJECT) world = NULL;
    clips = world ? json_obj_get(world, "clips") : NULL;

    /* if not clips: await adapter._cache_inbound_av(msg, event, msg.voice,
     * "voice message", "voice", ".ogg", "audio/ogg"); return */
    if (!tvo_truthy(clips)) {
        out = json_object();
        json_set(out, "status", json_string("download"));
        json_set(out, "ext", json_string(".ogg"));
        json_set(out, "mime", json_string("audio/ogg"));
        json_set(out, "kind", json_string("voice"));
        json_set(out, "label", json_string("voice message"));
        return out;
    }

    /* event.media_urls, event.media_types = list(clips), ["audio/ogg"] * len(clips) */
    out = json_object();
    json_set(out, "status", json_string("attach"));
    {
        json_t *urls = json_array();
        json_t *types = json_array();
        if (clips->type == JSON_ARRAY) {
            for (i = 0; i < clips->c.count; i++) {
                json_t *v = clips->c.items[i];
                const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
                json_append(urls, json_string(s));
                json_append(types, json_string("audio/ogg"));
            }
        }
        json_set(out, "media_urls", urls);
        json_set(out, "media_types", types);
    }
    json_set(out, "reply_expected", json_bool(1));
    return out;
}

json_t *w21_telegramvo_2f1ab7_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
