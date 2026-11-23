#ifndef NOC_W10_MACOSSIGNI_A9CF03_H
#define NOC_W10_MACOSSIGNI_A9CF03_H

/* PoP port of hermes_cli/macos_signing.py — managed interpreter identity.
 * sign_managed_python is environment/subprocess driven; the *_ex seam makes every
 * external input explicit so the C port can be differentially tested.
 * Returns 1 (True) / 0 (False), matching the Python bool. */
int w10_macossigni_a9cf03_sign_managed_python_ex(int sign_raises,
        const char *system_name, int codesign_available,
        int sign_rc, int verify_rc);
#endif /* NOC_W10_MACOSSIGNI_A9CF03_H */
