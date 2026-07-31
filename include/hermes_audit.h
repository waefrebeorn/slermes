/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_AUDIT_H
#define SLERMES_AUDIT_H

#include "hermes_core_types.h"

/* Security audit log */
bool audit_init(const char *log_dir);
void audit_shutdown(void);
void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail);
void audit_log_approval(const char *tool, const char *command, bool approved);
void audit_log_redaction(const char *context, const char *pattern_matched);
void audit_log_violation(const char *rule, const char *detail);
void audit_set_rotation(size_t max_size_kb, int max_files, int max_age_days);

#endif /* SLERMES_AUDIT_H */
