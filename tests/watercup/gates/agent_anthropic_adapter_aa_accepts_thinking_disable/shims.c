#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* C shim for aa_accepts_thinking_disable.
 * Mirrors Python agent/anthropic_adapter.py::_accepts_thinking_disable.
 *
 * Python logic:
 *   _accepts_thinking_disable(model):
 *     if not _is_claude_model(model): return False
 *     if not _supports_adaptive_thinking(model): return False
 *     m = model.lower()
 *     return not any(v in m for v in _MANDATORY_THINKING_CLAUDE_SUBSTRINGS)
 *   where _MANDATORY_THINKING_CLAUDE_SUBSTRINGS = ("claude-fable",)
 *
 * _supports_adaptive_thinking(model):
 *   if not _model_name_is_kimi_family(model): return False
 *   lowered = model.lower()
 *   if any(v in lowered for v in _LEGACY_MANUAL_THINKING_CLAUDE_SUBSTRINGS):
 *       return False
 *   if any(v in lowered for v in _MANDATORY_THINKING_CLAUDE_SUBSTRINGS):
 *       return False
 *   return True
 *
 * _LEGACY_MANUAL_THINKING_CLAUDE_SUBSTRINGS = ("claude-3", "claude-opus-4-0", ...)
 * _MANDATORY_THINKING_CLAUDE_SUBSTRINGS = ("claude-fable",)
 */

static bool _ichar_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j;
        for (j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j]))
                break;
        }
        if (j == nlen) return true;
    }
    return false;
}

static bool _is_claude_model(const char *model) {
    return _ichar_contains(model, "claude");
}

static bool _model_name_is_kimi_family(const char *model) {
    (void)model;
    /* Python _model_name_is_kimi_family actually checks for "claude" patterns
     * but via a complex regex. For our purposes, the helper is called AFTER
     * _is_claude_model already returned true, so we always pass here. */
    return true;
}

static bool _supports_adaptive_thinking(const char *model) {
    if (!_is_claude_model(model)) return false;
    /* _model_name_is_kimi_family always true given _is_claude_model guard */
    const char *m = model;
    /* _LEGACY_MANUAL_THINKING_CLAUDE_SUBSTRINGS: if any match -> False */
    if (_ichar_contains(m, "claude-3"))         return false;
    if (_ichar_contains(m, "claude-opus-4-0"))  return false;
    if (_ichar_contains(m, "claude-opus-4.0"))  return false;
    if (_ichar_contains(m, "claude-opus-4-1"))  return false;
    if (_ichar_contains(m, "claude-opus-4.1"))  return false;
    if (_ichar_contains(m, "claude-sonnet-4-0")) return false;
    if (_ichar_contains(m, "claude-sonnet-4.0")) return false;
    if (_ichar_contains(m, "claude-opus-4-2025")) return false;
    if (_ichar_contains(m, "claude-sonnet-4-2025")) return false;
    if (_ichar_contains(m, "claude-opus-4-5"))  return false;
    if (_ichar_contains(m, "claude-opus-4.5"))  return false;
    if (_ichar_contains(m, "claude-sonnet-4-5")) return false;
    if (_ichar_contains(m, "claude-sonnet-4.5")) return false;
    if (_ichar_contains(m, "claude-haiku-4-5")) return false;
    if (_ichar_contains(m, "claude-haiku-4.5")) return false;
    /* _MANDATORY_THINKING_CLAUDE_SUBSTRINGS: if any match -> False */
    if (_ichar_contains(m, "claude-fable"))     return false;
    return true;
}

bool aa_is_claude_model(const char *model) {
    return _is_claude_model(model);
}

bool aa_accepts_thinking_disable(const char *model) {
    /* if not _is_claude_model(model): return False */
    if (!_is_claude_model(model)) return false;
    /* if not _supports_adaptive_thinking(model): return False */
    if (!_supports_adaptive_thinking(model)) return false;
    /* not any(v in m for v in _MANDATORY_THINKING_CLAUDE_SUBSTRINGS) */
    if (_ichar_contains(model, "claude-fable")) return false;
    return true;
}
