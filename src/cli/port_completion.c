/*
 * port_completion.c — pure shell-completion script generation.
 *
 * Faithful C port of hermes_cli/completion.py (generate_bash/zsh/fish + _clean),
 * operating on a caller-built completion_node tree instead of argparse.
 *
 * PoP: _clean         @ hermes_cli/completion.py:_clean
 * PoP: generate_bash  @ hermes_cli/completion.py:generate_bash
 * PoP: generate_zsh   @ hermes_cli/completion.py:generate_zsh
 * PoP: generate_fish  @ hermes_cli/completion.py:generate_fish
 */

#include "completion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── node lifecycle ───────────────────────────────────────────────── */

completion_node_t *completion_node_new(const char *name, const char *help,
                                        char **flags, completion_node_t **subcommands) {
    completion_node_t *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->name = strdup(name ? name : "");
    n->help = strdup(help ? help : "");
    n->flags = flags;                 /* adopted */
    n->subcommands = subcommands;     /* adopted */
    return n;
}

void completion_node_free(completion_node_t *node) {
    if (!node) return;
    free(node->name);
    free(node->help);
    if (node->flags) {
        for (size_t i = 0; node->flags[i]; i++) free(node->flags[i]);
        free(node->flags);
    }
    if (node->subcommands) {
        for (size_t i = 0; node->subcommands[i]; i++) completion_node_free(node->subcommands[i]);
        free(node->subcommands);
    }
    free(node);
}

/* ── _clean ───────────────────────────────────────────────────────── */

/* PoP: completion_clean @ hermes_cli.completion.py:_clean */
char *completion_clean(const char *text, size_t maxlen) {
    if (!text) text = "";
    /* strip ' " \ then take first maxlen chars */
    size_t cap = strlen(text) + 1;
    char *out = malloc(cap);
    size_t j = 0;
    for (size_t i = 0; text[i] && j < maxlen; i++) {
        char c = text[i];
        if (c == '\'' || c == '"' || c == '\\') continue;
        out[j++] = c;
    }
    out[j] = '\0';
    return out;
}

/* ── helpers ──────────────────────────────────────────────────────── */

/* count + NULL-terminated iteration helpers */
static size_t node_count(completion_node_t *const *arr) {
    size_t n = 0;
    if (arr) while (arr[n]) n++;
    return n;
}

