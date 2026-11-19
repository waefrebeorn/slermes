/* t_port_state_guard.c — oracle harness for persistence/sqlite/_state/guard.py
 * pure predicate logic. Emits one JSON line per case. */
#include "port_state_guard.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void json_str_or_null(const char *s)
{
    if (s)
        jstr_escape(s);
    else
        printf("null");
}

static void env_case(int cs, const char *cur, const char *ver,
                     const char *marker)
{
    bool out = sg_running_under_pytest(cur, ver, marker);
    printf("{\"case\":%d,\"fn\":\"_running_under_pytest\",\"in\":{"
           "\"pytest_current_test\":", cs);
    json_str_or_null(cur);
    printf(",\"pytest_version\":");
    json_str_or_null(ver);
    printf(",\"marker\":");
    json_str_or_null(marker);
    printf("},\"out\":%s}\n", out ? "true" : "false");
    fflush(stdout);
}

static void argv_case(int cs, const char *const *args, int n)
{
    bool out = sg_process_looks_like_pytest(args, n);
    printf("{\"case\":%d,\"fn\":\"_process_looks_like_pytest\",\"in\":"
           "{\"cmdline\":[", cs);
    for (int i = 0; i < n; i++) {
        if (i)
            putchar(',');
        jstr_escape(args[i]);
    }
    printf("]},\"out\":%s}\n", out ? "true" : "false");
    fflush(stdout);
}

static void path_case(int cs, const char *resolved, const char *root)
{
    bool out = sg_is_production_state_db(resolved, root);
    printf("{\"case\":%d,\"fn\":\"_is_production_state_db\",\"in\":{"
           "\"resolved\":", cs);
    jstr_escape(resolved);
    printf(",\"root\":");
    jstr_escape(root);
    printf("},\"out\":%s}\n", out ? "true" : "false");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _running_under_pytest: env truthiness (unset NULL / "" both falsy). */
    env_case(1, NULL, NULL, NULL);
    env_case(2, "test_x.py::test_a (call)", NULL, NULL);
    env_case(3, NULL, "8.3.0", NULL);
    env_case(4, NULL, NULL, "1");
    env_case(5, "", NULL, NULL);
    env_case(6, NULL, "", "");

    /* _process_looks_like_pytest: launcher basenames, the fork-server runner
     * script, and the false-positive guards. */
    {
        static const char *const a7[] = { "python3", "-m", "pytest", "-q" };
        argv_case(7, a7, 4);
        static const char *const a8[] = { "C:\\tools\\pytest.exe", "tests" };
        argv_case(8, a8, 2);
        static const char *const a9[] = { "python",
                                          "scripts/run_tests_parallel.py",
                                          "--x" };
        argv_case(9, a9, 3);
        static const char *const a10[] = { "python", "-u",
                                           "run_tests_parallel.py" };
        argv_case(10, a10, 3);
        static const char *const a11[] = { "hermes", "chat", "-q",
                                           "fix scripts/run_tests_parallel.py" };
        argv_case(11, a11, 4);
        argv_case(12, NULL, 0);
        static const char *const a13[] = { "'\"pytest\"'" };
        argv_case(13, a13, 1);
        static const char *const a14[] = { "python", "/tmp/pytest-of-dev/x.py" };
        argv_case(14, a14, 2);
    }
    /* Unreadable cmdline fails open (not pytest). */
    printf("{\"case\":15,\"fn\":\"_process_looks_like_pytest\",\"in\":"
           "{\"cmdline_error\":true},\"out\":%s}\n",
           sg_process_looks_like_pytest(NULL, 0) ? "true" : "false");
    fflush(stdout);

    /* _is_production_state_db: <root>/state.db and the profiles slot only. */
    path_case(16, "C:/hermes/state.db", "C:/hermes");
    path_case(17, "C:/hermes/profiles/work/state.db", "C:/hermes");
    path_case(18, "C:/hermes/profiles/work/sub/state.db", "C:/hermes");
    path_case(19, "C:/other/state.db", "C:/hermes");
    path_case(20, "C:/HERMES/state.db", "C:/hermes");
    path_case(21, "C:/hermes/Profiles/work/state.db", "C:/hermes");
    path_case(22, "state.db", ".");
    path_case(23, "C:/hermes/profiles/state.db", "C:/hermes");
    path_case(24, "C:/hermes/state.db", "C:/hermes/");
    return 0;
}
