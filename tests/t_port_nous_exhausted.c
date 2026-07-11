/*
 * t_port_nous_exhausted.c — Oracle harness for
 * agent/nous_rate_guard.py:_has_exhausted_bucket_in_object
 * (ported to src/agent/nous_rate_guard.c as
 *  nous_has_exhausted_bucket_in_object).
 *
 * Builds JSON state objects with bucket sub-objects under the four keys
 * (requests_min / requests_hour / tokens_min / tokens_hour) and emits the
 * bool result. The Python oracle replays equivalent SimpleNamespace states.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>

extern bool nous_has_exhausted_bucket_in_object(json_t *state);

static void emit(const char *name, const char *state_json)
{
    json_t *st = json_parse(state_json ? state_json : "{}", NULL);
    int r = nous_has_exhausted_bucket_in_object(st);
    if (st) json_free(st);
    printf("{\"case\":\"%s\",\"ret\":%s}\n", name, r ? "true" : "false");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* no buckets -> false */
    emit("no_buckets", "{}");
    /* all buckets have remaining>0 -> false */
    emit("all_remaining",
        "{\"requests_min\":{\"limit\":10,\"remaining\":5,\"reset_seconds\":120},"
         "\"tokens_hour\":{\"limit\":100,\"remaining\":50,\"reset_seconds\":120}}");
    /* one bucket remaining==0 but limit<=0 -> skipped -> false */
    emit("zero_limit",
        "{\"requests_min\":{\"limit\":0,\"remaining\":0,\"reset_seconds\":120}}");
    /* one bucket remaining==0, limit>0, reset>=60 -> true */
    emit("exhausted_ok",
        "{\"requests_hour\":{\"limit\":10,\"remaining\":0,\"reset_seconds\":120}}");
    /* remaining==0 but reset < 60 -> false */
    emit("exhausted_short_reset",
        "{\"tokens_min\":{\"limit\":10,\"remaining\":0,\"reset_seconds\":30}}");
    /* uses remaining_seconds_now when present */
    emit("remaining_seconds_now",
        "{\"tokens_hour\":{\"limit\":5,\"remaining\":0,\"remaining_seconds_now\":90,\"reset_seconds\":30}}");
    return 0;
}