/* Insertion-sort nodes by name (ascending), in place. */
static void sort_nodes(completion_node_t **arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        completion_node_t *key = arr[i];
        size_t j = i;
        while (j > 0 && strcmp(arr[j - 1]->name, key->name) > 0) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

/* dynamic string buffer */
typedef struct { char *buf; size_t len; size_t cap; } sbuf_t;

static void sbuf_init(sbuf_t *s) { s->buf = NULL; s->len = 0; s->cap = 0; }
static void sbuf_free(sbuf_t *s) { free(s->buf); s->buf = NULL; s->len = s->cap = 0; }
static void sbuf_append(sbuf_t *s, const char *str) {
    if (!str) return;
    size_t add = strlen(str);
    if (s->len + add + 1 > s->cap) {
        size_t ncap = (s->cap ? s->cap * 2 : 64);
        while (ncap < s->len + add + 1) ncap *= 2;
        s->buf = realloc(s->buf, ncap);
        s->cap = ncap;
    }
    memcpy(s->buf + s->len, str, add);
    s->len += add;
    s->buf[s->len] = '\0';
}

/* Build "a b c" from a node list (sorted). Caller must have sorted, or we
 * sort a local copy. Returns malloc'd string. */
static char *join_sorted_names(completion_node_t *const *arr) {
    size_t n = node_count(arr);
    completion_node_t **copy = NULL;
    if (n) {
        copy = malloc(n * sizeof(*copy));
        for (size_t i = 0; i < n; i++) copy[i] = arr[i];
        sort_nodes(copy, n);
    }
    sbuf_t s; sbuf_init(&s);
    for (size_t i = 0; i < n; i++) {
        if (i) sbuf_append(&s, " ");
        sbuf_append(&s, copy[i]->name);
    }
    free(copy);
    char *r = s.buf ? s.buf : strdup("");
    return r;
}

/* ── generate_bash ────────────────────────────────────────────────── */

char *completion_generate_bash(completion_node_t *const *tree) {
    sbuf_t s; sbuf_init(&s);

    /* sort a working copy of top-level */
    size_t n = node_count(tree);
    completion_node_t **top = NULL;
    if (n) { top = malloc(n * sizeof(*top)); for (size_t i=0;i<n;i++) top[i]=tree[i]; sort_nodes(top,n); }

    char *top_cmds = join_sorted_names(tree);

    /* cases */
    sbuf_t cases; sbuf_init(&cases);
    for (size_t i = 0; i < n; i++) {
        completion_node_t *info = top[i];
        if (strcmp(info->name, "profile") == 0 && node_count(info->subcommands) > 0) {
            char *subcmds = join_sorted_names(info->subcommands);
            sbuf_append(&cases,
                "        profile)\n"
                "            case \"$prev\" in\n"
                "                profile)\n"
                "                    COMPREPLY=($(compgen -W \"");
            sbuf_append(&cases, subcmds);
            sbuf_append(&cases,
                "\" -- \"$cur\"))\n"
                "                    return\n"
                "                    ;;\n"
                "                use|delete|show|alias|rename|export)\n"
                "                    COMPREPLY=($(compgen -W \"$(_hermes_profiles)\" -- \"$cur\"))\n"
                "                    return\n"
                "                    ;;\n"
                "            esac\n"
                "            ;;\n");
            free(subcmds);
        } else if (node_count(info->subcommands) > 0) {
            char *subcmds = join_sorted_names(info->subcommands);
            sbuf_append(&cases, "        ");
            sbuf_append(&cases, info->name);
            sbuf_append(&cases,
                ")\n"
                "            COMPREPLY=($(compgen -W \"");
            sbuf_append(&cases, subcmds);
            sbuf_append(&cases,
                "\" -- \"$cur\"))\n"
                "            return\n"
                "            ;;\n");
            free(subcmds);
        } else if (info->flags && info->flags[0]) {
            /* join flags with spaces */
            sbuf_t fl; sbuf_init(&fl);
            for (size_t k = 0; info->flags[k]; k++) {
                if (k) sbuf_append(&fl, " ");
                sbuf_append(&fl, info->flags[k]);
            }
            sbuf_append(&cases, "        ");
            sbuf_append(&cases, info->name);
            sbuf_append(&cases,
                ")\n"
                "            COMPREPLY=($(compgen -W \"");
            sbuf_append(&cases, fl.buf ? fl.buf : "");
            sbuf_append(&cases,
                "\" -- \"$cur\"))\n"
                "            return\n"
                "            ;;\n");
            sbuf_free(&fl);
        }
    }

    /* header + body (mirrors Python f-string layout) */
    sbuf_append(&s,
        "# Hermes Agent bash completion\n"
        "# Add to ~/.bashrc:\n"
        "#   eval \"$(hermes completion bash)\"\n"
        "\n"
        "_hermes_profiles() {\n"
        "    local profiles_dir=\"$HOME/.hermes/profiles\"\n"
        "    local profiles=\"default\"\n"
        "    if [ -d \"$profiles_dir\" ]; then\n"
        "        for f in \"$profiles_dir\"/*/; do\n"
        "            [ -d \"$f\" ] && profiles=\"$profiles $(basename \"$f\")\"\n"
        "        done\n"
        "    fi\n"
        "    echo \"$profiles\"\n"
        "}\n"
        "\n"
        "_hermes_completion() {\n"
        "    local cur prev\n"
        "    COMPREPLY=()\n"
        "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
        "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
        "\n"
        "    # Complete profile names after -p / --profile\n"
        "    if [[ \"$prev\" == \"-p\" || \"$prev\" == \"--profile\" ]]; then\n"
        "        COMPREPLY=($(compgen -W \"$(_hermes_profiles)\" -- \"$cur\"))\n"
        "        return\n"
        "    fi\n"
        "\n"
        "    if [[ $COMP_CWORD -ge 2 ]]; then\n"
        "        case \"${COMP_WORDS[1]}\" in\n");
    sbuf_append(&s, cases.buf ? cases.buf : "");
    sbuf_append(&s,
        "        esac\n"
        "    fi\n"
        "\n"
        "    if [[ $COMP_CWORD -eq 1 ]]; then\n"
        "        COMPREPLY=($(compgen -W \"");
    sbuf_append(&s, top_cmds);
    sbuf_append(&s,
        "\" -- \"$cur\"))\n"
        "    fi\n"
        "}\n"
        "\n"
        "complete -F _hermes_completion hermes\n");

    sbuf_free(&cases);
    free(top_cmds);
    free(top);
    char *r = s.buf ? s.buf : strdup("");
    return r;
}

/* ── generate_zsh ─────────────────────────────────────────────────── */

char *completion_generate_zsh(completion_node_t *const *tree) {
    sbuf_t s; sbuf_init(&s);
    size_t n = node_count(tree);
    completion_node_t **top = NULL;
    if (n) { top = malloc(n*sizeof(*top)); for (size_t i=0;i<n;i++) top[i]=tree[i]; sort_nodes(top,n); }

    /* top_cmds_lines */
    sbuf_t tops; sbuf_init(&tops);
    for (size_t i = 0; i < n; i++) {
        char *h = completion_clean(top[i]->help, 60);
        sbuf_append(&tops, "                '");
        sbuf_append(&tops, top[i]->name);
        sbuf_append(&tops, ":");
        sbuf_append(&tops, h);
        sbuf_append(&tops, "'\n");
        free(h);
    }

    /* sub_cases */
    sbuf_t subc; sbuf_init(&subc);
    for (size_t i = 0; i < n; i++) {
        completion_node_t *info = top[i];
        if (!info->subcommands || !info->subcommands[0]) continue;
        if (strcmp(info->name, "profile") == 0) {
            sbuf_t subl; sbuf_init(&subl);
            size_t m = node_count(info->subcommands);
            completion_node_t **mc = malloc(m*sizeof(*mc)); for (size_t k=0;k<m;k++) mc[k]=info->subcommands[k]; sort_nodes(mc,m);
            for (size_t k = 0; k < m; k++) {
                char *h = completion_clean(mc[k]->help, 60);
                sbuf_append(&subl, "                        '");
                sbuf_append(&subl, mc[k]->name);
                sbuf_append(&subl, ":");
                sbuf_append(&subl, h);
                sbuf_append(&subl, "'\n");
                free(h);
            }
            free(mc);
            sbuf_append(&subc,
                "                profile)\n"
                "                    case ${line[2]} in\n"
                "                        use|delete|show|alias|rename|export)\n"
                "                            _hermes_profiles\n"
                "                            ;;\n"
                "                        *)\n"
                "                            local -a profile_cmds\n"
                "                            profile_cmds=(\n");
            sbuf_append(&subc, subl.buf ? subl.buf : "");
            sbuf_append(&subc,
                "                            )\n"
                "                            _describe 'profile command' profile_cmds\n"
                "                            ;;\n"
                "                    esac\n"
                "                    ;;\n");
            sbuf_free(&subl);
        } else {
            sbuf_t subl; sbuf_init(&subl);
            size_t m = node_count(info->subcommands);
            completion_node_t **mc = malloc(m*sizeof(*mc)); for (size_t k=0;k<m;k++) mc[k]=info->subcommands[k]; sort_nodes(mc,m);
            for (size_t k = 0; k < m; k++) {
                char *h = completion_clean(mc[k]->help, 60);
                sbuf_append(&subl, "                    '");
                sbuf_append(&subl, mc[k]->name);
                sbuf_append(&subl, ":");
                sbuf_append(&subl, h);
                sbuf_append(&subl, "'\n");
                free(h);
            }
            free(mc);
            char safe[256];
            /* cmd with '-' -> '_' */
            size_t li = 0;
            for (size_t k = 0; info->name[k] && li < sizeof(safe)-1; k++)
                safe[li++] = (info->name[k]=='-') ? '_' : info->name[k];
            safe[li] = '\0';
            sbuf_append(&subc, "                ");
            sbuf_append(&subc, info->name);
            sbuf_append(&subc,
                ")\n"
                "                    local -a ");
            sbuf_append(&subc, safe);
            sbuf_append(&subc,
                "_cmds\n"
                "                    ");
            sbuf_append(&subc, safe);
            sbuf_append(&subc, "_cmds=(\n");
            sbuf_append(&subc, subl.buf ? subl.buf : "");
            sbuf_append(&subc,
                "                    )\n"
                "                    _describe '");
            sbuf_append(&subc, info->name);
            sbuf_append(&subc,
                " command' ");
            sbuf_append(&subc, safe);
            sbuf_append(&subc,
                "_cmds\n"
                "                    ;;\n");
            sbuf_free(&subl);
        }
    }

    sbuf_append(&s,
        "#compdef hermes\n"
        "# Hermes Agent zsh completion\n"
        "# Add to ~/.zshrc:\n"
        "#   eval \"$(hermes completion zsh)\"\n"
        "\n"
        "_hermes_profiles() {\n"
        "    local -a profiles\n"
        "    profiles=(default)\n"
        "    if [[ -d \"$HOME/.hermes/profiles\" ]]; then\n"
        "        profiles+=($HOME/.hermes/profiles/*(N/:t))\n"
        "    fi\n"
        "    _describe 'profile' profiles\n"
        "}\n"
        "\n"
        "_hermes() {\n"
        "    local context state line\n"
        "    typeset -A opt_args\n"
        "\n"
        "    _arguments -C \\\n"
        "        '(-)'{-h,--help}'[Show help and exit]' \\\n"
        "        '(-)'{-V,--version}'[Show version and exit]' \\\n"
        "        '(-)'{-p,--profile}'[Profile name]:profile:_hermes_profiles' \\\n"
        "        '1:command:->commands' \\\n"
        "        '*::arg:->args'\n"
        "\n"
        "    case $state in\n"
        "        commands)\n"
        "            local -a subcmds\n"
        "            subcmds=(\n");
    sbuf_append(&s, tops.buf ? tops.buf : "");
    sbuf_append(&s,
        "            )\n"
        "            _describe 'hermes command' subcmds\n"
        "            ;;\n"
        "        args)\n"
        "            case ${line[1]} in\n");
    sbuf_append(&s, subc.buf ? subc.buf : "");
    sbuf_append(&s,
        "            esac\n"
        "            ;;\n"
        "    esac\n"
        "}\n"
        "\n"
        "compdef _hermes hermes\n");

    sbuf_free(&tops);
    sbuf_free(&subc);
    free(top);
    char *r = s.buf ? s.buf : strdup("");
    return r;
}

/* ── generate_fish ────────────────────────────────────────────────── */

char *completion_generate_fish(completion_node_t *const *tree) {
    sbuf_t s; sbuf_init(&s);
    size_t n = node_count(tree);
    completion_node_t **top = NULL;
    if (n) { top = malloc(n*sizeof(*top)); for (size_t i=0;i<n;i++) top[i]=tree[i]; sort_nodes(top,n); }

    char *top_cmds_str = join_sorted_names(tree);

    sbuf_append(&s,
        "# Hermes Agent fish completion\n"
        "# Add to your config:\n"
        "#   hermes completion fish | source\n"
        "\n"
        "# Helper: list available profiles\n"
        "function __hermes_profiles\n"
        "    echo default\n"
        "    if test -d $HOME/.hermes/profiles\n"
        "        for d in $HOME/.hermes/profiles/*/\n"
        "            basename $d\n"
        "        end\n"
        "    end\n"
        "end\n"
        "\n"
        "# Disable file completion by default\n"
        "complete -c hermes -f\n"
        "\n"
        "# Complete profile names after -p / --profile\n"
        "complete -c hermes -f -s p -l profile"
        " -d 'Profile name' -xa '(__hermes_profiles)'\n"
        "\n"
        "# Top-level subcommands\n");

    for (size_t i = 0; i < n; i++) {
        char *h = completion_clean(top[i]->help, 60);
        sbuf_append(&s, "complete -c hermes -f -n 'not __fish_seen_subcommand_from ");
        sbuf_append(&s, top_cmds_str);
        sbuf_append(&s, "' -a ");
        sbuf_append(&s, top[i]->name);
        sbuf_append(&s, " -d '");
        sbuf_append(&s, h);
        sbuf_append(&s, "'\n");
        free(h);
    }

    sbuf_append(&s, "\n# Subcommand completions\n");

    /* profile_name_actions set */
    const char *profile_actions[] = {"use","delete","show","alias","rename","export",NULL};

    for (size_t i = 0; i < n; i++) {
        completion_node_t *info = top[i];
        if (!info->subcommands || !info->subcommands[0]) continue;
        sbuf_append(&s, "# ");
        sbuf_append(&s, info->name);
        sbuf_append(&s, "\n");
        size_t m = node_count(info->subcommands);
        completion_node_t **mc = malloc(m*sizeof(*mc)); for (size_t k=0;k<m;k++) mc[k]=info->subcommands[k]; sort_nodes(mc,m);
        for (size_t k = 0; k < m; k++) {
            char *h = completion_clean(mc[k]->help, 60);
            sbuf_append(&s, "complete -c hermes -f -n '__fish_seen_subcommand_from ");
            sbuf_append(&s, info->name);
            sbuf_append(&s, "' -a ");
            sbuf_append(&s, mc[k]->name);
            sbuf_append(&s, " -d '");
            sbuf_append(&s, h);
            sbuf_append(&s, "'\n");
            free(h);
        }
        free(mc);
        if (strcmp(info->name, "profile") == 0) {
            for (size_t a = 0; profile_actions[a]; a++) {
                sbuf_append(&s, "complete -c hermes -f -n '__fish_seen_subcommand_from ");
                sbuf_append(&s, profile_actions[a]);
                sbuf_append(&s, "; and __fish_seen_subcommand_from profile' -a '(__hermes_profiles)' -d 'Profile name'\n");
            }
        }
    }

    free(top_cmds_str);
    free(top);
    char *r = s.buf ? s.buf : strdup("");
    return r;
}
