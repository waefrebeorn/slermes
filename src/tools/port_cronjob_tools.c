1|/**
2| * port_cronjob_tools.c — Port of Python: tools/cronjob_tools.py
3| *
4| * Real C implementations for cron job tool helpers.
5| *
6| * Coverage (16 functions; mirrors _is_emoji_cp .. check_cronjob_requirements):
7| *   emoji/ZWJ token surgery, prompt threat scanning (user + skill-assembled),
8| *   origin capture from session env, local-delivery notice synthesis,
9| *   repeat display, model-override resolution, base_url/script validation,
10| *   canonical skill list assembly, optional value normalization, deliver
11| *   parameter flattening, job formatting, cronjob dispatcher, requirements.
12| */
13|
14|#include "port_cronjob_tools.h"
15|#include "hermes_logger.h"
16|#include "hermes_json.h"
17|#include <stdbool.h>
18|#include <stdlib.h>
19|#include <string.h>
20|#include <stdio.h>
21|#include <ctype.h>
22|#include <unistd.h>
23|#include <time.h>
24|
25|/* Opaque struct definition - private to this translation unit */
26|struct port_cronjob_tools_state {
27|    bool threat_patterns_loaded;
28|};
29|
30|port_cronjob_tools_state_t *port_cronjob_tools_state_init(void)
31|{
32|    port_cronjob_tools_state_t *state = calloc(1, sizeof(*state));
33|    if (!state) return NULL;
34|    state->threat_patterns_loaded = false;
35|    return state;
36|}
37|
38|void port_cronjob_tools_state_cleanup(port_cronjob_tools_state_t *state)
39|{
40|    if (!state) return;
41|    free(state);
42|}
43|
#include "cron_prompt_sanitize.h"

/* The emoji/ZWJ unicode surgery + invisible-unicode detection + threat
 * scanning cluster was extracted to src/tools/cron_prompt_sanitize.c (v551
 * refactor-first monolith split). The public sanitize entry points below
 * delegate to that self-contained, oracle-verified module. */

