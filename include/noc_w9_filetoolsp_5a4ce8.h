#ifndef NOC_W9_FILETOOLSP_5A4CE8_H
#define NOC_W9_FILETOOLSP_5A4CE8_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w9_filetoolsp_5a4ce8_normalize_posix @ tools/file_tools_paths.py:_normalize_without_host_deref */
char *w9_filetoolsp_5a4ce8_normalize_posix(const char *path);
/* PoP: w9_filetoolsp_5a4ce8_ssh_escapes_home @ tools/file_tools_paths.py:_ssh_path_escapes_home */
int w9_filetoolsp_5a4ce8_ssh_escapes_home(const char *resolved);
/* PoP: w9_filetoolsp_5a4ce8_sentinel @ tools/file_tools_paths.py:_TERMINAL_CWD_SENTINELS */
int w9_filetoolsp_5a4ce8_sentinel(const char *value);
/* PoP: w9_filetoolsp_5a4ce8_expand_tilde @ tools/file_tools_paths.py:_expand_tilde */
char *w9_filetoolsp_5a4ce8_expand_tilde(const char *path, const char *home);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_FILETOOLSP_5A4CE8_H */
