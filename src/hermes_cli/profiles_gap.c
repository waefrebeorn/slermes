/* ── gap-fillers for hermes_cli/profiles.py functions missing from C ─────────── */
/*
 * Implements functions that are DA-1 (c_location=None) in the parity scan:
 *   profile_delete_custom_profile       — remove a custom profile dir
 *   profile_export_secrets_json         — extract secrets as JSON for export
 *   profile_import_profile              — import (copy in) a profile from a dir
 *
 * Follows the naming convention: cfn = profile_<python_method_basename>
 * and carries a PoP header comment pointing at the Python source.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <std...[truncated]