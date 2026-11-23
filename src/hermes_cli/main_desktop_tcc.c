/*
 * main_desktop_tcc.c — C port of upstream Hermes hermes_cli/main_desktop_tcc.py.
 * Gate: w15_maindeskto_7d5d41
 *
 * PoP: w15_maindeskto_7d5d41_macos_codesigning_identity_valid @ hermes_cli/main_desktop_tcc.py:_macos_codesigning_identity_valid
 *
 * Python:
 *   try:
 *       result = subprocess.run([...], capture_output=True, text=True, ...)
 *   except Exception:
 *       return False
 *   return f'"{identity}"' in (result.stdout or "")
 * The subprocess result is an explicit fixture seam (`ok`, `stdout_text`):
 * the oracle injects a fake subprocess.run that either raises or returns the
 * given stdout, so the Windows host never invokes `security`.
 */
#include "noc_w15_maindeskto_7d5d41.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

json_t *w15_maindeskto_7d5d41_macos_codesigning_identity_valid(const char *identity,
                                                               const char *stdout_text,
                                                               int ok) {
    if (!ok || !stdout_text) return json_bool(false);
    const char *id_s = identity ? identity : "";
    size_t n = strlen(id_s);
    char *needle = (char *)malloc(n + 3);
    if (!needle) return json_bool(false);
    needle[0] = '"';
    if (n) memcpy(needle + 1, id_s, n);
    needle[n + 1] = '"';
    needle[n + 2] = '\0';
    int found = strstr(stdout_text, needle) != NULL;
    free(needle);
    return json_bool(found ? true : false);
}
