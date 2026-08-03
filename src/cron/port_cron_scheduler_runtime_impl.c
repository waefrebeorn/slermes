/*
 * port_cron_scheduler_runtime_impl.c — faithful C11 port of the
 * cron/scheduler.py ORCHESTRATION + DELIVERY runtime surface. This is the
 * layer that ties the pure helpers (delivery routing, prompt assembly,
 * script runner, locks/pools) into a runnable job:
 *
 *   _run_job_no_agent-style no_agent path  -> scheduler_run_job_no_agent
 *   run_job                          (full orchestrator) -> scheduler_run_job
 *   _deliver_result                  (multi-target deliver) -> scheduler_deliver_result
 *   _send_media_via_adapter          (extension-routed send) -> scheduler_send_media_via_adapter
 *   _route_media                    (pure kind router)     -> scheduler_route_media
 *   _confirm_adapter_delivery        (SendResult guard)     -> scheduler_confirm_adapter_delivery
 *   _summarize_cron_failure_for_delivery -> scheduler_summarize_cron_failure_for_delivery
 *   _is_channel_dm_topic             (topic probe)          -> scheduler_is_channel_dm_topic
 *   _maybe_mirror_cron_delivery      (mirror to origin)     -> scheduler_maybe_mirror_cron_delivery
 *   _open_continuable_cron_thread    (thread open)           -> scheduler_open_continuable_cron_thread
 *   _seed_cron_thread_session        (thread session seed)  -> scheduler_seed_cron_thread_session
 *   _seed_cron_channel_session       (flat channel seed)    -> scheduler_seed_cron_channel_session
 *
 * The IO/process-coupled tail of run_job (provider resolution, AIAgent
 * construction, SessionDB, fallback chain, reasoning config) is delegated to
 * the caller via the scheduler_agent_run_fn callback — exactly the seam
 * Python uses (it imports/constructs AIAgent and calls run_conversation).
 * The C orchestrator faithfully reproduces the ORDER and GUARDS: no_agent
 * short-circuit, script-run + wake-gate, credential-exfil guard,
 * injection-scanned prompt assembly, interruption-flag honor, then delivery.
 *
 * Live-adapter branch: when a gateway runtime is live (cron fired from inside
 * the running gateway), delivery routes through the running adapter exactly
 * like Python's `adapters`/`loop` path. Standalone (scheduler tick / `hermes
 * cron run`) falls through to send_message_send_to_platform, the C equivalent
 * of Python's _send_to_platform fallback.
 *
 * Opaque structs + minimal includes; no god headers.
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "cron_jobs.h"                 /* now_iso, SCHEDULER_SILENT_MARKER */
#include "cron_scheduler_delivery.h"  /* routing/target/resolve helpers */
#include "cron_scheduler_helpers.h"   /* scheduler_normalize_deliver_value */
#include "gateway/platforms/base.h"  /* gw_extract_media / validate_media_delivery_path */
#include "hermes_gateway_mirror.h"    /* mirror_to_session */
#include "gw_server_internals.h"      /* session_get_or_create */
#include "hermes_redact.h"             /* hermes_redact */
#include "hermes_logger.h"             /* hermes_log */
#include "hermes_tool_config.h"        /* tool_config_get_bool */
#include "port_config_py_helpers.h"    /* config_py_load_config_impl */
#include "port_send_message_tool.h"      /* send_message_send_to_platform */
#include <stdbool.h>

/* send_message_send_to_platform is defined in port_send_message_tool.c and
 * not exported via a header; declare it here (faithful router into the
 * platform send surface). Signature mirrors tools/send_message_tool.py. */
extern json_t *send_message_send_to_platform(const char *platform, json_t *pconfig,
                                              const char *chat_id, const char *message,
                                              const char *thread_id, const char *const *media_files);

/* ── static config plumbing ─────────────────────────────────────────── */

