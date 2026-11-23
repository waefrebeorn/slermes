#ifndef NOC_W11_APPROVALHO_875E9D_H
#define NOC_W11_APPROVALHO_875E9D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_approvalho_875e9d_msys_drive_spellings @ tools/approval_home_fold.py:_msys_drive_spellings */
struct json_t *w11_approvalho_875e9d_msys_drive_spellings(const char *path);

/* PoP: w11_approvalho_875e9d_path_below @ tools/approval_home_fold.py:_path_below
 * Returns the relative tail (JSON string) or JSON null. win32 folds case. */
struct json_t *w11_approvalho_875e9d_path_below(const char *root, const char *path, int win32);

/* PoP: w11_approvalho_875e9d_home_spellings @ tools/approval_home_fold.py:_home_spellings
 * Environment values are injected explicitly instead of read from the CRT. */
struct json_t *w11_approvalho_875e9d_home_spellings(const struct json_t *paths, const char *command,
        const char *home_env, const char *expand_home, const char *local_appdata,
        const char *roaming, const char *profile, int win32);

/* PoP: w11_approvalho_875e9d_fold_home_prefixes @ tools/approval_home_fold.py:_fold_home_prefixes */
struct json_t *w11_approvalho_875e9d_fold_home_prefixes(const char *command, const struct json_t *paths,
        const char *replacement, int win32);

/* PoP: w11_approvalho_875e9d_rewrite_user_home_ex @ tools/approval_home_fold.py:_rewrite_resolved_user_home */
struct json_t *w11_approvalho_875e9d_rewrite_user_home_ex(const char *command, const char *expand_home,
        const char *real_home, const char *home_env, int win32);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_APPROVALHO_875E9D_H */
