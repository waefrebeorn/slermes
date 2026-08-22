/*
 * plugin_dev.h — faithful C11 port of hermes_cli/plugin_dev.py
 * ("hermes plugins doctor" validation logic).
 *
 * The full doctor loads plugins through the live Python runtime (imports,
 * registry mutation, socket denial). Those side effects are not portable;
 * this port carries the DETERMINISTIC decision surface 1:1 and routes the
 * runtime behind callers:
 *
 * PoP (Python -> C):
 *   DoctorFinding            -> pd_finding_t
 *   DoctorReport             -> pd_report_t (+ error/warning/ok)
 *   DoctorReport.format_text -> plugin_dev_format_text
 *   resolve_plugin_path      -> plugin_dev_resolve_plugin_path
 *   _check_manifest_v2       -> plugin_dev_check_manifest_v2
 *   registration diff ladder -> plugin_dev_check_registrations
 *   VALID_HOOKS              -> plugin_dev_is_valid_hook
 */
#ifndef HERMES_PLUGIN_DEV_H
#define HERMES_PLUGIN_DEV_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Injectable hook: importlib.metadata.version(dist) analog.
 * Return 1 and set *out (malloc'd) when installed; 0 when
 * PackageNotFoundError; -1 for any other exception (skipped). */
typedef int (*plugin_dev_dist_version_fn)(const char *dist, char **out);
extern plugin_dev_dist_version_fn plugin_dev_dist_version_hook;

typedef struct {
    char *level;     /* "error" | "warning" */
    char *message;
} pd_finding_t;

typedef struct {
    char *path;
    bool has_manifest;
    char *manifest_name;
    char *manifest_version;   /* may be NULL => "(no version)" */
    char *manifest_kind;
    pd_finding_t *findings;
    size_t n_findings;
    char **registered_tools;
    size_t n_registered_tools;
    char **registered_hooks;
    size_t n_registered_hooks;
} pd_report_t;

pd_report_t *pd_report_new(const char *path);
void pd_report_free(pd_report_t *r);
void pd_report_error(pd_report_t *r, const char *msg);
void pd_report_warning(pd_report_t *r, const char *msg);
bool pd_report_ok(const pd_report_t *r);

/* DoctorReport.format_text() -> malloc'd text. */
char *plugin_dev_format_text(const pd_report_t *r);

/* resolve_plugin_path(): returns malloc'd resolved directory or NULL, with
 * *out_error set to the FileNotFoundError message when not found. */
char *plugin_dev_resolve_plugin_path(const char *target,
                                     const char *hermes_home,
                                     const char *bundled_dir,
                                     const char *cwd,
                                     char **out_error);

/* _check_manifest_v2(report, manifest_json). manifest is a JSON object with
 * optional keys manifest_version, api_version, requires_plugins,
 * python_dependencies, config_schema. Appends findings to report. */
void plugin_dev_check_manifest_v2(pd_report_t *report, const json_t *manifest);

/* Registration diff ladders from doctor_plugin(). */
void plugin_dev_check_registrations(pd_report_t *report,
                                    char *const *declared_hooks, size_t nh_d,
                                    char *const *registered_hooks, size_t nh_r,
                                    char *const *declared_tools, size_t nt_d,
                                    char *const *registered_tools, size_t nt_r);

bool plugin_dev_is_valid_hook(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_PLUGIN_DEV_H */
