/*
 * test_verification_evidence.c — Faithful port of
 * agent/verification_evidence.py command-classification core + ledger.
 */

#include "verification_evidence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_fail = 0;
#define TEST(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

static void free_strv(char **v, int n) { for (int i=0;i<n;i++) free(v[i]); free(v); }

int main(void) {
    /* verify_clean_token */
    { char *c = verify_clean_token("./run.sh"); TEST(strcmp(c, "run.sh")==0, "clean_token strips ./"); free(c); }
    { char *c = verify_clean_token("node"); TEST(strcmp(c, "node")==0, "clean_token idempotent"); free(c); }

    /* verify_canonical_tokens */
    { int n; char **t = verify_canonical_tokens("npm run test", &n);
      TEST(n==3 && strcmp(t[0],"npm")==0 && strcmp(t[1],"run")==0 && strcmp(t[2],"test")==0, "canonical_tokens");
      free_strv(t,n); }

    /* verify_find_subsequence */
    { char *toks[] = {"x","y","z"}; char *ndl[] = {"y","z"};
      TEST(verify_find_subsequence(toks,3,ndl,2)==1, "subsequence found at 1"); }
    { char *toks[] = {"a","b"}; char *ndl[] = {"x"};
      TEST(verify_find_subsequence(toks,2,ndl,1)==-1, "subsequence not found"); }

    /* verify_strip_command_prefix */
    { char *toks[] = {"env","FOO=1","pytest"}; int n; char **r = verify_strip_command_prefix(toks,3,&n);
      TEST(n==1 && strcmp(r[0],"pytest")==0, "strip env + VAR="); }

    /* verify_equivalent_needles: npm run X -> [npm, X] */
    { char *ndl[] = {"npm","run","test"}; int n; int *l; char ***eq = verify_equivalent_needles(ndl,3,&n,&l);
      int found=0;
      for (int i=0;i<n;i++) if (eq[i][0] && strcmp(eq[i][0],"npm")==0 && eq[i][1] && strcmp(eq[i][1],"test")==0 && eq[i][2]==NULL) found=1;
      TEST(found, "equivalent_needles npm run test -> [npm,test]");
      for (int i=0;i<n;i++) { free_strv(eq[i],l[i]); } free(eq); free(l); }

    /* verify_find_canonical_match: `npm run test` matches canonical "npm run test" */
    { char *cmds[] = {"npm run test"};
      verify_match_t *m = verify_find_canonical_match("npm run test", cmds, 1);
      TEST(m && strcmp(m->canonical,"npm run test")==0 && m->trailing_n==0, "canonical match npm run test");
      if (m) { free(m->canonical); free_strv(m->trailing, m->trailing_n); free(m); } }

    /* `FOO=1 npm run test foo.py` matches with trailing [foo.py] */
    { char *cmds[] = {"npm run test"};
      verify_match_t *m = verify_find_canonical_match("FOO=1 npm run test foo.py", cmds, 1);
      TEST(m && m->trailing_n==1 && strcmp(m->trailing[0],"foo.py")==0, "canonical match with trailing arg");
      if (m) { free(m->canonical); free_strv(m->trailing, m->trailing_n); free(m); } }

    /* `python -m pytest` matches canonical "pytest" */
    { char *cmds[] = {"pytest"};
      verify_match_t *m = verify_find_canonical_match("python -m pytest", cmds, 1);
      TEST(m != NULL, "pytest equivalent: python -m pytest");
      if (m) { free(m->canonical); free_strv(m->trailing, m->trailing_n); free(m); } }

    /* verify_kind_for_command */
    TEST(verify_kind_for_command("npm run lint")==VERIFY_KIND_LINT, "kind lint");
    TEST(verify_kind_for_command("tsc")==VERIFY_KIND_TYPECHECK, "kind typecheck");
    TEST(verify_kind_for_command("npm run build")==VERIFY_KIND_BUILD, "kind build");
    TEST(verify_kind_for_command("black .")==VERIFY_KIND_TEST, "kind test (formatter name not substring-matched)");
    TEST(verify_kind_for_command("npm test")==VERIFY_KIND_TEST, "kind test (default)");
    TEST(verify_kind_for_command("check-config")==VERIFY_KIND_CHECK, "kind check");

    /* verify_looks_like_target */
    TEST(verify_looks_like_target("foo.py"), "target foo.py");
    TEST(verify_looks_like_target("tests/"), "target tests/");
    TEST(!verify_looks_like_target("--verbose"), "not target --verbose");
    TEST(!verify_looks_like_target("FOO=1"), "not target FOO=1");

    /* verify_scope_for_args */
    { char *a[] = {"foo.py"}; TEST(verify_scope_for_args(a,1)==VERIFY_SCOPE_TARGETED, "scope targeted"); }
    { char *a[] = {"--verbose"}; TEST(verify_scope_for_args(a,1)==VERIFY_SCOPE_FULL, "scope full"); }

    /* verify_summarize_output: short -> unchanged */
    { char *s = verify_summarize_output("hello"); TEST(strcmp(s,"hello")==0, "summarize short"); free(s); }
    { char *big = (char*)malloc(5000); memset(big,'x',4999); big[4999]='\0';
      char *s = verify_summarize_output(big); TEST(strstr(s,"chars omitted")!=NULL, "summarize long has omission note"); free(s); free(big); }

    /* verification_classify: a configured verify command -> valid evidence */
    {
        char *cmds[] = {"npm run test"};
        verification_evidence_t ev;
        verification_classify("npm run test", "/repo", "sess1", 0, "", cmds, 1, "/repo", &ev);
        TEST(ev.valid, "classify: valid evidence");
        TEST(ev.kind==VERIFY_KIND_TEST, "classify: kind test");
        TEST(ev.status==VERIFY_STATUS_PASSED, "classify: passed (exit 0)");
        TEST(ev.scope==VERIFY_SCOPE_FULL, "classify: full scope");
    }
    /* non-verify command -> not evidence */
    {
        char *cmds[] = {"npm run test"};
        verification_evidence_t ev;
        verification_classify("rm -rf /tmp/x", "/repo", "sess1", 0, "", cmds, 1, "/repo", &ev);
        TEST(!ev.valid, "classify: rm is not evidence");
    }
    /* exit non-zero -> failed */
    {
        char *cmds[] = {"npm run test"};
        verification_evidence_t ev;
        verification_classify("npm run test", "/repo", "sess1", 1, "", cmds, 1, "/repo", &ev);
        TEST(ev.valid && ev.status==VERIFY_STATUS_FAILED, "classify: failed (exit 1)");
    }

    /* ── persistence (isolated temp DB) ── */
    {
        char dbdir[256];
        sprintf(dbdir, "/tmp/ve_db_%d", (int)getpid());
        mkdir(dbdir, 0755);
        char *cmds[] = {"npm run test"};
        verification_evidence_t ev;
        bool rec = verification_record_result(dbdir, "npm run test", "/repo", "sess1", 0, "ok", cmds, 1, "/repo", &ev);
        TEST(rec && ev.valid, "record_result stores evidence");

        char *status = verification_status_json(dbdir, "sess1", "/repo", cmds, 1, "/repo");
        TEST(strstr(status, "\"status\":\"passed\"")!=NULL, "status_json passed");
        TEST(strstr(status, "\"evidence\"")!=NULL, "status_json has evidence");
        free(status);

        /* mark edited -> stale */
        const char *paths[] = {"/repo/src/x.py"};
        verification_mark_edited(dbdir, "sess1", "/repo", paths, 1, "/repo");
        char *status2 = verification_status_json(dbdir, "sess1", "/repo", cmds, 1, "/repo");
        TEST(strstr(status2, "\"status\":\"stale\"")!=NULL, "status_json stale after edit");
        free(status2);

        /* record a failing run -> failed */
        verification_record_result(dbdir, "npm run test", "/repo", "sess1", 1, "boom", cmds, 1, "/repo", &ev);
        char *status3 = verification_status_json(dbdir, "sess1", "/repo", cmds, 1, "/repo");
        TEST(strstr(status3, "\"status\":\"failed\"")!=NULL, "status_json failed after failing run");
        free(status3);
    }

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
