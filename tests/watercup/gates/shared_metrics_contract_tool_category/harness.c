/* gate: shared_metrics_contract_tool_category — traced C copy of
 * src/hermes_cli/smc_event_extractors.c::smc_tool_category.
 * Faithful port of LIVE Python
 * hermes_cli/observability/shared_metrics_contract.py::tool_category.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static char *dup_lower_strip(const char *s) {
    while (*s && (unsigned char)*s <= ' ') s++;
    size_t n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= ' ') n--;
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static const char *tool_category(const char *toolset) {
    WCUP_BEGIN("tool_category");
    char *s = dup_lower_strip(toolset ? toolset : "");
    static const char *known[] = {
        "browser","code_execution","communication","computer_use","delegation","file",
        "home_automation","mcp","media","memory","other","planning","project","scheduler",
        "skill","terminal","unknown","web", NULL
    };
    int empty = (*s == '\0');
    WCUP_BRANCH(empty);
    if (empty) {
        WCUP_VALSTR("unknown");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup); WCUP_END("tool_category"); free(s);
        return "unknown";
    }
    int in_known = 0;
    for (int i = 0; known[i]; i++) if (strcmp(s, known[i]) == 0) { in_known = 1; break; }
    WCUP_BRANCH(in_known);
    const char *result;
    if (in_known) {
        result = s;
        WCUP_VALSTR(result);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup); WCUP_END("tool_category"); free(s);
        return result;
    }
    WCUP_BRANCH(strncmp(s, "mcp", 3) == 0);
    if (strncmp(s, "mcp", 3) == 0) result = "mcp";
    else {
        WCUP_BRANCH(strncmp(s, "browser", 7) == 0);
        if (strncmp(s, "browser", 7) == 0) result = "browser";
        else {
            WCUP_BRANCH(strncmp(s, "image", 5) == 0 || strncmp(s, "tts", 3) == 0 ||
                        strncmp(s, "video", 5) == 0 || strncmp(s, "vision", 6) == 0);
            if (strncmp(s, "image", 5) == 0 || strncmp(s, "tts", 3) == 0 ||
                strncmp(s, "video", 5) == 0 || strncmp(s, "vision", 6) == 0) result = "media";
            else {
                WCUP_BRANCH(strncmp(s, "homeassistant", 13) == 0);
                if (strncmp(s, "homeassistant", 13) == 0) result = "home_automation";
                else {
                    WCUP_BRANCH(strcmp(s, "clarify") == 0 || strcmp(s, "kanban") == 0 ||
                                strcmp(s, "todo") == 0);
                    if (strcmp(s, "clarify") == 0 || strcmp(s, "kanban") == 0 ||
                        strcmp(s, "todo") == 0) result = "planning";
                    else {
                        WCUP_BRANCH(strcmp(s, "session_search") == 0);
                        if (strcmp(s, "session_search") == 0) result = "memory";
                        else {
                            WCUP_BRANCH(strcmp(s, "cronjob") == 0);
                            if (strcmp(s, "cronjob") == 0) result = "scheduler";
                            else {
                                WCUP_BRANCH(strcmp(s, "skills") == 0);
                                if (strcmp(s, "skills") == 0) result = "skill";
                                else {
                                    WCUP_BRANCH(strcmp(s, "x_search") == 0);
                                    if (strcmp(s, "x_search") == 0) result = "web";
                                    else {
                                        WCUP_BRANCH(strncmp(s, "discord", 7) == 0 ||
                                                    strncmp(s, "email", 5) == 0 ||
                                                    strncmp(s, "feishu", 6) == 0 ||
                                                    strncmp(s, "hermes-yuanbao", 14) == 0 ||
                                                    strncmp(s, "slack", 5) == 0 ||
                                                    strncmp(s, "sms", 3) == 0);
                                        if (strncmp(s, "discord", 7) == 0 ||
                                            strncmp(s, "email", 5) == 0 ||
                                            strncmp(s, "feishu", 6) == 0 ||
                                            strncmp(s, "hermes-yuanbao", 14) == 0 ||
                                            strncmp(s, "slack", 5) == 0 ||
                                            strncmp(s, "sms", 3) == 0) result = "communication";
                                        else result = "other";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    WCUP_VALSTR(result);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup); WCUP_END("tool_category"); free(s);
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tool_category <toolset>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) tool_category(argv[i]);
    return 0;
}