/* printf-style strdup (avoids a non-portable strdupf dependency). */
static char *xstrdupf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* Read cron.wrap_response from config.yaml (default true). */
static bool cron_wrap_response_enabled(void)
{
    bool wrap = true;
    json_t *cfg = config_py_load_config_impl(0);
    if (cfg) {
        const json_t *cron = json_obj_get(cfg, "cron");
        if (cron && json_node_is_object(cron)) {
            const json_t *w = json_obj_get(cron, "wrap_response");
            if (w && json_node_is_bool(w))
                wrap = json_bool_value(w);
        }
        json_free(cfg);
    }
    return wrap;
}

/* Build the cron-mirror prefixed text: "[Cron delivery: <name>]\n<body>". */
static char *cron_mirror_prefix(const json_t *job, const char *body)
{
    const char *name = json_get_str((json_t *)job, "name", NULL);
    const char *id   = json_get_str((json_t *)job, "id", "cron");
    size_t n = (name ? strlen(name) : 0) + (id ? strlen(id) : 5) +
               (body ? strlen(body) : 0) + 48;
    char *msg = malloc(n);
    if (msg)
        snprintf(msg, n, "[Cron delivery: %s]\n%s",
                 name ? name : (id ? id : "cron"), body ? body : "");
    return msg;
}

/* ── failure text builder ──────────────────────────────────────────── */

/* PoP: scheduler_summarize_cron_failure_for_delivery
 *      @ cron/scheduler.py:_summarize_cron_failure_for_delivery */
char *scheduler_summarize_cron_failure_for_delivery(const char *job_id,
                                                    const char *error)
{
    if (!job_id) job_id = "<unknown>";
    if (!error) error = "unknown error";

    char *safe = hermes_redact(error);

    size_t n = strlen(job_id) + strlen(safe) + 160;
    char *out = malloc(n);
    if (out)
        snprintf(out, n,
                 "Cron job '%s' failed: %s\n\n"
                 "To stop or manage this job, send me a new message "
                 "(e.g. \"stop reminder %s\").",
                 job_id, safe, job_id);
    free(safe);
    return out;
}

/* ── SendResult contract guard ─────────────────────────────────────── */

/* PoP: scheduler_confirm_adapter_delivery
 *      @ cron/scheduler.py:_confirm_adapter_delivery */
int scheduler_confirm_adapter_delivery(const json_t *send_result)
{
    if (!send_result || !json_node_is_object(send_result)) return 0;
    const json_t *status = json_obj_get(send_result, "status");
    if (status && json_node_is_string(status) &&
        strcmp(json_string_value(status), "delivered") == 0)
        return 1;
    const json_t *success = json_obj_get(send_result, "success");
    if (success && json_node_is_bool(success) && json_bool_value(success))
        return 1;
    return 0;
}

/* ── pure media kind router ────────────────────────────────────────── */

/* PoP: scheduler_route_media @ cron/scheduler.py:_send_media_via_adapter */
/* Pure routing: extension + platform + voice flag -> adapter method kind.
 * Faithful to base.py:should_send_media_as_audio:
 *   - if should_send_media_as_audio(platform, ext, is_voice) -> VOICE (audio)
 *   - elif video extension -> VIDEO
 *   - elif image extension -> IMAGE
 *   - else -> DOCUMENT
 * Telegram: .ogg/.opus are audio (voice sender) ONLY when is_voice is set;
 * .mp3/.m4a are audio attachments; other (.jpg/.png/.mp4...) fall through.
 * Every other platform: any recognized audio extension -> audio. */
