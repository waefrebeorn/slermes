#ifndef NOC_W9_MAINDESKTOP_B77781_H
#define NOC_W9_MAINDESKTOP_B77781_H

#ifdef __cplusplus
extern "C" {
#endif

struct json_t;

/* PoP: w9_maindesktop_b77781_macos_signing_downgrade_error @ hermes_cli/main_desktop.py:_macos_signing_downgrade_error
 * installed/rebuilt are {team, identifier, verified} objects; rebuilt == NULL
 * means Python None. Returns a JSON string with the refusal reason, or JSON
 * null when the swap is allowed. */
struct json_t *w9_maindesktop_b77781_macos_signing_downgrade_error(
    const struct json_t *installed, const struct json_t *rebuilt);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_MAINDESKTOP_B77781_H */
