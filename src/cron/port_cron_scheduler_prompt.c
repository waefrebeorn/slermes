/*
 * port_cron_scheduler_prompt.c — faithful C11 port of cron/scheduler.py's
 * prompt-assembly surface:
 *   _cron_job_origin_log_suffix   (safe provenance for security logs)
 *   _scan_assembled_cron_prompt   (two-tier injection scanner dispatch)
 *   _build_job_prompt             (script/context/skill/bundle assembly)
 *   _guard_job_credential_exfil   (F8 runtime backstop)
 *
 * Reuses: cron_prompt_sanitize.{scan_prompt,scan_skill_assembled},
 * cronjob_validate_cron_base_url, cronjobs_output_dir, skill loading via
 * skills_view_handler, bundles via build_bundle_invocation_message /
 * resolve_bundle_command_key, usage via libskillusage.
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "cron_jobs.h"                     /* cronjobs_output_dir */
#include "port_cronjob_tools.h"            /* cronjob_validate_cron_base_url */
#include "slermes_home.h"
#include "skill_usage.h"                   /* skill_usage_bump_use */
#include "../tools/cron_prompt_sanitize.h"

/* skills / bundles surfaces (subsystem-level externs, opaque payloads) */
extern char *skills_view_handler(const char *args_json, const char *task_id);
extern json_t *build_bundle_invocation_message(const char *cmd_key,
                                               const char *user_instruction);
extern char *resolve_bundle_command_key(const char *command);

/* ── small str builder ──────────────────────────────────────────────── */

typedef struct { char *buf; size_t len, cap; } sb_t;

static bool sb_init(sb_t *sb)
{
    sb->cap = 4096; sb->len = 0;
    sb->buf = malloc(sb->cap);
    if (!sb->buf) return false;
    sb->buf[0] = '\0';
    return true;
}

static void sb_put(sb_t *sb, const char *s)
{
    if (!s || !sb->buf) return;
    size_t sl = strlen(s);
    if (sb->len + sl + 1 > sb->cap) {
        while (sb->len + sl + 1 > sb->cap) sb->cap *= 2;
        char *nb = realloc(sb->buf, sb->cap);
        if (!nb) return;
        sb->buf = nb;
    }
    memcpy(sb->buf + sb->len, s, sl + 1);
    sb->len += sl;
}

/* ================================================================
 * _cron_job_origin_log_suffix
 * ================================================================ */

/* PoP: scheduler_cron_job_origin_log_suffix @ cron/scheduler.py:_cron_job_origin_log_suffix */
char *scheduler_cron_job_origin_log_suffix(const json_t *job)
{
    const json_t *origin =
        job ? json_object_get((json_t *)job, "origin") : NULL;
    if (!json_node_is_object(origin)) return strdup("");

    static const char *keys[] = {
        "platform", "chat_id", "thread_id", "source_ip", "remote",
        "forwarded_for", NULL
    };

    sb_t sb;
    if (!sb_init(&sb)) return strdup("");
    bool any = false;

    for (const char **k = keys; *k; k++) {
        const json_t *v = json_object_get((json_t *)origin, *k);
        if (!v || v->type == JSON_NULL) continue;

        char raw[256];
        if (v->type == JSON_STRING) {
            snprintf(raw, sizeof(raw), "%.200s", json_string_value(v));
        } else if (v->type == JSON_NUMBER) {
            double d = json_number_value(v);
            if (d == (double)(long long)d)
                snprintf(raw, sizeof(raw), "%lld", (long long)d);
            else
                snprintf(raw, sizeof(raw), "%g", d);
        } else if (v->type == JSON_BOOL) {
            snprintf(raw, sizeof(raw), "%s",
                     json_bool_value(v) ? "True" : "False");
        } else {
            continue;
        }

        /* replace \r,\n with space; strip */
        for (char *c = raw; *c; c++)
            if (*c == '\r' || *c == '\n') *c = ' ';
        char *s = raw;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        if (!l) continue;

        char field[320];
        snprintf(field, sizeof(field), "%sorigin_%s='%s'",
                 any ? " " : " ", *k, s);
        sb_put(&sb, field);
        any = true;
    }

    if (!any) { free(sb.buf); return strdup(""); }
    return sb.buf;
}

/* ================================================================
 * _scan_assembled_cron_prompt
 * ================================================================ */