scheduler_media_kind_t scheduler_route_media(const char *platform,
                                             const char *media_path,
                                             bool is_voice)
{
    const char *dot = media_path ? strrchr(media_path, '.') : NULL;
    const char *ext = dot ? dot : "";

    static const char *video_exts[] = {".mp4",".mov",".avi",".mkv",".webm",".3gp", NULL};
    static const char *image_exts[] = {".jpg",".jpeg",".png",".webp",".gif", NULL};
    static const char *audio_exts[] = {".ogg",".opus",".mp3",".wav",".m4a",".flac", NULL};
    static const char *telegram_voice_exts[] = {".ogg",".opus", NULL};
    static const char *telegram_audio_exts[] = {".mp3",".m4a", NULL};

    int is_video = 0, is_image = 0, is_audio = 0;
    for (int i = 0; video_exts[i]; i++) if (!strcmp(ext, video_exts[i])) is_video = 1;
    for (int i = 0; image_exts[i]; i++) if (!strcmp(ext, image_exts[i])) is_image = 1;
    for (int i = 0; audio_exts[i]; i++) if (!strcmp(ext, audio_exts[i])) is_audio = 1;

    bool as_audio = false;
    if (is_audio) {
        if (platform && strcmp(platform, "telegram") == 0) {
            for (int i = 0; telegram_voice_exts[i]; i++)
                if (!strcmp(ext, telegram_voice_exts[i])) as_audio = is_voice;
            for (int i = 0; telegram_audio_exts[i]; i++)
                if (!strcmp(ext, telegram_audio_exts[i])) as_audio = true;
        } else {
            as_audio = true;
        }
    }

    if (as_audio) return SCHEDULER_MEDIA_VOICE;
    if (is_video) return SCHEDULER_MEDIA_VIDEO;
    if (is_image) return SCHEDULER_MEDIA_IMAGE;
    return SCHEDULER_MEDIA_DOCUMENT;
}

/* ── media send via adapter (live path) ────────────────────────────── */

/* PoP: scheduler_send_media_via_adapter @ cron/scheduler.py:_send_media_via_adapter */
/* Routes each extracted MEDIA file to the appropriate adapter method by kind.
 * Returns the number of successful sends. The adapter may be NULL (no live
 * gateway) in which case 0 is returned (the standalone fallback handles
 * media via send_message_send_to_platform). */
