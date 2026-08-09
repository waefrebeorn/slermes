/*
 * cua_doctor.h — C11 port of tools/computer_use/doctor.py (NS-610).
 */

#ifndef HERMES_CUA_DOCTOR_H
#define HERMES_CUA_DOCTOR_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _STATUS_GLYPH @ tools/computer_use/doctor.py:_STATUS_GLYPH */
/* PoP: _OVERALL_GLYPH @ tools/computer_use/doctor.py:_OVERALL_GLYPH */

/* ── Validation ──────────────────────────────────────────── */

/* PoP: _is_valid_health_report @ tools/computer_use/doctor.py:_is_valid_health_report */
bool cua_doctor_is_valid_health_report(const json_t *payload);

/* PoP: _normalize_version_token @ tools/computer_use/doctor.py:_normalize_version_token */
char *cua_doctor_normalize_version_token(const char *text);

/* PoP: _platform_name @ tools/computer_use/doctor.py:_platform_name */
void cua_doctor_platform_name(char *out, size_t out_sz);

/* ── Identity ────────────────────────────────────────────── */

/* PoP: _build_identity @ tools/computer_use/doctor.py:_build_identity */
json_t *cua_doctor_build_identity(const char *binary,
                                  const char *cli_version,
                                  const char *health_report_driver_version);

/* ── Report extraction ───────────────────────────────────── */

/* PoP: _extract_health_report_from_result @ tools/computer_use/doctor.py:_extract_health_report_from_result
 * Returns CUA_DOC_OK / CUA_DOC_UNAVAILABLE / CUA_DOC_PROTOCOL_ERR. */
#define CUA_DOC_OK 0
#define CUA_DOC_UNAVAILABLE 1
#define CUA_DOC_PROTOCOL_ERR 2
int cua_doctor_extract_health_report(const json_t *result, json_t **out, char **out_msg);

/* ── CLI version (caller pre-captures subprocess output) ─── */

/* PoP: _read_cli_version @ tools/computer_use/doctor.py:_read_cli_version */
char *cua_doctor_read_cli_version(const char *stdout_text, const char *stderr_text);

/* PoP: _cli_driver_version @ tools/computer_use/doctor.py:_cli_driver_version */
typedef struct { char *status; char *version_or_msg; } cua_doc_ver_t;
cua_doc_ver_t cua_doctor_cli_driver_version(const char *combined_text, int return_code);

/* PoP: _cli_doctor_snippet @ tools/computer_use/doctor.py:_cli_doctor_snippet */
char *cua_doctor_cli_doctor_snippet(const char *raw_output);

/* ── Fallback report composition ─────────────────────────── */

/* Probe results the caller (process infra) pre-collects by spawning cua-driver.
 * Faithful port of _drive_fallback_probes's return dict keys. */
typedef struct {
    char *init_version;      /* from MCP initialize serverInfo.version, or NULL */
    char *binary_version;    /* parsed from --version, or NULL */
    bool  binary_version_pass;
    json_t *permissions;     /* check_permissions structuredContent, or NULL */
    char *permissions_error; /* or NULL */
    /* Python _drive_fallback_probes returns list_apps_ok: None|True|False and
     * list_apps_count: None|int. We model these as tri-state ints. */
    int   list_apps_ok;       /* -1 = None (not probed), 0 = False, 1 = True */
    char *list_apps_error;    /* or NULL */
    int   list_apps_count;    /* -1 = None, >=0 = value */
    char *cli_doctor_text;   /* first line of `cua-driver doctor`, or NULL */
    bool  cli_doctor_ok;
} cua_doctor_probes_t;

/* PoP: _compose_fallback_report @ tools/computer_use/doctor.py:_compose_fallback_report */
/* PoP: _drive_fallback_probes @ tools/computer_use/doctor.py:_drive_fallback_probes */
json_t *cua_doctor_compose_fallback_report(const char *binary,
                                           const cua_doctor_probes_t *probes,
                                           const char *reason,
                                           double timeout);

/* PoP: _drive_health_report_or_fallback @ tools/computer_use/doctor.py:_drive_health_report_or_fallback */
json_t *cua_doctor_health_report_or_fallback(json_t *maybe_report,
                                             bool report_available,
                                             const char *unavail_reason,
                                             const cua_doctor_probes_t *probes,
                                             const char *binary,
                                             const char *reason,
                                             char **out_msg);

/* PoP: _print_text_report @ tools/computer_use/doctor.py:_print_text_report */
void cua_doctor_print_text_report(FILE *out, const json_t *report, bool color, const json_t *identity);

/* PoP: run_doctor @ tools/computer_use/doctor.py:run_doctor
 * High-level entry: health_report JSON (if any) + pre-collected probes ->
 * prints text or JSON to *out*, returns 0/1/2 per cua-driver exit codes. */
int cua_doctor_run(FILE *out, const json_t *maybe_report, bool report_available,
                   const char *unavail_reason, const cua_doctor_probes_t *probes,
                   const char *binary, bool json_output, bool color);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CUA_DOCTOR_H */
