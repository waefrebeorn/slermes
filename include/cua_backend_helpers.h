/*
 * cua_backend_helpers.h — public API for the pure tools/computer_use/
 * cua_backend.py parsing helpers. Opaque, minimal includes.
 */

#ifndef CUA_BACKEND_HELPERS_H
#define CUA_BACKEND_HELPERS_H

#include <stddef.h>
#include <stdlib.h>

/* Best-effort PNG/JPEG dimension sniffing. Sets *out_w/*out_h; returns 1 if
 * a dimension was found, else 0 with 0,0. (PoP: _image_dimensions_from_bytes) */
int cua_image_dimensions_from_bytes(const unsigned char *raw, size_t len,
                                    int *out_w, int *out_h);

/* Split get_window_state text into (summary, tree). Both malloc'd; tree is ""
 * when no newline. Caller frees. (PoP: _split_tree_text) */
void cua_split_tree_text(const char *full_text, char **out_summary, char **out_tree);

/* Parse a key combo like "cmd+s" into (key, modifiers[]). key is malloc'd
 * (NULL when none); modifiers is a malloc'd array of malloc'd strings with
 * count in *out_nmods. Caller frees key, each modifier, and the array.
 * (PoP: _parse_key_combo) */
char *cua_parse_key_combo(const char *keys, char ***out_modifiers, int *out_nmods);

/* Free a modifiers array produced by cua_parse_key_combo. */
static inline void cua_free_modifiers(char **mods, int n)
{
    if (!mods) return;
    for (int i = 0; i < n; i++) free(mods[i]);
    free(mods);
}

/* PoP: _wsl_windows_path_to_posix @ tools/computer_use/cua_backend.py:_wsl_windows_path_to_posix */
/* Translate a Windows path (C:\\Users\\foo) to DrvFS POSIX (/mnt/c/Users/foo).
 * Callers pass is_wsl=1 when on WSL; non-Windows paths returned unchanged.
 * Caller frees result. */
char *cua_wsl_windows_path_to_posix(const char *path, int is_wsl);

#endif /* CUA_BACKEND_HELPERS_H */
