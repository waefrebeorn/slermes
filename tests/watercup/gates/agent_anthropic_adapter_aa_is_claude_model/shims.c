#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static bool _is_claude_model(const char *model) {
    if (!model) return false;
    size_t ml = strlen(model);
    char *low = malloc(ml + 1);
    if (!low) return false;
    for (size_t k = 0; k < ml; k++)
        low[k] = (char)((model[k] >= 'A' && model[k] <= 'Z') ? model[k] + 32 : model[k]);
    low[ml] = '\0';
    bool hit = strstr(low, "claude") != NULL;
    free(low);
    return hit;
}

bool aa_is_claude_model(const char *model) {
    return _is_claude_model(model);
}
