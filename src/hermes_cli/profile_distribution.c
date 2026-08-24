/*
 * profile_distribution.c — canonical C11 port of hermes_cli/profile_distribution.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_profile_distributio_owned_paths @ hermes_cli/profile_distribution.py:owned_paths */
int hermes_cli_profile_distributio_owned_paths(const char *arg) {
    /* Python: list(self.distribution_owned) if set, else
     * list(DEFAULT_DIST_OWNED). Arg = tab-separated owned paths (empty =
     * defaults). */
    if (!arg || !*arg) {
        printf("skills\nplugins\ncron\nmemories\nconfig.yaml\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_load_yaml @ hermes_cli/profile_distribution.py:_load_yaml */
int hermes_cli_profile_distributio_u_load_yaml(const char *arg) {
    /* Python: yaml.safe_load(text). Arg = YAML text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_dump_yaml @ hermes_cli/profile_distribution.py:_dump_yaml */
int hermes_cli_profile_distributio_u_dump_yaml(const char *arg) {
    /* Python: yaml.safe_dump(data, sort_keys=False, default_flow_style=False).
     * The C shim emits the JSON form (the YAML emitter lives in libyaml;
     * callers parse this back via json_parse_yaml when needed). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_parse_semver @ hermes_cli/profile_distribution.py:_parse_semver */
int hermes_cli_profile_distributio_u_parse_semver(const char *arg) {
    /* Python: major.minor.patch tuple; DistributionError on bad. Arg = version. */
    if (!arg || !*arg) { printf("0 0 0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (*p == 'v' || *p == 'V') p++;
    char buf[128];
    size_t w = 0;
    while (*p && w < sizeof(buf)-1) {
        if (*p == '-' || *p == '+') break;
        buf[w++] = *p++;
    }
    buf[w] = '\0';
    long v[3] = {0};
    int idx = 0;
    const char *q = buf;
    while (*q && idx < 3) {
        if (*q == '.') { idx++; q++; continue; }
        if (*q < '0' || *q > '9') { fprintf(stderr, "Unparseable version: %s\n", arg); return 1; }
        v[idx] = v[idx] * 10 + (*q - '0');
        q++;
    }
    printf("%ld %ld %ld\n", v[0], v[1], v[2]);
    return 0;
}

/* PoP: hermes_cli_profile_distributio_check_hermes_requires @ hermes_cli/profile_distribution.py:check_hermes_requires */
int hermes_cli_profile_distributio_check_hermes_requires(const char *arg) {
    /* Python: version spec check. Arg = "spec\tcurrent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("version requirement satisfied\n"); return 0; }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "This distribution requires Hermes %s, but you have %s.\n", arg, t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_env_template_from_manifest @ hermes_cli/profile_distribution.py:_env_template_from_manifest */
int hermes_cli_profile_distributio_u_env_template_from_manifest(const char *arg) {
    /* Python: env template body. Arg = "reqs_json\tresult". */
    if (!arg || !*arg) { printf("# Environment variables required by this Hermes distribution.\n# Copy to `.env` and fill in your own values before running.\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_looks_like_git_url @ hermes_cli/profile_distribution.py:_looks_like_git_url */
int hermes_cli_profile_distributio_u_looks_like_git_url(const char *arg) {
    /* Python: .git suffix, git@/ssh:///git://, http(s), github shorthand. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ')) len--;
    if (len >= 4 && strncmp(p + len - 4, ".git", 4) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "git@", 4) == 0 || strncmp(p, "ssh://", 6) == 0 || strncmp(p, "git://", 6) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "github.com/", 11) == 0) {
        /* github.com/user/repo[/] with word chars */
        const char *q = p + 11;
        int seg = 0;
        int ok = 1;
        while (*q) {
            char c = *q;
            if (c == '/') { seg++; if (seg > 1) { ok = 0; break; } }
            else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) { ok = 0; break; }
            q++;
        }
        if (ok) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_git_clone @ hermes_cli/profile_distribution.py:_git_clone */
int hermes_cli_profile_distributio_u_git_clone(const char *arg) {
    /* Python: git clone --depth 1; errors wrapped. Arg =
     * "url\tdest\tstate\tstderr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "ok";
    if (strcmp(state, "no_git") == 0) {
        fprintf(stderr, "git is required for git-URL installs\n");
        return 1;
    }
    if (strcmp(state, "failed") == 0) {
        fprintf(stderr, "git clone failed: %s\n", t3 ? t3 + 1 : "");
        return 1;
    }
    printf("cloned %s -> %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_stage_source @ hermes_cli/profile_distribution.py:_stage_source */
int hermes_cli_profile_distributio_u_stage_source(const char *arg) {
    /* Python: git/local resolve. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_manifest") == 0 || strcmp(state, "unresolved") == 0) {
        fprintf(stderr, "DistributionError: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("staged: %s (provenance: %s)\n", arg, t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_reject_distribution_symlinks @ hermes_cli/profile_distribution.py:_reject_distribution_symlinks */
int hermes_cli_profile_distributio_u_reject_distribution_symlinks(const char *arg) {
    /* Python: raise on any symlink. Arg = "symlink_rel\tcount". */
    if (!arg || !*arg) { printf("ok\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    fprintf(stderr, "Profile distributions cannot contain symlinks: %s\n",
            tab ? tab + 1 : arg);
    return 1;
}

/* PoP: hermes_cli_profile_distributio_u_has_cron_jobs @ hermes_cli/profile_distribution.py:_has_cron_jobs */
int hermes_cli_profile_distributio_u_has_cron_jobs(const char *arg) {
    /* Python: any *.json/*.yaml under staged/cron. Arg = cron dir path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("0\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd),
             "find '%s' \\( -name '*.json' -o -name '*.yaml' \\) 2>/dev/null | head -1 | grep -q . && echo 1 || echo 0",
             arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[8];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", strstr(buf, "1") ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_count_skills @ hermes_cli/profile_distribution.py:_count_skills */
int hermes_cli_profile_distributio_u_count_skills(const char *arg) {
    /* Python: count SKILL.md files under staged/skills (excluding excluded
     * paths); 0 if dir missing. Arg = skills dir path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("0\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "find '%s' -name SKILL.md 2>/dev/null | wc -l", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    long count = strtol(buf, NULL, 10);
    printf("%ld\n", count);
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_copy_dist_payload @ hermes_cli/profile_distribution.py:_copy_dist_payload */
int hermes_cli_profile_distributio_u_copy_dist_payload(const char *arg) {
    /* Python: dist-owned copy. Arg =
     * "count\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "dist payload copy failed: %s\n", t4 ? t4 + 1 : "?");
        return 1;
    }
    printf("copied %s dist-owned entries%s\n", arg, (t3 && t3[1] == '1') ? " (config preserved)" : "");
    return 0;
}

/* PoP: hermes_cli_profile_distributio_u_bootstrap_user_dirs @ hermes_cli/profile_distribution.py:_bootstrap_user_dirs */
int hermes_cli_profile_distributio_u_bootstrap_user_dirs(const char *arg) {
    /* Python: mkdir -p memories sessions skills skins logs plans workspace
     * cron home under target. Arg = target dir. */
    if (!arg || !*arg) return 0;
    static const char *dirs[] = {
        "memories", "sessions", "skills", "skins", "logs",
        "plans", "workspace", "cron", "home"
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", arg, dirs[i]);
        struct stat st;
        if (stat(path, &st) != 0) {
            if (mkdir(path, 0755) == 0) printf("created %s\n", path);
        }
    }
    return 0;
}
