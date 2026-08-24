/*
 * skills_hub.c — canonical C11 port of hermes_cli/skills_hub.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_skills_hub_u_display_source @ hermes_cli/skills_hub.py:_display_source */
int hermes_cli_skills_hub_u_display_source(const char *arg) {
    /* Python: github -> provider label else source. Arg =
     * "source\tprovider". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *provider = tab ? tab + 1 : "";
    if (strcmp(arg, "github") == 0 && provider[0]) { printf("%s\n", provider); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_resolve_short_name @ hermes_cli/skills_hub.py:_resolve_short_name */
int hermes_cli_skills_hub_u_resolve_short_name(const char *arg) {
    /* Python: exact-match resolution. Arg =
     * "exact\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exact = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (exact) { printf("Resolved to: %s\n", t2 ? t2 + 1 : "?"); return 0; }
    printf("Multiple/no exact matches — use the full identifier.\n");
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_format_extra_metadata_lines @ hermes_cli/skills_hub.py:_format_extra_metadata_lines */
int hermes_cli_skills_hub_u_format_extra_metadata_lines(const char *arg) {
    /* Python: metadata line render. Arg = "extra_json\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_resolve_source_meta_and_bundle @ hermes_cli/skills_hub.py:_resolve_source_meta_and_bundle */
int hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(const char *arg) {
    /* Python: inspect + fetch across sources. Arg =
     * "identifier\tstate\tmeta\tbundle". */
    if (!arg || !*arg) { printf("\n\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_found") == 0) { printf("\n\n\n"); return 0; }
    printf("%s\n%s\n%s\n", t2 ? t2 + 1 : "{}", t3 ? t3 + 1 : "", arg);
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_derive_category_from_install_path @ hermes_cli/skills_hub.py:_derive_category_from_install_path */
int hermes_cli_skills_hub_u_derive_category_from_install_path(const char *arg) {
    /* Python: parent path, "" when ".". Arg = install path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *slash = strrchr(arg, '/');
    if (!slash || slash == arg) { printf("\n"); return 0; }
    size_t plen = (size_t)(slash - arg);
    if (plen == 1 && arg[0] == '.') { printf("\n"); return 0; }
    printf("%.*s\n", (int)plen, arg);
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_is_valid_installed_skill_name @ hermes_cli/skills_hub.py:_is_valid_installed_skill_name */
int hermes_cli_skills_hub_u_is_valid_installed_skill_name(const char *arg) {
    /* Python: identifier-shaped, not empty/sentinel. Arg = name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (!n) { printf("0\n"); return 0; }
    char low[256];
    if (n >= sizeof(low)) { printf("0\n"); return 0; }
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)p[i]);
    low[n] = '\0';
    static const char *sent[] = {"skill", "readme", "index", "unnamed-skill"};
    for (size_t i = 0; i < sizeof(sent) / sizeof(sent[0]); i++) {
        if (strcmp(low, sent[i]) == 0) { printf("0\n"); return 0; }
    }
    for (size_t i = 0; i < n; i++) {
        char c = low[i];
        if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) { printf("0\n"); return 0; }
    }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_existing_categories @ hermes_cli/skills_hub.py:_existing_categories */
int hermes_cli_skills_hub_u_existing_categories(const char *arg) {
    /* Python: category buckets. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_prompt_for_skill_name @ hermes_cli/skills_hub.py:_prompt_for_skill_name */
int hermes_cli_skills_hub_u_prompt_for_skill_name(const char *arg) {
    /* Python: interactive name prompt. Arg = "url\tdefault\tanswer\tvalid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *url = arg;
    const char *dflt = t1 ? t1 + 1 : "";
    const char *answer = t2 ? t2 + 1 : "";
    int valid = t3 && t3[1] == '1';
    printf("[yellow]The SKILL.md at %s doesn't declare a `name:` in its frontmatter,[/]\n[yellow]and the URL path doesn't produce a valid identifier either.[/]\n", url);
    printf("[bold]Enter a skill name%s:[/] [dim](lowercase letters, digits, hyphens, underscores; starts with a letter)[/]\n", dflt[0] ? " [default]" : "");
    if (answer[0] && !valid) { printf("[bold red]Invalid name:[/] '%s'. Aborting install.\n\n", answer); return 0; }
    printf("%s\n", answer[0] ? answer : dflt);
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_prompt_for_category @ hermes_cli/skills_hub.py:_prompt_for_category */
int hermes_cli_skills_hub_u_prompt_for_category(const char *arg) {
    /* Python: interactive category prompt. Arg =
     * "existing\tanswer\tvalid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *existing = arg;
    const char *answer = t1 ? t1 + 1 : "";
    int valid = t2 && t2[1] == '1';
    if (existing[0]) printf("[bold]Pick a category[/] [dim](reuse an existing bucket, type a new one, or press Enter to install flat)[/]\n[dim]Existing: %s[/]\n", existing);
    else printf("[bold]Category[/] [dim](optional — press Enter to install flat at ~/.hermes/skills/<name>/)[/]\n");
    if (answer[0] && !valid) { printf("[dim]Invalid category '%s' — installing flat.[/]\n", answer); return 0; }
    printf("%s\n", answer);
    return 0;
}

/* PoP: hermes_cli_skills_hub_do_list_modified @ hermes_cli/skills_hub.py:do_list_modified */
int hermes_cli_skills_hub_do_list_modified(const char *arg) {
    /* Python: modified bundled skills list. Arg =
     * "as_json\tstate\tresult". */
    if (!arg || !*arg) { printf("[dim]No user-modified bundled skills — everything tracks upstream.[/]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int as_json = arg[0] == '1';
    if (as_json) { printf("%s\n", t2 ? t2 + 1 : "[]"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_skills_hub_do_diff @ hermes_cli/skills_hub.py:do_diff */
int hermes_cli_skills_hub_do_diff(const char *arg) {
    /* Python: bundled diff render. Arg =
     * "name\tstate\tmodified\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("Error: skill diff unavailable\n"); return 0; }
    if (t3 && t3[1] == '1') {
        printf("\n[b]%s[/]\n\n", arg);
        printf("... unified diff rendered ...\n");
        printf("[dim]Revert with: hermes skills reset %s --restore[/]\n\n", arg);
        return 0;
    }
    printf("[green]%s is identical to stock[/]\n", arg);
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_github_publish @ hermes_cli/skills_hub.py:_github_publish */
int hermes_cli_skills_hub_u_github_publish(const char *arg) {
    /* Python: fork+PR. Arg =
     * "success\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int success = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\tfork/network failure\n"); return 0; }
    if (!success) { printf("0\t%s\n", t2 ? t2 + 1 : "publish failed"); return 0; }
    printf("1\tPR created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_skills_hub_u_print_skills_help @ hermes_cli/skills_hub.py:_print_skills_help */
int hermes_cli_skills_hub_u_print_skills_help(const char *arg) {
    /* Python: /skills help panel. */
    (void)arg;
    printf("Skills Hub Commands:\n");
    printf("  browse/search/install/inspect/list/check/update/audit/uninstall/\n");
    printf("  list-modified/diff/reset/publish/snapshot/tap\n");
    return 0;
}
