/*
 * PoP: w10_macossigni_a9cf03_sign_managed_python_ex @ hermes_cli/macos_signing.py:sign_managed_python
 *
 * Faithful port of the boolean control flow. Logging uses the captured stderr/stdout
 * in Python but never affects the return value, so only the exit codes and the
 * deterministic environment inputs are needed here.
 */
#include "noc_w10_macossigni_a9cf03.h"
#include <string.h>

int w10_macossigni_a9cf03_sign_managed_python_ex(int sign_raises,
        const char *system_name, int codesign_available,
        int sign_rc, int verify_rc) {
    /* if platform.system() != "Darwin": return False */
    if (!system_name || strcmp(system_name, "Darwin")) return 0;
    /* codesign = shutil.which("codesign"); if not codesign: return False */
    if (!codesign_available) return 0;
    /* subprocess.run(...) wrapped in try/except Exception: return False */
    if (sign_raises) return 0;
    /* signed = run(...); if signed.returncode != 0: return False */
    if (sign_rc != 0) return 0;
    /* verified = run(...); if verified.returncode != 0: return False */
    if (verify_rc != 0) return 0;
    return 1;
}