/* PoP: scheduler_scan_assembled_cron_prompt @ cron/scheduler.py:_scan_assembled_cron_prompt */
char *scheduler_scan_assembled_cron_prompt(const char *assembled,
                                           const json_t *job,
                                           bool has_skills,
                                           bool has_injected_data,
                                           const char *user_prompt,
                                           char **error_out)
{
    (void)job;   /* job label only feeds Python's log line */
    if (error_out) *error_out = NULL;
    if (!assembled) assembled = "";

    char *result = NULL;
    char *scan_error = NULL;

    if (has_skills || has_injected_data) {
        /* Looser tier: sanitize invisible unicode, block only unambiguous
         * injection directives. */
        json_t *scan = cron_prompt_sanitize_scan_skill_assembled(assembled);
        if (scan) {
            const char *cleaned = json_get_str(scan, "cleaned", assembled);
            const char *err = json_get_str(scan, "error", "");
            result = strdup(cleaned ? cleaned : assembled);
            if (err && err[0]) scan_error = strdup(err);
            json_free(scan);
        } else {
            result = strdup(assembled);
        }
        if (!scan_error && !has_skills && user_prompt && user_prompt[0]) {
            /* Data-injection path: strict guarantee on the user prompt. */
            char *strict = cron_prompt_sanitize_scan_prompt(user_prompt);
            if (strict && strict[0]) scan_error = strict;
            else free(strict);
        }
    } else {
        char *strict = cron_prompt_sanitize_scan_prompt(assembled);
        if (strict && strict[0]) scan_error = strict;
        else free(strict);
        if (!scan_error) result = strdup(assembled);
    }

    if (scan_error) {
        free(result);
        if (error_out) *error_out = scan_error;
        else free(scan_error);
        return NULL;   /* CronPromptInjectionBlocked */
    }
    return result;
}

/* ================================================================
 * _guard_job_credential_exfil
 * ================================================================ */

/* PoP: scheduler_guard_job_credential_exfil @ cron/scheduler.py:_guard_job_credential_exfil */
char *scheduler_guard_job_credential_exfil(const json_t *job)
{
    if (!job) return NULL;
    const char *provider = json_get_str((json_t *)job, "provider", NULL);
    const char *base_url = json_get_str((json_t *)job, "base_url", NULL);

    char *err = cronjob_validate_cron_base_url(provider, base_url);
    if (!err) return NULL;

    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    size_t n = strlen(job_id) + strlen(err) + 64;
    char *msg = malloc(n);
    if (msg)
        snprintf(msg, n, "Cron job '%s' blocked for safety: %s", job_id, err);
    free(err);
    return msg;
}

/* ================================================================
 * _build_job_prompt
 * ================================================================ */

/* Valid context_from job ids are lowercase-hex strings. */
static bool is_hex_job_id(const char *s)
{
    if (!s || !s[0]) return false;
    for (const char *c = s; *c; c++) {
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f')))
            return false;
    }
    return true;
}

/* Latest *.md file in dir by mtime. Returns malloc'd path or NULL. */
static char *latest_md_in_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char *best = NULL;
    time_t best_mtime = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 4 || strcmp(ent->d_name + nl - 3, ".md") != 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!best || st.st_mtime > best_mtime) {
            free(best);
            best = strdup(full);
            best_mtime = st.st_mtime;
        }
    }
    closedir(d);
    return best;
}

static char *read_file_all(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

static void strip_inplace2(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t' ||
                   s[len-1] == '\n' || s[len-1] == '\r')) s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' ||
           *start == '\r') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static const char *CRON_HINT =
    "[IMPORTANT: You are running as a scheduled cron job. "
    "DELIVERY: Your final response will be automatically delivered "
    "to the user — do NOT use send_message or try to deliver "
    "the output yourself. Just produce your report/output as your "
    "final response and the system handles the rest. "
    "SILENT: If there is genuinely nothing new to report, respond "
    "with exactly \"[SILENT]\" (nothing else) to suppress delivery. "
    "Never combine [SILENT] with content — either report your "
    "findings normally, or say [SILENT] and nothing more.]\n\n";