int scheduler_send_media_via_adapter(const scheduler_adapter_t *adapter,
                                     const char *platform,
                                     const char *chat_id,
                                     const char *const *media_paths,
                                     const bool *voice_flags,
                                     const json_t *job)
{
    (void)job;
    if (!adapter || !platform || !chat_id || !media_paths) return 0;
    int sent = 0;
    for (int i = 0; media_paths[i]; i++) {
        bool is_voice = voice_flags ? voice_flags[i] : false;
        scheduler_media_kind_t kind = scheduler_route_media(platform,
                                                            media_paths[i],
                                                            is_voice);
        bool ok = false;
        switch (kind) {
        case SCHEDULER_MEDIA_VOICE:
            if (adapter->send_voice)
                ok = adapter->send_voice(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_VIDEO:
            if (adapter->send_video)
                ok = adapter->send_video(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_IMAGE:
            if (adapter->send_image)
                ok = adapter->send_image(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_DOCUMENT:
        default:
            if (adapter->send_document)
                ok = adapter->send_document(chat_id, media_paths[i], adapter->ctx);
            break;
        }
        if (ok) sent++;
    }
    return sent;
}

/* ── ambiguous Telegram topic probe ────────────────────────────────── */

/* PoP: scheduler_is_channel_dm_topic @ cron/scheduler.py:_is_channel_dm_topic */
/* The running gateway adapter exposes a get_chat_info probe. When no live
 * adapter is available (standalone cron), we fail SAFE to forum-topic routing
 * (return 0), exactly like the Python default. */
bool scheduler_is_channel_dm_topic(const scheduler_adapter_t *adapter,
                                   const char *chat_id, const char *job_id)
{
    (void)job_id;
    if (!adapter || !adapter->get_chat_info || !chat_id) return false;
    char type[32];
    if (adapter->get_chat_info(chat_id, type, sizeof(type), adapter->ctx) &&
        strcmp(type, "channel") == 0)
        return true;
    return false;
}

/* ── mirror / seed helpers ─────────────────────────────────────────── */

/* PoP: scheduler_maybe_mirror_cron_delivery @ cron/scheduler.py:_maybe_mirror_cron_delivery */
/* Best-effort mirror of a cron delivery into the origin chat's session.
 * No-op unless enabled. Prefixes "[Cron delivery: <name>]". */
void scheduler_maybe_mirror_cron_delivery(const json_t *job,
                                          const char *platform,
                                          const char *chat_id,
                                          const char *mirror_text,
                                          const char *thread_id,
                                          const char *user_id,
                                          bool enabled)
{
    if (!enabled) return;
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return; }
    mirror_to_session(platform, chat_id, text, "cron", thread_id, user_id);
    free(text);
}

/* PoP: scheduler_open_continuable_cron_thread @ cron/scheduler.py:_open_continuable_cron_thread */
/* Open a dedicated thread for a continuable cron job. Returns malloc'd thread
 * id or NULL (fall back to DM mirror). In standalone cron there is no live
 * adapter with a create_thread capability, so we return NULL — the
 * mirror-to-session fallback (seed_cron_thread_session) handles the surface. */
char *scheduler_open_continuable_cron_thread(const json_t *job,
                                             const scheduler_adapter_t *adapter,
                                             const char *chat_id)
{
    (void)job;
    if (!adapter || !adapter->create_thread || !chat_id) return NULL;
    const char *name = json_get_str((json_t *)job, "name", "cron");
    char *tid = adapter->create_thread(chat_id, name, adapter->ctx);
    return tid; /* may be NULL */
}

/* PoP: scheduler_seed_cron_thread_session @ cron/scheduler.py:_seed_cron_thread_session */
/* Seed the freshly-opened cron thread's session with the brief. Ensures the
 * thread-keyed session row exists, then mirrors the brief. */
void scheduler_seed_cron_thread_session(const json_t *job,
                                        const char *platform,
                                        const char *chat_id,
                                        const char *thread_id,
                                        const char *mirror_text)
{
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return; }
    (void)session_get_or_create(platform, chat_id);
    mirror_to_session(platform, chat_id, text, "cron", thread_id,
                      "system:cron");
    free(text);
}

/* PoP: scheduler_seed_cron_channel_session @ cron/scheduler.py:_seed_cron_channel_session */
/* Seed the FLAT (thread-less) session for an in_channel delivery. Returns true
 * when a seed row was created and the brief mirrored. */
bool scheduler_seed_cron_channel_session(const json_t *job,
                                         const char *platform,
                                         const char *chat_id,
                                         const char *mirror_text,
                                         bool is_dm, const char *user_id)
{
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return false; }
    (void)session_get_or_create(platform, chat_id);
    mirror_to_session(platform, chat_id, text, "cron", NULL,
                      user_id ? user_id : NULL);
    free(text);
    return true;
}

/* ── deliver_result ────────────────────────────────────────────────── */

/* PoP: scheduler_deliver_result @ cron/scheduler.py:_deliver_result */
/* Deliver job output to the configured target(s). Uses the adapter when given,
 * else the registered platform send path. Returns NULL on success (or a
 * legitimately silent run) or a malloc'd error string. */
