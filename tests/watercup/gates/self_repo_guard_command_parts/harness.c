/* gate: self_repo_guard_command_parts — traced copy of
 * src/tools/self_repo_guard.c::srg_command_parts (+ its static helpers)
 * (LIVE twin: tools/self_repo_guard.py::_command_parts).
 *
 * argv input per cup: space-separated word list.
 * consume_options() internals are untraced on BOTH sides (shared helper).
 *
 * Ladder per word:
 *   B1: word is a VAR=value assignment
 *   B2: executable == "sudo"
 *   B3: executable == "env"
 *   B4: executable == "command"
 *     B5: next word in {-v,-V} -> return env,None,[]
 *   B6: executable in simple wrappers
 *   else -> return env, word, rest
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static const char *const SUDO_OPTS[] = {
    "-C", "--chdir", "-c", "--close-from", "-g", "--group", "-h", "--host",
    "-p", "--prompt", "-R", "--chroot", "-T", "--command-timeout", "-u",
    "--user", NULL};
static const char *const ENV_OPTS[] = {
    "-a", "--argv0", "-C", "--chdir", "-S", "--split-string", "-u", "--unset", NULL};
static const char *const EXEC_OPTS[] = {"-a", NULL};
static const char *const TIME_OPTS[] = {"-f", "--format", "-o", "--output", NULL};
static const char *const SIMPLE_WRAPPERS[] = {"builtin", "exec", "nohup", "setsid", "time", NULL};

static bool str_in(const char *const *list, const char *value) {
    if (!list || !value) return false;
    for (int i = 0; list[i]; i++)
        if (strcmp(list[i], value) == 0) return true;
    return false;
}

/* _ASSIGNMENT_RE: [A-Za-z_][A-Za-z0-9_]*=(.*) fullmatch */
static bool is_assignment(const char *word) {
    if (!word || !*word) return false;
    if (!(isalpha((unsigned char)word[0]) || word[0] == '_')) return false;
    const char *eq = strchr(word, '=');
    if (!eq) return false;
    for (const char *p = word; p < eq; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_')) return false;
    return true;
}

/* basename, backslash->slash, strip .exe, lowercase */
static char *executable_name(const char *value) {
    if (!value) return strdup("");
    char normalized[512];
    snprintf(normalized, sizeof normalized, "%s", value);
    for (char *p = normalized; *p; p++) if (*p == '\\') *p = '/';
    const char *base = strrchr(normalized, '/');
    const char *name = base ? base + 1 : normalized;
    size_t nl = strlen(name);
    size_t keep = nl;
    if (nl > 4 && strcmp(name + nl - 4, ".exe") == 0) keep = nl - 4;
    char *out = malloc(keep + 1);
    for (size_t i = 0; i < keep; i++) out[i] = (char)tolower((unsigned char)name[i]);
    out[keep] = '\0';
    return out;
}

#define MAXW 64

static int consume_options(char **words, size_t start, const char *const *opts) {
    /* untraced shared helper — faithful copy of srg_consume_options */
    size_t index = start;
    while (words && words[index]) {
        const char *option = words[index];
        if (strcmp(option, "--") == 0) return (int)(index + 1);
        if (option[0] != '-' || strcmp(option, "-") == 0) break;
        const char *eq = strchr(option, '=');
        size_t olen = eq ? (size_t)(eq - option) : strlen(option);
        char optname[256];
        memcpy(optname, option, olen); optname[olen] = '\0';
        if (!eq && str_in(opts, optname)) index += 2;
        else index += 1;
    }
    return (int)index;
}

static void command_parts(char **words) {
    WCUP_BEGIN("srg_command_parts");
    char envacc[4096]; size_t eo = 0; envacc[0] = '\0';
    size_t index = 0;

    while (words && words[index]) {
        if (is_assignment(words[index])) {
            WCUP_BRANCH(1);
            char *eq = strchr(words[index], '=');
            *eq = '\0';
            eo += (size_t)snprintf(envacc + eo, sizeof envacc - eo, "%s=%s;",
                                   words[index], eq + 1);
            index++;
            continue;
        }
        WCUP_BRANCH(0);

        char *exec = executable_name(words[index]);
        if (exec && strcmp(exec, "sudo") == 0) {
            WCUP_BRANCH(1);
            index = (size_t)consume_options(words, index + 1, SUDO_OPTS);
            free(exec);
            continue;
        }
        WCUP_BRANCH(0);
        if (exec && strcmp(exec, "env") == 0) {
            WCUP_BRANCH(1);
            index = (size_t)consume_options(words, index + 1, ENV_OPTS);
            free(exec);
            continue;
        }
        WCUP_BRANCH(0);
        if (exec && strcmp(exec, "command") == 0) {
            WCUP_BRANCH(1);
            if (words[index + 1] && (strcmp(words[index + 1], "-v") == 0 ||
                                     strcmp(words[index + 1], "-V") == 0)) {
                WCUP_BRANCH(1);
                free(exec);
                goto ret_none;
            }
            WCUP_BRANCH(0);
            index = (size_t)consume_options(words, index + 1, NULL);
            free(exec);
            continue;
        }
        WCUP_BRANCH(0);
        if (exec && str_in(SIMPLE_WRAPPERS, exec)) {
            WCUP_BRANCH(1);
            const char *const *opts = NULL;
            if (strcmp(exec, "exec") == 0) opts = EXEC_OPTS;
            else if (strcmp(exec, "time") == 0) opts = TIME_OPTS;
            index = (size_t)consume_options(words, index + 1, opts);
            free(exec);
            continue;
        }
        WCUP_BRANCH(0);

        /* final: env, words[index], rest */
        {
            char acc[8192]; size_t ao = 0;
            ao += (size_t)snprintf(acc + ao, sizeof acc - ao, "%s|%s|",
                                   envacc, words[index]);
            for (size_t k = index + 1; words[k]; k++)
                ao += (size_t)snprintf(acc + ao, sizeof acc - ao, "%s%s",
                                       words[k], words[k + 1] ? "," : "");
            WCUP_VALSTR(acc);
            char cup[32];
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("srg_command_parts");
            return;
        }
    }

ret_none:
    {
        char acc[8192];
        snprintf(acc, sizeof acc, "%s||", envacc);
        WCUP_VALSTR(acc);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("srg_command_parts");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srg_command_parts <word word ...>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        static char buf[8192];
        snprintf(buf, sizeof buf, "%s", argv[a]);
        char **words = calloc(MAXW + 1, sizeof(char *));
        int n = 0;
        if (*buf) {
            char *tok = strtok(buf, " ");
            while (tok && n < MAXW) { words[n++] = tok; tok = strtok(NULL, " "); }
        }
        command_parts(words);
        free(words);
    }
    return 0;
}