/* PoP: scheduler_build_job_prompt @ cron/scheduler.py:_build_job_prompt */
char *scheduler_build_job_prompt(const json_t *job,
                                 bool prerun_ok, const char *prerun_output,
                                 bool *silent_out, char **error_out)
{
    if (silent_out) *silent_out = false;
    if (error_out) *error_out = NULL;
    if (!job) return NULL;

    const char *user_prompt = json_get_str((json_t *)job, "prompt", "");
    bool has_injected_data = false;

    sb_t prompt;
    if (!sb_init(&prompt)) return NULL;

    /* ── 1. data-collection script ─────────────────────────────── */
    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    char *inline_output = NULL;
    if (script_path && script_path[0]) {
        bool success;
        const char *script_output;
        if (prerun_output != NULL) {
            success = prerun_ok;
            script_output = prerun_output;
        } else {
            const char *workdir = json_get_str((json_t *)job, "workdir", NULL);
            success = scheduler_run_job_script(script_path, workdir,
                                               &inline_output);
            script_output = inline_output ? inline_output : "";
        }
        if (success) {
            if (script_output && script_output[0]) {
                sb_put(&prompt,
                       "## Script Output\n"
                       "The following data was collected by a pre-run "
                       "script. Use it as context for your analysis.\n\n"
                       "```\n");
                sb_put(&prompt, script_output);
                sb_put(&prompt, "\n```\n\n");
                has_injected_data = true;
            } else {
                /* No output — skip the AI call entirely. */
                free(inline_output);
                free(prompt.buf);
                if (silent_out) *silent_out = true;
                return NULL;
            }
        } else {
            sb_put(&prompt,
                   "## Script Error\n"
                   "The data-collection script failed. Report this to "
                   "the user.\n\n```\n");
            sb_put(&prompt, script_output ? script_output : "");
            sb_put(&prompt, "\n```\n\n");
            has_injected_data = true;
        }
        free(inline_output);
        inline_output = NULL;
    }

    /* ── 2. context_from injection ─────────────────────────────── */
    const json_t *context_from =
        json_object_get((json_t *)job, "context_from");
    if (context_from &&
        (json_node_is_array(context_from) ||
         json_node_is_string(context_from))) {
        char *output_dir = cronjobs_output_dir();
        size_t n_src = json_node_is_array(context_from)
            ? json_len(context_from) : 1;
        for (size_t i = 0; i < n_src && output_dir; i++) {
            const char *src_id;
            if (json_node_is_array(context_from)) {
                const json_t *item = json_get(context_from, i);
                src_id = json_node_is_string(item)
                    ? json_string_value(item) : NULL;
            } else {
                src_id = json_string_value(context_from);
            }
            if (!is_hex_job_id(src_id)) continue;   /* traversal guard */

            char jdir[PATH_MAX];
            snprintf(jdir, sizeof(jdir), "%s/%s", output_dir, src_id);
            char *latest = latest_md_in_dir(jdir);
            if (!latest) continue;
            char *text = read_file_all(latest);
            free(latest);
            if (!text) continue;
            strip_inplace2(text);

            /* truncate to 8000 chars */
            const size_t MAX_CONTEXT_CHARS = 8000;
            if (strlen(text) > MAX_CONTEXT_CHARS) {
                text[MAX_CONTEXT_CHARS] = '\0';
                char *t2 = malloc(MAX_CONTEXT_CHARS + 40);
                if (t2) {
                    snprintf(t2, MAX_CONTEXT_CHARS + 40,
                             "%s\n\n[... output truncated ...]", text);
                    free(text);
                    text = t2;
                }
            }
            if (text[0]) {
                char hdr[160];
                snprintf(hdr, sizeof(hdr),
                         "## Output from job '%s'\n", src_id);
                sb_put(&prompt, hdr);
                sb_put(&prompt,
                       "The following is the most recent output from a "
                       "preceding cron job. Use it as context for your "
                       "analysis.\n\n```\n");
                sb_put(&prompt, text);
                sb_put(&prompt, "\n```\n\n");
                has_injected_data = true;
            }
            free(text);
        }
        free(output_dir);
    }

    /* ── 3. cron hint + user prompt ────────────────────────────── */
    sb_t final_prompt;
    if (!sb_init(&final_prompt)) { free(prompt.buf); return NULL; }
    sb_put(&final_prompt, CRON_HINT);
    sb_put(&final_prompt, prompt.buf);   /* injected blocks first */
    sb_put(&final_prompt, user_prompt);
    free(prompt.buf);

    /* ── 4. skills normalization ───────────────────────────────── */
    const json_t *skills = json_object_get((json_t *)job, "skills");
    const char *legacy = json_get_str((json_t *)job, "skill", NULL);

    /* Build the effective skill-name list. */
    char *skill_names[64];
    size_t n_skills = 0;
    if (skills && json_node_is_array(skills)) {
        for (size_t i = 0; i < json_len(skills) && n_skills < 64; i++) {
            const json_t *item = json_get(skills, i);
            const char *s = json_node_is_string(item)
                ? json_string_value(item) : NULL;
            if (!s) continue;
            char *dup = strdup(s);
            strip_inplace2(dup);
            if (dup[0]) skill_names[n_skills++] = dup;
            else free(dup);
        }
    } else if (skills && json_node_is_string(skills)) {
        const char *s = json_string_value(skills);
        char *dup = strdup(s ? s : "");
        strip_inplace2(dup);
        if (dup[0]) skill_names[n_skills++] = dup;
        else free(dup);
    } else if (!skills && legacy && legacy[0]) {
        char *dup = strdup(legacy);
        strip_inplace2(dup);
        if (dup[0]) skill_names[n_skills++] = dup;
        else free(dup);
    }

    if (n_skills == 0) {
        char *out = scheduler_scan_assembled_cron_prompt(
            final_prompt.buf, job, false, has_injected_data,
            user_prompt, error_out);
        free(final_prompt.buf);
        return out;
    }

    /* ── 5. skill/bundle expansion ─────────────────────────────── */
    sb_t parts;
    if (!sb_init(&parts)) {
        for (size_t i = 0; i < n_skills; i++) free(skill_names[i]);
        free(final_prompt.buf);
        return NULL;
    }
    char skipped_csv[2048] = "";
    bool any_parts = false;

    const char *hermes_home = slermes_home();
    const char *job_id = json_get_str((json_t *)job, "id", "");

    for (size_t i = 0; i < n_skills; i++) {
        const char *skill_name = skill_names[i];
        const char *lookup = skill_name;
        while (*lookup == '/') lookup++;   /* lstrip("/") */

        /* Bundles shadow skills with the same slug. */
        char *bundle_key = resolve_bundle_command_key(lookup);
        if (bundle_key) {
            json_t *payload =
                build_bundle_invocation_message(bundle_key, "");
            free(bundle_key);
            if (payload) {
                const char *msg = json_get_str(payload, "message", "");
                if (any_parts) sb_put(&parts, "\n\n");
                sb_put(&parts, msg);
                any_parts = true;
                json_free(payload);
                continue;
            }
            /* bundle loaded zero skills — skip with notice */
            if (skipped_csv[0])
                strncat(skipped_csv, ", ",
                        sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            strncat(skipped_csv, skill_name,
                    sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            continue;
        }

        /* Plain skill via the tools surface. */
        char args[512];
        snprintf(args, sizeof(args), "{\"name\":\"%s\"}", lookup);
        char *resp = skills_view_handler(args, job_id[0] ? job_id : NULL);
        json_t *doc = resp ? json_parse(resp, NULL) : NULL;
        free(resp);

        const char *content = doc ? json_get_str(doc, "content", NULL) : NULL;
        if (!doc || !content || !content[0] ||
            json_obj_get(doc, "error")) {
            if (skipped_csv[0])
                strncat(skipped_csv, ", ",
                        sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            strncat(skipped_csv, skill_name,
                    sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            if (doc) json_free(doc);
            continue;
        }

        /* Bump usage so the curator sees the skill as used. */
        if (hermes_home) skill_usage_bump_use(hermes_home, lookup);

        char *cdup = strdup(content);
        strip_inplace2(cdup);
        if (any_parts) sb_put(&parts, "\n\n");
        char note[512];
        snprintf(note, sizeof(note),
            "[IMPORTANT: The user has invoked the \"%s\" skill, indicating "
            "they want you to follow its instructions. The full skill "
            "content is loaded below.]\n\n", skill_name);
        sb_put(&parts, note);
        sb_put(&parts, cdup);
        free(cdup);
        any_parts = true;
        json_free(doc);
    }

    for (size_t i = 0; i < n_skills; i++) free(skill_names[i]);

    /* skipped notice goes FIRST */
    sb_t assembled;
    if (!sb_init(&assembled)) {
        free(parts.buf); free(final_prompt.buf);
        return NULL;
    }
    if (skipped_csv[0]) {
        char notice[4600];
        snprintf(notice, sizeof(notice),
            "[IMPORTANT: The following skill(s) were listed for this job "
            "but could not be found and were skipped: %s. Start your "
            "response with a brief notice so the user is aware, e.g.: "
            "'⚠️ Skill(s) not found and skipped: %s']",
            skipped_csv, skipped_csv);
        sb_put(&assembled, notice);
        if (any_parts) sb_put(&assembled, "\n");
    }
    sb_put(&assembled, parts.buf);
    free(parts.buf);

    if (final_prompt.buf[0]) {
        sb_put(&assembled, "\n\nThe user has provided the following "
                           "instruction alongside the skill invocation: ");
        sb_put(&assembled, final_prompt.buf);
    }
    free(final_prompt.buf);

    char *out = scheduler_scan_assembled_cron_prompt(
        assembled.buf, job, true, has_injected_data, user_prompt, error_out);
    free(assembled.buf);
    return out;
}