char *scheduler_deliver_result(const json_t *job, const char *content,
                               const scheduler_adapter_t *adapter)
{
    if (!job) return strdup("no job to deliver");
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");

    /* 1. resolve targets */
    scheduler_job_t sjob;
    memset(&sjob, 0, sizeof(sjob));
    const json_t *origin = json_obj_get((json_t *)job, "origin");
    if (origin && json_node_is_object(origin)) {
        const char *plat = json_get_str(origin, "platform", NULL);
        const char *cid  = json_get_str(origin, "chat_id", NULL);
        if (plat && cid)
            sjob.origin = (scheduler_origin_t){ plat, cid,
                json_get_str(origin, "thread_id", NULL), 1 };
    }
    sjob.deliver = json_get_str((json_t *)job, "deliver", "local");

    scheduler_target_t targets[16];
    int n = scheduler_resolve_delivery_targets(&sjob, targets, 16);
    if (n == 0) {
        const char *deliver_value = sjob.deliver ? sjob.deliver : "local";
        if (strcmp(deliver_value, "local") == 0)
            return NULL;                 /* local-only: not a failure */
        if (strcmp(deliver_value, "origin") == 0)
            return NULL;                 /* origin unresolvable: treat local */
        char *msg = malloc(128);
        snprintf(msg, 128, "no delivery target resolved for deliver=%s",
                 deliver_value);
        return msg;
    }

    /* 2. build delivery content */
    bool wrap = cron_wrap_response_enabled();
    char *delivery_content;
    if (wrap) {
        const char *task_name = json_get_str((json_t *)job, "name", job_id);
        size_t nlen = strlen(task_name) + strlen(job_id) +
                   strlen(content ? content : "") + 200;
        delivery_content = malloc(nlen);
        snprintf(delivery_content, nlen,
                 "Cronjob Response: %s\n(job_id: %s)\n-------------\n\n%s\n\n"
                 "To stop or manage this job, send me a new message "
                 "(e.g. \"stop reminder %s\").",
                 task_name, job_id, content ? content : "", task_name);
    } else {
        delivery_content = strdup(content ? content : "");
    }

    /* 3. extract MEDIA: tags */
    gw_media_list_t media = gw_extract_media(delivery_content);
    gw_media_list_t safe_media;
    memset(&safe_media, 0, sizeof(safe_media));
    for (int i = 0; i < media.count && safe_media.count < 64; i++) {
        char *p = validate_media_delivery_path(media.paths[i]);
        if (p) {
            safe_media.paths[safe_media.count] = p;
            safe_media.is_voice[safe_media.count] =
                media.is_voice ? media.is_voice[i] : 0;
            safe_media.count++;
        }
    }
    gw_media_list_free(&media);

    /* mirror gate (computed once) */
    json_t *user_cfg = config_py_load_config_impl(0);
    int mirror_enabled = scheduler_cron_mirror_delivery_enabled(&sjob,
        user_cfg ? tool_config_get_bool("cron", "mirror_delivery", false) : 0);
    json_free(user_cfg);

    char *mirror_text = NULL;
    if (mirror_enabled) {
        gw_media_list_t m2 = gw_extract_media(content ? content : "");
        gw_media_list_free(&m2);
        mirror_text = strdup(content ? content : "");
        if (mirror_text) {             /* strip trailing whitespace */
            size_t l = strlen(mirror_text);
            while (l && (mirror_text[l-1]==' '||mirror_text[l-1]=='\n'||
                         mirror_text[l-1]=='\r'||mirror_text[l-1]=='\t'))
                mirror_text[--l] = '\0';
        }
    }

    /* 4. per-target delivery */
    char *errors[16]; int ne = 0;
    int live_adapter_ready = (adapter != NULL);

    for (int t = 0; t < n; t++) {
        const char *platform_name = targets[t].platform;
        const char *chat_id = targets[t].chat_id;
        char *thread_id = strdup(targets[t].thread_id);
        if (!thread_id) thread_id = strdup("");

        int mirror_this =
            mirror_enabled &&
            scheduler_target_matches_origin(&sjob.origin, platform_name,
                                            chat_id, thread_id);
        const char *origin_user_id = NULL;
        if (mirror_this && origin && json_node_is_object(origin))
            origin_user_id = json_get_str(origin, "user_id", NULL);

        if (!scheduler_is_known_delivery_platform(platform_name)) {
            if (ne < 16) errors[ne++] =
                xstrdupf("unknown platform '%s'", platform_name);
            free(thread_id);
            continue;
        }

        /* continuable surface: thread (default) vs in_channel */
        int in_channel = 0;
        if (gateway_config_platform_extra_bool(platform_name,
                                               "cron_continuable_surface")) {
            json_t *cfg = config_py_load_config_impl(0);
            if (cfg) {
                const json_t *gwc = json_obj_get(cfg, "gateway");
                const json_t *plats = gwc ? json_obj_get(gwc, "platforms") : NULL;
                const json_t *pc = plats ? json_obj_get(plats, platform_name) : NULL;
                const json_t *extra = pc ? json_obj_get(pc, "extra") : NULL;
                const json_t *surf = extra ? json_obj_get(extra,
                                                 "cron_continuable_surface") : NULL;
                if (surf && json_node_is_string(surf) &&
                    strcmp(json_string_value(surf), "in_channel") == 0)
                    in_channel = 1;
                json_free(cfg);
            }
        }

        if (in_channel && mirror_this && live_adapter_ready) {
            free(thread_id);
            thread_id = strdup("");
        }

        int is_dm_target = 0;
        const json_t *och = origin ? json_obj_get(origin, "chat_type") : NULL;
        const char *och_s = och && json_node_is_string(och)
            ? json_string_value(och) : "";
        if (!strcmp(och_s, "dm") ||
            (!och_s[0] && strncmp(chat_id, "D", 1) == 0))
            is_dm_target = 1;

        int thread_seeded = 0;
        int inchannel_seeded = 0;

        char *opened_thread_id = NULL;
        if (mirror_this && !in_channel && live_adapter_ready &&
            !thread_id[0]) {
            opened_thread_id =
                scheduler_open_continuable_cron_thread(job, adapter, chat_id);
            if (opened_thread_id) {
                free(thread_id);
                thread_id = opened_thread_id;
            }
        }

        int delivered = 0;
        if (live_adapter_ready) {
            /* Live path: route through the running gateway adapter. */
            if (opened_thread_id && !thread_seeded) {
                scheduler_seed_cron_thread_session(job, platform_name,
                                                    chat_id, thread_id,
                                                    mirror_text);
                thread_seeded = 1;
            }
            int ok = 0;
            if (adapter->send_text)
                ok = adapter->send_text(chat_id, delivery_content,
                                        thread_id[0] ? thread_id : NULL,
                                        adapter->ctx);
            if (ok) delivered = 1;
        }

        if (!delivered) {
            /* Standalone fallback: send via the platform send surface. */
            const char *files[65];
            int fc = 0;
            for (int i = 0; i < safe_media.count && fc < 64; i++)
                files[fc++] = safe_media.paths[i];
            files[fc] = NULL;

            json_t *pcfg = json_object();
            json_t *res = send_message_send_to_platform(
                platform_name, pcfg, chat_id,
                delivery_content, thread_id[0] ? thread_id : NULL, files);
            json_free(pcfg);

            int ok = 0;
            if (res) {
                const json_t *err = json_obj_get(res, "error");
                ok = !err;
                json_free(res);
            }
            if (ok) {
                hermes_log(LOG_INFO, "cron", "delivered to %s:%s",
                           platform_name, chat_id);
                delivered = 1;
                if (in_channel && mirror_this) {
                    inchannel_seeded = scheduler_seed_cron_channel_session(
                        job, platform_name, chat_id, mirror_text,
                        is_dm_target, origin_user_id);
                }
                scheduler_maybe_mirror_cron_delivery(
                    job, platform_name, chat_id, mirror_text,
                    thread_id[0] ? thread_id : NULL, origin_user_id,
                    mirror_this && !thread_seeded && !inchannel_seeded);
            } else {
                if (ne < 16) errors[ne++] = xstrdupf(
                    "delivery to %s:%s failed", platform_name, chat_id);
            }
        }

        free(thread_id);
    }

    for (int i = 0; i < safe_media.count; i++) free(safe_media.paths[i]);
    free(mirror_text);
    free(delivery_content);

    if (ne > 0) {
        size_t total = 0;
        for (int i = 0; i < ne; i++) total += strlen(errors[i]) + 3;
        char *out = malloc(total + 1);
        out[0] = '\0';
        for (int i = 0; i < ne; i++) {
            strcat(out, errors[i]);
            if (i + 1 < ne) strcat(out, "; ");
            free(errors[i]);
        }
        return out;
    }
    return NULL;
}

