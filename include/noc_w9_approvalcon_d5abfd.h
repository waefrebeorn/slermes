#ifndef NOC_W9_APPROVALCON_D5ABFD_H
#define NOC_W9_APPROVALCON_D5ABFD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_approvalcon_d5abfd_normalize_mode @ tools/approval_context.py:_normalize_approval_mode */
char *w9_approvalcon_d5abfd_normalize_mode(const struct json_t *value);
/* PoP: w9_approvalcon_d5abfd_format_window @ tools/approval_context.py:format_approval_window
 * Returns "approval.window.<unit>_<one|other>|<count>" (the i18n key and count). */
char *w9_approvalcon_d5abfd_format_window(const char *seconds_dec);
/* PoP: w9_approvalcon_d5abfd_binary_mode @ tools/approval_context.py:_binary_approval_mode */
char *w9_approvalcon_d5abfd_binary_mode(const struct json_t *value);
/* PoP: w9_approvalcon_d5abfd_unattended_platform @ tools/approval_context.py:_is_unattended_platform_approval_context */
int w9_approvalcon_d5abfd_unattended_platform(const char *platform);
/* PoP: w9_approvalcon_d5abfd_prompt_authority @ tools/approval_context.py:prompt_authority */
struct json_t *w9_approvalcon_d5abfd_prompt_authority(const struct json_t *req, const struct json_t *env,
                                    const struct json_t *approvers);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_APPROVALCON_D5ABFD_H */
