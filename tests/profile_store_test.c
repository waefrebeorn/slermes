/*
 * profile_store_test.c — real-behavior test for port_cli_profiles.c
 *
 * Exercises the multi-profile HERMES_HOME management in a temp SLERMES_HOME
 * and asserts faithful behavior: name normalization/validation, path
 * resolution, sticky active-profile, profile.yaml round-trip, distribution
 * metadata, archive member path-safety, and env resolution.
 *
 * MIT License — Slermes Fork
 */

#include "profile_store.h"
#include "slermes_home.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

static void rmrf(const char *p) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", p);
    system(cmd);
}

/* Recursively create a directory (like mkdir -p). */
static bool mkdir_p_ok(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

int main(void) {
    char tmpl[] = "/tmp/profile_store_test.XXXXXX";
    char *home = mkdtemp(tmpl);
    if (!home) { printf("cannot make temp home\n"); return 2; }
    setenv("SLERMES_HOME", home, 1);
    /* Ensure the home resolves to our temp dir. */
    CHECK(strcmp(slermes_home(), home) == 0, "slermes_home == temp home");

    /* ── Path helpers ─────────────────────────────────────────────── */
    char *proot = profile_profiles_root();
    CHECK(proot && strcmp(proot, home) != 0 &&
          strcmp(proot + strlen(home), "/profiles") == 0,
          "profiles_root == <home>/profiles");
    free(proot);

    char *def = profile_default_home();
    CHECK(def && strcmp(def, home) == 0, "default_home == home");
    free(def);

    char *apath = profile_active_profile_path();
    CHECK(apath && strcmp(apath + strlen(home), "/active_profile") == 0,
          "active_profile_path == <home>/active_profile");
    free(apath);

    char *wd = profile_wrapper_dir();
    CHECK(wd != NULL, "wrapper_dir non-null");
    free(wd);

    /* ── Normalization ────────────────────────────────────────────── */
    char *n1 = profile_normalize_name("Work-Profile");
    CHECK(n1 && strcmp(n1, "work-profile") == 0, "normalize lowercases");
    free(n1);

    char *n2 = profile_normalize_name("DEFAULT");
    CHECK(n2 && strcmp(n2, "default") == 0, "normalize 'DEFAULT' -> 'default'");
    free(n2);

    char *n3 = profile_normalize_name(NULL);
    CHECK(n3 == NULL, "normalize NULL -> NULL");
    char *n4 = profile_normalize_name("   ");
    CHECK(n4 == NULL, "normalize blank -> NULL");

    /* ── Validation ───────────────────────────────────────────────── */
    char *e = NULL;
    CHECK(profile_validate_name("alpha1", &e) == true, "valid name alpha1");
    if (e) { free(e); e = NULL; }
    CHECK(profile_validate_name("default", &e) == true, "'default' is a valid alias");
    if (e) { free(e); e = NULL; }
    CHECK(profile_validate_name("Root", &e) == false, "'Root' reserved -> invalid");
    if (e) { free(e); e = NULL; }
    CHECK(profile_validate_name("bad name!", &e) == false, "space/bang -> invalid");
    if (e) { free(e); e = NULL; }

    CHECK(profile_validate_alias_name("myalias", &e) == true, "alias myalias valid");
    if (e) { free(e); e = NULL; }
    CHECK(profile_validate_alias_name("x", &e) == true, "single-char alias valid (regex min len 1)");
    if (e) { free(e); e = NULL; }

    /* ── Resolution ───────────────────────────────────────────────── */
    char *ddef = profile_dir_for("default");
    CHECK(ddef && strcmp(ddef, home) == 0, "dir_for('default') == home");
    free(ddef);

    char *dnamed = profile_dir_for("team-a");
    CHECK(dnamed && strcmp(dnamed + strlen(home), "/profiles/team-a") == 0,
          "dir_for('team-a') == <home>/profiles/team-a");
    free(dnamed);

    CHECK(profile_dir_exists("default") == 1, "default dir always exists");
    CHECK(profile_dir_exists("nonexistent") == 0, "nonexistent dir does not exist");

    /* ── Active profile (sticky) ──────────────────────────────────── */
    char *act = profile_get_active();
    CHECK(act && strcmp(act, "default") == 0, "get_active defaults to 'default'");
    free(act);

    /* create a real named profile dir so set_active('team-a') is allowed */
    char pdir[4096];
    snprintf(pdir, sizeof(pdir), "%s/profiles/team-a", home);
    CHECK(mkdir_p_ok(pdir), "created profiles/team-a");
    CHECK(profile_set_active("team-a") == true, "set_active('team-a') succeeds");
    act = profile_get_active();
    CHECK(act && strcmp(act, "team-a") == 0, "get_active now 'team-a'");
    free(act);

    CHECK(profile_set_active("default") == true, "set_active('default') clears");
    act = profile_get_active();
    CHECK(act && strcmp(act, "default") == 0, "get_active back to 'default'");
    free(act);

    /* ── Active profile name inference ────────────────────────────── */
    char *an = profile_get_active_name();
    CHECK(an && strcmp(an, "default") == 0, "active_name is 'default' at home");
    free(an);

    /* ── profile.yaml round-trip ──────────────────────────────────── */
    char pd[4096];
    snprintf(pd, sizeof(pd), "%s/profiles/team-a", home);
    CHECK(profile_write_profile_meta(pd, "Team A profile", true, false, false),
          "write_profile_meta succeeds");
    char *desc = NULL; bool auto_flag = false;
    profile_read_profile_meta(pd, &desc, &auto_flag);
    CHECK(desc && strcmp(desc, "Team A profile") == 0, "read back description");
    CHECK(auto_flag == false, "read back desc_auto=false");
    free(desc);

    /* ── distribution metadata ────────────────────────────────────── */
    char dist[4096];
    snprintf(dist, sizeof(dist), "%s/profiles/team-a/distribution.yaml", home);
    FILE *df = fopen(dist, "w");
    if (df) { fprintf(df, "name: TeamA\nversion: 1.2.3\nsource: git\n"); fclose(df); }
    char *dn = NULL, *dv = NULL, *ds = NULL;
    profile_read_distribution_meta(pd, &dn, &dv, &ds);
    CHECK(dn && strcmp(dn, "TeamA") == 0, "distribution name");
    CHECK(dv && strcmp(dv, "1.2.3") == 0, "distribution version");
    CHECK(ds && strcmp(ds, "git") == 0, "distribution source");
    free(dn); free(dv); free(ds);

    /* missing distribution.yaml -> all NULL */
    char *ndn = NULL, *ndv = NULL, *nds = NULL;
    char empty_dir[4096];
    snprintf(empty_dir, sizeof(empty_dir), "%s/profiles/empty", home);
    mkdir_p_ok(empty_dir);
    profile_read_distribution_meta(empty_dir, &ndn, &ndv, &nds);
    CHECK(ndn == NULL && ndv == NULL && nds == NULL, "missing distribution -> NULLs");
    if (ndn) free(ndn);
    if (ndv) free(ndv);
    if (nds) free(nds);

    /* ── Archive member path-safety ──────────────────────────────── */
    CHECK(profile_archive_member_safe("config/config.yaml") == true, "safe relative path");
    CHECK(profile_archive_member_safe("../escape") == false, "rejects '..'");
    CHECK(profile_archive_member_safe("/abs/path") == false, "rejects absolute");
    CHECK(profile_archive_member_safe("c:/windows") == false, "rejects drive letter");
    CHECK(profile_archive_member_safe("") == false, "rejects empty");
    CHECK(profile_archive_member_safe("a//b") == true, "collapses empty part (a//b ok)");

    /* ── Bundled-skills opt-out ──────────────────────────────────── */
    CHECK(profile_has_bundled_skills_opt_out(pd) == false, "no opt-out initially");
    char marker[4096];
    snprintf(marker, sizeof(marker), "%s/.no-bundled-skills", pd);
    FILE *mf = fopen(marker, "w"); if (mf) fclose(mf);
    CHECK(profile_has_bundled_skills_opt_out(pd) == true, "opt-out marker detected");
    unlink(marker);
    CHECK(profile_has_bundled_skills_opt_out(pd) == false, "opt-out cleared");

    /* ── Skills count (creates a tiny skills tree) ───────────────── */
    char skd[4096];
    snprintf(skd, sizeof(skd), "%s/skills/cat1", pd);
    mkdir_p_ok(skd);
    FILE *skf = fopen("/tmp/skillmd_XXXXXX", "w"); /* placeholder */
    if (skf) { char sp[4096]; snprintf(sp,sizeof(sp),"%s/skills/cat1/SKILL.md",pd);
               FILE *w=fopen(sp,"w"); if(w) fclose(w); fclose(skf); }
    int nsk = profile_count_skills(pd);
    CHECK(nsk == 1, "counted 1 SKILL.md");
    CHECK(profile_count_skills(pd) == 1, "skills count cached/steady");
    char *pysig = pd; /* signature on non-existent skills dir */
    CHECK(profile_skills_dir_signature("/no/such/dir") == 0.0, "signature missing dir -> 0");

    /* ── config.yaml model/provider ──────────────────────────────── */
    char cfg[4096];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", pd);
    FILE *cf = fopen(cfg, "w");
    if (cf) { fprintf(cf, "model:\n  default: claude-3\n  provider: anthropic\n"); fclose(cf); }
    char *cm = NULL, *cp = NULL;
    profile_read_config_model(pd, &cm, &cp);
    CHECK(cm && strcmp(cm, "claude-3") == 0, "config model.default read");
    CHECK(cp && strcmp(cp, "anthropic") == 0, "config model.provider read");
    free(cm); free(cp);

    /* ── profiles_to_serve ──────────────────────────────────────── */
    char *serve = profile_profiles_to_serve(false);
    CHECK(serve != NULL, "profiles_to_serve(false) non-null");
    if (serve) {
        char *cur = NULL; const char *nm=NULL, *hm=NULL;
        int cnt = 0; bool saw_default = false;
        while (profile_serve_next(serve, &cur, &nm, &hm)) {
            cnt++;
            if (strcmp(nm, "default") == 0) saw_default = true;
        }
        CHECK(cnt == 1 && saw_default, "serve(false) -> just active(default)");
        free(serve);
    }
    char *serve2 = profile_profiles_to_serve(true);
    if (serve2) {
        char *cur = NULL; const char *nm=NULL, *hm=NULL;
        int cnt = 0; bool saw_team = false;
        while (profile_serve_next(serve2, &cur, &nm, &hm)) {
            cnt++;
            if (strcmp(nm, "team-a") == 0) saw_team = true;
        }
        CHECK(cnt >= 2 && saw_team, "serve(true) -> default + team-a");
        free(serve2);
    }

    /* ── Clone / export ignore predicates ────────────────────────── */
    CHECK(profile_clone_ignore(home, home, "__pycache__") == true, "clone ignores __pycache__");
    CHECK(profile_clone_ignore(home, home, "sessions") == true, "clone ignores sessions (history)");
    CHECK(profile_clone_ignore(home, home, "mydata") == false, "clone keeps mydata");
    CHECK(profile_export_ignore(home, home, "logs") == true, "export ignores logs");
    CHECK(profile_export_ignore(home, home, "package-lock.json") == true, "export ignores lockfile");

    /* ── profile_yaml_path ───────────────────────────────────────── */
    char *yp = profile_yaml_path(pd);
    CHECK(yp && strcmp(yp + strlen(pd), "/profile.yaml") == 0, "yaml_path suffix");
    free(yp);

    /* ── Seed skills (single-home fail-open) ─────────────────────── */
    char *seed = profile_seed_profile_skills(pd, true);
    CHECK(seed && strstr(seed, "user_modified") != NULL, "seed returns result dict");
    free(seed);

    /* ── Archive inspect / safe-extract (round-trip) ─────────────── */
    /* Build a tiny tar.gz via the system tar for a deterministic check. */
    char arc[4096];
    snprintf(arc, sizeof(arc), "%s/test_profile_archive.tar.gz", home);
    char tarbuild[8192];
    snprintf(tarbuild, sizeof(tarbuild),
             "cd '%s' && mkdir -p _arcroot/sub && echo hi > _arcroot/file.txt && "
             "tar -czf '%s' _arcroot && rm -rf _arcroot", home, arc);
    if (system(tarbuild) == 0) {
        char *roots = profile_inspect_archive_roots(arc);
        CHECK(roots && strstr(roots, "arcroot") != NULL, "inspect found top dir");
        free(roots);
        char dest[4096];
        snprintf(dest, sizeof(dest), "%s/_extract_out", home);
        CHECK(profile_safe_extract_archive(arc, dest) == true, "safe extract succeeds");
        struct stat es;
        char extfile[8192];
        snprintf(extfile, sizeof(extfile), "%s/_arcroot/file.txt", dest);
        CHECK(stat(extfile, &es) == 0, "extracted file present");
        CHECK(profile_safe_extract_archive("/no/such.tar.gz", dest) == false,
              "safe extract rejects missing archive");
        rmrf(dest);
        unlink(arc);
    }

    /* ── Env resolution ──────────────────────────────────────────── */
    char *re = profile_resolve_env("default");
    CHECK(re && strcmp(re, home) == 0, "resolve_env('default') == home");
    free(re);
    char *ren = profile_resolve_env("team-a");
    CHECK(ren && strcmp(ren, pd) == 0, "resolve_env('team-a') == its dir");
    free(ren);
    char *rem = profile_resolve_env("ghost");
    CHECK(rem == NULL, "resolve_env of missing non-default -> NULL");

    /* ── Alias collision check ───────────────────────────────────── */
    char *col = profile_check_alias_collision("chat");
    CHECK(col != NULL, "alias 'chat' collides with subcommand");
    free(col);
    char *nocol = profile_check_alias_collision("teama-cli");
    CHECK(nocol == NULL, "alias 'teama-cli' is safe");
    if (nocol) free(nocol);

    printf("\nprofile_store_test: %d passed, %d failed\n", g_pass, g_fail);
    rmrf(home);
    return g_fail ? 1 : 0;
}
