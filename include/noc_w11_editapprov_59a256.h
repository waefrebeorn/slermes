#ifndef NOC_W11_EDITAPPROV_59A256_H
#define NOC_W11_EDITAPPROV_59A256_H

/* C API for the acp_adapter/edit_approval.py port (gate w11_editapprov_59a256). */
#include "hermes_json.h"

/* _extract_v4a_patch_paths(patch_body) -> list[str] */
json_t *w11_editapprov_59a256_extract_v4a_patch_paths(const char *patch_body);
/* _path_is_sensitive(Path(path)) -> bool (path already expanduser()'d by caller) */
bool w11_editapprov_59a256_path_is_sensitive(const char *path);
/* _denied(message) -> malloc'd json.dumps({"error": message}, ensure_ascii=False) text */
char *w11_editapprov_59a256_denied(const char *message);

#endif /* NOC_W11_EDITAPPROV_59A256_H */
