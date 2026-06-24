/*
 * port_model_normalize.c — Port of Python hermes_cli/model_normalize.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Known DeepSeek models */
static const char* DEEPSEEK_KNOWN[] = {
    "deepseek-chat", "deepseek-reasoner", "deepseek-v4-pro", "deepseek-v4-flash", NULL
};

/* Reasoner keywords */
static const char* REASONER_KEYWORDS[] = {
    "r1", "think", "reasoning", "cot", "reasoner", NULL
};

/*
 * _normalize_for_deepseek — Map a model input to a DeepSeek-accepted identifier.
 *
 * Python: def _normalize_for_deepseek(model_name: str) -> str:
 *   - Already a known canonical -> pass through
 *   - Matches deepseek-v<digit>... -> pass through
 *   - Contains reasoner keyword -> deepseek-reasoner
 *   - Everything else -> deepseek-chat
 */
/* Port of Python: _normalize_for_deepseek */
const char* _normalize_for_deepseek(const char* model_name)
{
    if (!model_name || !model_name[0]) {
        return "deepseek-chat";
    }

    /* Check known canonicals */
    for (int i = 0; DEEPSEEK_KNOWN[i]; i++) {
        if (strcmp(model_name, DEEPSEEK_KNOWN[i]) == 0) {
            return model_name;
        }
    }

    /* Check V-series pattern: deepseek-v<digit>... */
    if (strncmp(model_name, "deepseek-v", 10) == 0) {
        if (isdigit(model_name[10])) {
            return model_name;
        }
    }

    /* Check reasoner keywords (case-insensitive) */
    char* lower = strdup(model_name);
    if (lower) {
        for (char* p = lower; *p; p++) *p = tolower(*p);
        for (int i = 0; REASONER_KEYWORDS[i]; i++) {
            if (strstr(lower, REASONER_KEYWORDS[i])) {
                free(lower);
                return "deepseek-reasoner";
            }
        }
        free(lower);
    }

    /* Default */
    return "deepseek-chat";
}