/* ── run_job_no_agent ─────────────────────────────────────────────── */

/* PoP: scheduler_run_job_no_agent @ cron/scheduler.py:run_job */
/* Full no_agent run path of run_job: the script IS the job. Returns success;
 * fills malloc'd *doc_out, *final_out, *error_out (any may be NULL).
 * final_out == SCHEDULER_SILENT_MARKER marks a silent run. */
bool scheduler_run_job_no_agent(const json_t *job, char **doc_out,
                                char **final_out, char **error_out)
{
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    const char *job_name = json_get_str((json_t *)job, "name",
                            json_get_str((json_t *)job, "prompt", job_id));

    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    if (!script_path) {
        if (error_out) *error_out = strdup(
            "no_agent=True but no script is set for this job");
        return false;
    }

    const char *wd = json_get_str((json_t *)job, "workdir", NULL);
    char *_job_workdir = NULL;
    if (wd && wd[0]) {
        /* Validate the workdir exists; else run without it. */
        struct stat st;
        if (stat(wd, &st) == 0 && S_ISDIR(st.st_mode)) {
            _job_workdir = strdup(wd);
        } else {
            hermes_log(LOG_WARNING, "cron",
                "Job '%s': configured workdir '%s' no longer exists",
                job_id, wd);
        }
    }

    bool ok = false; char *output = NULL;
    ok = scheduler_run_job_script_with_claim_heartbeat(job, script_path,
                                                       _job_workdir, &output);
    free(_job_workdir);

    char *now_iso_str = now_iso();

    if (!ok) {
        char *alert = xstrdupf(
            "\xE2\x9A\xA0 Cron watchdog '%s' script failed\n\n%s\n\nTime: %s",
            job_name, output ? output : "", now_iso_str ? now_iso_str : "");
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** script failed\n\n%s\n",
            job_name, job_id, now_iso_str ? now_iso_str : "",
            output ? output : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = alert; else free(alert);
        if (error_out) *error_out = strdup(output ? output : "");
        free(output); free(now_iso_str);
        return false;
    }

    /* wakeAgent=false gate -> silent */
    if (!scheduler_parse_wake_gate(output)) {
        hermes_log(LOG_INFO, "cron",
            "Job '%s' (no_agent): wakeAgent=false gate — silent run", job_id);
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** silent (wakeAgent=false)\n",
            job_name, job_id, now_iso_str ? now_iso_str : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        free(output); free(now_iso_str);
        return true;
    }

    if (!output || !output[0] || !output[strspn(output, " \t\r\n")]) {
        hermes_log(LOG_INFO, "cron",
            "Job '%s' (no_agent): empty stdout — silent run", job_id);
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** silent (empty output)\n",
            job_name, job_id, now_iso_str ? now_iso_str : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        free(output); free(now_iso_str);
        return true;
    }

    char *doc = xstrdupf(
        "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
        "**Mode:** no_agent (script)\n\n---\n\n%s\n",
        job_name, job_id, now_iso_str ? now_iso_str : "", output);
    if (doc_out) *doc_out = doc; else free(doc);
    if (final_out) *final_out = strdup(output); else { /* output owned below */ }
    if (error_out) *error_out = NULL;
    free(output); free(now_iso_str);
    return true;
}