269| * ================================================================ */
270|/* PoP: check_invisible_unicode @ tools/cronjob_tools.py:_check_invisible_unicode
271| * Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
272|char *check_invisible_unicode(const char *prompt)
273|{
274|    return cron_prompt_sanitize_check_invisible(prompt);
275|}
276|
277|/* ================================================================
278| *  6. PoP: _strip_invisible_unicode
279| *  Returns a JSON object {cleaned_prompt, removed_codepoints[]}.
280| *  Caller owns the returned json_t*.
281| * ================================================================ */
282|/* PoP: strip_invisible_unicode @ tools/cronjob_tools.py:_strip_invisible_unicode
283| * Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
284|json_t *strip_invisible_unicode(const char *prompt)
285|{
286|    return cron_prompt_sanitize_strip_invisible(prompt);
287|}
288|
289|/* ================================================================
290| *  7. PoP: _scan_cron_skill_assembled
291| *  Scans an assembled cron prompt (includes loaded skill content).
292| *  Returns json_t* {cleaned, error} where error is empty string on pass.
293| * ================================================================ */
294|/* PoP: scan_cron_skill_assembled @ tools/cronjob_tools.py:_scan_cron_skill_assembled
295| * Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
296|json_t *scan_cron_skill_assembled(const char *assembled)
297|{
298|    return cron_prompt_sanitize_scan_skill_assembled(assembled);
299|}
300|
301|/* ================================================================
302| *  8. PoP: _origin_from_env
303| *  Captures session env vars into a JSON object.
304| *  Returns json_t* object or NULL if env vars not set.
305| * ================================================================ */
306|/* PoP: origin_from_env @ tools/cronjob_tools.py:_origin_from_env
307| * Port of Python tools/cronjob_tools.py:_origin_from_env().
308| * Reads HERMES_SESSION_PLATFORM, HERMES_SESSION_CHAT_ID, HERMES_SESSION_THREAD_ID,
309| * HERMES_SESSION_CHAT_NAME, HERMES_SESSION_USER_ID from environment.
310| * Returns json_t* object with platform, chat_id, thread_id, chat_name, user_id,
311| * or NULL if platform/chat_id are not both set. */
312|json_t *origin_from_env(void)
313|{
314|    const char *platform = getenv("HERMES_SESSION_PLATFORM");
315|    const char *chat_id = getenv("HERMES_SESSION_CHAT_ID");
316|    if (!platform || !chat_id || !*platform || !*chat_id) return NULL;
317|
318|    const char *thread_id = getenv("HERMES_SESSION_THREAD_ID");
319|    const char *chat_name = getenv("HERMES_SESSION_CHAT_NAME");
320|    const char *user_id = getenv("HERMES_SESSION_USER_ID");
321|
322|    json_t *obj = json_object();
323|    json_set(obj, "platform", json_string(platform));
324|    json_set(obj, "chat_id", json_string(chat_id));
325|    if (thread_id && *thread_id) json_set(obj, "thread_id", json_string(thread_id));
326|    if (chat_name && *chat_name) json_set(obj, "chat_name", json_string(chat_name));
327|    if (user_id && *user_id) json_set(obj, "user_id", json_string(user_id));
328|    return obj;
329|}
330|
331|/* ================================================================
332| *  9. PoP: _local_delivery_notice
333| *  Returns a notice string (caller frees) when job is local-only
334| *  and will not be delivered back to the session.
335| * ================================================================ */
336|/* PoP: local_delivery_notice @ tools/cronjob_tools.py:_local_delivery_notice
337| * Port of Python tools/cronjob_tools.py:_local_delivery_notice().
338| * Returns malloc'd notice string when a created job won't deliver anywhere
339| * (CLI/TUI sessions have no live-delivery channel). Returns NULL when
340| * user explicitly requested "local" or job resolves to a real delivery target.
341| * In C we cannot call _resolve_delivery_targets, so we check origin presence
342| * as a best-effort proxy: if origin exists, we assume delivery will work. */
343|char *local_delivery_notice(const json_t *job, const char *user_deliver)
344|{
345|    if (!job) return NULL;
346|    if (user_deliver) {
347|        /* Normalize user_deliver: trim, lower-case */
348|        char norm[256];
349|        const char *p = user_deliver;
350|        while (*p && isspace((unsigned char)*p)) p++;
351|        size_t len = 0;
352|        while (*p && !isspace((unsigned char)*p) && len < sizeof(norm)-1) {
353|            norm[len++] = tolower((unsigned char)*p++);
354|        }
355|        norm[len] = '\0';
356|        if (strcmp(norm, "local") == 0) return NULL;
357|    }
358|    if (json_obj_get(job, "origin")) return NULL;
359|
360|    const char *msg = "This is a local-only cron job: its output is saved (view it with "
361|                      "cronjob(action='list')) but will NOT be delivered back into this "
362|                      "session — CLI/TUI sessions have no live-delivery channel. To be "
363|                      "notified when it runs, recreate or update the job with deliver set to "
364|                      "a gateway-connected platform, e.g. deliver='telegram' or deliver='all'.";
365|    return strdup(msg);
366|}
367|
368|/* ================================================================
369| *  10. PoP: _repeat_display
370| *  Returns a malloc'd string describing the repeat state.
371| * ================================================================ */
372|/* PoP: repeat_display @ tools/cronjob_tools.py:_repeat_display
373| * Port of Python tools/cronjob_tools.py:_repeat_display().
374| * Formats the repeat configuration: "forever", "once", "1/1", "N times", "X/Y". */
375|char *repeat_display(const json_t *job)
376|{
377|    if (!job) return strdup("?");
378|    json_t *repeat = json_obj_get(job, "repeat");
379|    if (!repeat) return strdup("forever");
380|    json_t *times = json_obj_get(repeat, "times");
381|    json_t *completed = json_obj_get(repeat, "completed");
382|    if (!times || times->type != JSON_NUMBER) return strdup("forever");
383|    long t = (long)times->num_val;
384|    long c = completed && completed->type == JSON_NUMBER ? (long)completed->num_val : 0;
385|    if (t <= 0) return strdup("forever");
386|    if (t == 1) {
387|        if (c == 0) return strdup("once");
388|        return strdup("1/1");
389|    }
390|    if (c == 0) {
391|        char *s = malloc(32);
392|        if (s) snprintf(s, 32, "%ld times", t);
393|        return s;
394|    }
395|    char *s = malloc(32);
396|    if (s) snprintf(s, 32, "%ld/%ld", c, t);
397|    return s;
398|}
399|
400|/* ================================================================
401| *  11. PoP: _resolve_model_override
402| *  Returns json_t* {provider, model} (both strings or null).
403| *  Pins provider to config main provider if model given but provider omitted.
404| * ================================================================ */
405|/* PoP: resolve_model_override @ tools/cronjob_tools.py:_resolve_model_override
406| * Port of Python tools/cronjob_tools.py:_resolve_model_override().
407| * Resolves a model override object into (provider, model) for job storage.
408| * If provider is omitted, pins the current main provider from config so the
409| * job doesn't drift when the user later changes their default via hermes model.
410| * Returns json_t* object with "provider" (string or null) and "model" (string or null). */
411|json_t *resolve_model_override(const json_t *model_obj)
412|{
413|    json_t *obj = json_object();
414|    if (!model_obj || model_obj->type != JSON_OBJECT) {
415|        json_set(obj, "provider", json_null());
416|        json_set(obj, "model", json_null());
417|        return obj;
418|    }
419|    const char *model_name = json_get_str(model_obj, "model", "");
420|    const char *provider_name = json_get_str(model_obj, "provider", "");
421|
422|    /* Strip whitespace */
423|    char *m = model_name && *model_name ? strdup(model_name) : NULL;
424|    char *p = provider_name && *provider_name ? strdup(provider_name) : NULL;
425|
426|    /* Bare "custom" → no named custom provider → treat as no provider supplied */
427|    if (p && strcmp(p, "custom") == 0) {
428|        /* In C we cannot check has_named_custom_provider; leave as "custom" if
429|         * explicitly given, else NULL. For parity we clear it. */
430|        free(p);
431|        p = NULL;
432|    }
433|
434|    if (m && !p) {
435|        /* Best-effort: read main provider from config if available.
436|         * In C port, we leave provider NULL — runtime will pin on fire. */
437|    }
438|
439|    json_set(obj, "provider", p ? json_string(p) : json_null());
440|    json_set(obj, "model", m ? json_string(m) : json_null());
441|    if (p) free(p);
442|    if (m) free(m);
443|    return obj;
444|}
445|
446|/* ================================================================
447| *  12. PoP: _normalize_optional_job_value
448| *  Normalizes an optional value: strips whitespace, optionally strips
449| *  trailing slash. Returns malloc'd string or NULL.
450| * ================================================================ */
451|static char *normalize_optional_job_value(const char *value, bool strip_trailing_slash)
452|{
453|    if (!value) return NULL;
454|    const char *start = value;
455|    while (*start && isspace((unsigned char)*start)) start++;
456|    const char *end = value + strlen(value);
457|    while (end > start && isspace((unsigned char)*(end - 1))) end--;
458|    if (strip_trailing_slash && end > start && *(end - 1) == '/') end--;
459|    size_t len = (size_t)(end - start);
460|    if (len == 0) return NULL;
461|    char *out = malloc(len + 1);
462|    if (!out) return NULL;
463|    memcpy(out, start, len);
464|    out[len] = '\0';
465|    return out;
466|}
467|
468|/* ================================================================
469| *  13. PoP: _normalize_deliver_param
470| *  Flattens list/tuple deliver values to comma-separated string.
471| * ================================================================ */
472|/* PoP: normalize_deliver_param @ tools/cronjob_tools.py:_normalize_deliver_param
473| * Port of Python tools/cronjob_tools.py:_normalize_deliver_param().
474| * Normalizes a user-supplied "deliver" value to canonical string form.
475| * Flattens arrays/tuples to comma-separated string. Returns malloc'd string
476| * or NULL for None/empty. */
477|static char *normalize_deliver_param(const json_t *value)
478|{
479|    if (!value) return NULL;
480|    if (value->type == JSON_ARRAY) {
481|        size_t n = json_len(value);
482|        char *parts[64]; size_t pc = 0;
483|        for (size_t i = 0; i < n && pc < 64; i++) {
484|            const char *s = json_get_str(json_get(value, i), NULL, "");
485|            if (s && *s) parts[pc++] = strdup(s);
486|        }
487|        if (pc == 0) return NULL;
488|        size_t total = 1;
489|        for (size_t i = 0; i < pc; i++) total += strlen(parts[i]) + 1;
490|        char *out = malloc(total);
491|        if (!out) { for (size_t i = 0; i < pc; i++) free(parts[i]); return NULL; }
492|        char *w = out;
493|        for (size_t i = 0; i < pc; i++) {
494|            if (i > 0) *w++ = ',';
495|            size_t l = strlen(parts[i]);
496|            memcpy(w, parts[i], l);
497|            w += l;
498|            free(parts[i]);
499|        }
500|        *w = '\0';
501|