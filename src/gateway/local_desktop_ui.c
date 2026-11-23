/*
 * local_desktop_ui.c — C home for upstream Hermes gateway/local_desktop_ui.py.
 *
 * PoP: w10_localdeskt_b6c610_theme @ gateway/local_desktop_ui.py:_theme
 * PoP: w10_localdeskt_b6c610_read_block @ gateway/local_desktop_ui.py:_read_block
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_localdeskt_b6c610.h"

/* Mirrors _theme given injected _ask / _ask_result results and clock:
 * no "picker" key -> theme.apply: empty answer -> _THEME_UNANSWERED,
 *   json.loads(answer) on success, else {"success":True,"text":answer}.
 * "picker" -> theme.pick: None -> timed_out/cancelled by elapsed vs 300s;
 *   dict "picked" -> success; nonblank str "said" -> stripped said; else cancelled. */
json_t *w10_localdeskt_b6c610_theme(const json_t *in) {
    const json_t *payload = json_obj_get(in, "payload");
    const json_t *picker = payload ? json_obj_get(payload, "picker") : NULL;
    if (!picker) {
        const char *answer = json_get_str(in, "apply_answer", "");
        if (!answer || !*answer) {
            json_t *o = json_object();
            json_set(o, "success", json_bool(0));
            json_set(o, "error", json_string(
                "No Hermes Desktop window answered the theme request."));
            return o;
        }
        json_t *parsed = json_parse(answer, NULL);   /* json.loads(answer) */
        if (parsed) return parsed;
        json_t *o = json_object();
        json_set(o, "success", json_bool(1));
        json_set(o, "text", json_string(answer));
        return o;
    }

    const json_t *pick = json_obj_get(in, "pick");
    double elapsed = 0.0;
    const json_t *ev = json_obj_get(in, "elapsed");
    if (ev && ev->type == JSON_NUMBER) elapsed = ev->num_val;

    if (!pick || pick->type == JSON_NULL) {
        json_t *o = json_object();
        json_set(o, elapsed >= 300.0 ? "timed_out" : "cancelled", json_bool(1));
        return o;
    }
    if (pick->type == JSON_OBJECT) {
        const json_t *picked = json_obj_get(pick, "picked");
        if (picked && picked->type == JSON_OBJECT) {
            json_t *o = json_object();
            json_set(o, "success", json_bool(1));
            json_set(o, "picked", json_copy(picked));
            return o;
        }
        const json_t *said = json_obj_get(pick, "said");
        if (said && said->type == JSON_STRING && said->str_val) {
            const char *s = said->str_val;
            const char *e = s + strlen(s);
            while (s < e && isspace((unsigned char)*s)) s++;
            while (e > s && isspace((unsigned char)e[-1])) e--;
            if (e > s) {
                size_t len = (size_t)(e - s);
                char *buf = (char *)malloc(len + 1);
                if (buf) {
                    memcpy(buf, s, len);
                    buf[len] = '\0';
                    json_t *o = json_object();
                    json_set(o, "said", json_string(buf));
                    free(buf);
                    return o;
                }
            }
        }
    }
    json_t *o = json_object();
    json_set(o, "cancelled", json_bool(1));
    return o;
}

/* Mirrors _read_block's lambda param filter:
 * {k: v for k, v in (("start", start), ("count", count)) if v is not None}.
 * 0 is kept; only null/absent is dropped. */
json_t *w10_localdeskt_b6c610_read_block(const json_t *in) {
    const json_t *start = json_obj_get(in, "start");
    const json_t *count = json_obj_get(in, "count");
    json_t *out = json_object();
    if (start && start->type != JSON_NULL) json_set(out, "start", json_copy(start));
    if (count && count->type != JSON_NULL) json_set(out, "count", json_copy(count));
    return out;
}