/* ── run_job (full orchestrator) ──────────────────────────────────── */

/* PoP: scheduler_run_job @ cron/scheduler.py:run_job */
/* Execute a single cron job. no_agent short-circuit, wake gate, prompt
 * assembly (injection-scanned), credential-exfil guard, then the agent
 * callback; interruption flags honored before status is reported. Returns
 * success; fills the 4-tuple (doc_out, final_out, error_out, + needs_deliver
 * via return of final_out). */
bool scheduler_run_job(const json_t *job,
                       scheduler_agent_run_fn agent_run, void *agent_ctx,
                       char **doc_out, char **final_out, char **error_out)
{
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    const char *job_name = json_get_str((json_t *)job, "name",
                            json_get_str((json_t *)job, "prompt", job_id));

    /* ----------------------------------------------------------------
     * no_agent short-circuit — the script IS the job, no LLM involvement.
     * ---------------------------------------------------------------- */
    if (json_get_bool((json_t *)job, "no_agent", false)) {
        char *d = NULL, *f = NULL, *e = NULL;
        bool ok = scheduler_run_job_no_agent(job, &d, &f, &e);
        if (doc_out) *doc_out = d; else free(d);
        if (final_out) *final_out = f; else free(f);
        if (error_out) *error_out = e; else free(e);
        return ok;
    }

    /* ----------------------------------------------------------------
     * Default (LLM) path.
     * ---------------------------------------------------------------- */
    /* Credential-exfil guard (fail closed before any provider call). */
    char *guard_err = scheduler_guard_job_credential_exfil(job);
    if (guard_err) {
        char *blocked_doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Status:** BLOCKED\n\n"
            "The job's stored provider/base_url pair would ship a credential "
            "off-host. Refusing to run.\n\n**Guard result:** %s\n",
            job_name, job_id, guard_err);
        if (doc_out) *doc_out = blocked_doc; else free(blocked_doc);
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = guard_err; /* transfers ownership */
        return false;
    }

    /* Wake-gate: run the pre-check script before building the prompt so a
     * wakeAgent=false short-circuits the agent run. */
    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    bool prerun_ok = false; char *prerun_output = NULL;
    if (script_path) {
        prerun_ok = scheduler_run_job_script_with_claim_heartbeat(
            job, script_path, NULL, &prerun_output);
        if (prerun_ok && !scheduler_parse_wake_gate(prerun_output)) {
            hermes_log(LOG_INFO, "cron",
                "Job '%s': wakeAgent=false, skipping agent run", job_id);
            char *now = now_iso();
            char *silent_doc = xstrdupf(
                "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n\n"
                "Script gate returned `wakeAgent=false` — agent skipped.\n",
                job_name, job_id, now ? now : "");
            free(now); free(prerun_output);
            if (doc_out) *doc_out = silent_doc; else free(silent_doc);
            if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
            if (error_out) *error_out = NULL;
            return true;
        }
    }

    bool silent = false;
    char *agent_err = NULL;
    char *prompt = scheduler_build_job_prompt(job, prerun_ok, prerun_output,
                                              &silent, &agent_err);
    free(prerun_output);
    if (agent_err) {
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = agent_err;
        return false;
    }
    if (prompt == NULL) {
        /* Silent (empty script output) — skip AI call. */
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        return true;
    }

    /* Honor interruption before spending the agent run. */
    if (scheduler_is_interrupted(job_id)) {
        char *blocked_doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Status:** INTERRUPTED\n\n"
            "Job was interrupted before the agent run.\n", job_name, job_id);
        free(prompt);
        if (doc_out) *doc_out = blocked_doc; else free(blocked_doc);
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = strdup("interrupted");
        return false;
    }

    /* Run the agent via the injected callback. */
    char *agent_final = agent_run ? agent_run(prompt, job, &agent_err, agent_ctx)
                                   : NULL;
    free(prompt);
    if (!agent_final) {
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = agent_err ? agent_err : strdup("agent failed");
        return false;
    }

    char *now = now_iso();
    char *doc = xstrdupf(
        "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
        "**Mode:** agent\n\n---\n\n%s\n",
        job_name, job_id, now ? now : "", agent_final);
    free(now);
    if (doc_out) *doc_out = doc; else free(doc);
    if (final_out) *final_out = agent_final; /* transfers ownership */
    if (error_out) *error_out = NULL;
    return true;
}
