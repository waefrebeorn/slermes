/*
 * noc_w21_approvalwi_56c32a.h -- public C API for the port of
 *   tools/approval_windows_hardline.py (wave 21 batch 5).
 *
 * The Windows analogue of the shutdown / mkfs / root-rm hardline rules: the
 * anchored pattern text (built from the caller's command-position prefix), the
 * -WhatIf-exempt variants without the exemption, and the per-statement -WhatIf
 * verdict. The patterns are byte-identical to the Python source and are
 * evaluated with the native PCRE2 API (the build links -lpcre2-8), which
 * compiles Python `re` syntax verbatim.
 */
#ifndef NOC_W21_APPROVALWI_56C32A_H
#define NOC_W21_APPROVALWI_56C32A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w21_approvalwi_56c32a_win_cmdpos @ tools/approval_windows_hardline.py:_win_cmdpos
 * cmdpos (+ a CR break) optionally followed by a powershell/pwsh/cmd launcher
 * and its switches and an opening quote. Caller frees. */
char *w21_approvalwi_56c32a_win_cmdpos(const char *cmdpos);

/* PoP: w21_approvalwi_56c32a_windows_hardline_patterns @ tools/approval_windows_hardline.py:windows_hardline_patterns
 * [[pattern, description], ...] for the six Windows hardline rules, anchored at
 * cmdpos exactly as the live list is. */
json_t *w21_approvalwi_56c32a_windows_hardline_patterns(const char *cmdpos);

/* PoP: w21_approvalwi_56c32a_windows_whatif_strict_patterns @ tools/approval_windows_hardline.py:windows_whatif_strict_patterns
 * {"<description>": "<pattern>"} for the two -WhatIf-exempt rules, without the
 * exemption. */
json_t *w21_approvalwi_56c32a_windows_whatif_strict_patterns(const char *cmdpos);

/* PoP: w21_approvalwi_56c32a_whatif_preview_is_real @ tools/approval_windows_hardline.py:whatif_preview_is_real
 * True when every guarded cmdlet previews with its own real -WhatIf and no other
 * destructive verb appears anywhere. Statements split on CR/LF, NEL, U+2028,
 * U+2029, ";", "&" and "|", as the live code splits them. */
json_t *w21_approvalwi_56c32a_whatif_preview_is_real(const char *command);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_APPROVALWI_56C32A_H */
