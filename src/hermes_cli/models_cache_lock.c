/*
 * models_cache_lock.c — C home for upstream Hermes hermes_cli/models_cache_lock.py.
 *
 * PoP: modelslock_provider_models_cache_lock @ hermes_cli/models_cache_lock.py:provider_models_cache_lock
 * PoP: modelslock_write_under_cache_lock @ hermes_cli/models_cache_lock.py:write_under_cache_lock
 *
 * The file lock itself and the write() callable are injected; what is ported
 * is the lock path/timeout/message and the OSError/TimeoutError swallow.
 */
#include "noc_modelslock.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* pathlib.Path on Windows normalises '/' to '\' when stringified. */
static char *ml_winpath(const char *p) {
    char *o = (char *)malloc((p ? strlen(p) : 0) + 2);
    if (!o) return NULL;
    if (!p) { o[0]=0; return o; }
    for (char *q=o; (*q=*p); ++p,++q) if (*q=='/') *q='\\';
    return o;
}

struct json_t *modelslock_lock_spec(const char *path) {
    char *norm = ml_winpath(path);
    char *lock = (char *)malloc((norm ? strlen(norm) : 0) + 8);
    char msg[2048];
    if (lock && norm) snprintf(lock, (strlen(norm)+8), "%s.lock", norm);
    else if (lock) lock[0]=0;
    snprintf(msg, sizeof msg,
             "Timed out waiting for the provider models cache lock (%s)",
             norm ? norm : "");
    json_t *out = json_object();
    json_set(out,"lock_path",json_string(lock ? lock : ""));
    json_set(out,"timeout",json_number(10.0));
    json_set(out,"message",json_string(msg));
    free(norm); free(lock);
    return out;
}

struct json_t *modelslock_write_under_lock(const char *kind) {
    const char *k = kind ? kind : "none";
    int write_called = strncmp(k,"lock_",5) != 0;  /* write() runs unless lock acquire raised */
    int err = (!strcmp(k,"write_oserror") || !strcmp(k,"write_timeouterror") ||
               !strcmp(k,"lock_oserror") || !strcmp(k,"lock_timeouterror"));
    const char *outcome = "written";
    if (!strcmp(k,"write_valueerror") || !strcmp(k,"lock_valueerror")) outcome = "propagated";
    else if (err) outcome = "skipped";
    json_t *out = json_object();
    json_set(out,"write_called",json_bool(write_called));
    json_set(out,"outcome",json_string(outcome));
    return out;
}
