#ifndef NOC_W12_UPDATECMD_C8ED08_H
#define NOC_W12_UPDATECMD_C8ED08_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_git.py (gate w12_updatecmd_c8ed08). */

/* _is_fork(origin_url) -> bool. */
json_t *w12_updatecmd_c8ed08_is_fork(const char *origin_url);
/* _has_http_code(stderr, codes[]) -> bool. */
json_t *w12_updatecmd_c8ed08_has_http_code(const char *stderr_text, const json_t *codes);
/* _classify_fetch_failure(stderr) -> string. */
json_t *w12_updatecmd_c8ed08_classify_fetch_failure(const char *stderr_text);

#endif /* NOC_W12_UPDATECMD_C8ED08_H */
