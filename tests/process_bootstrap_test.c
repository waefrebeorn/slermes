/*
 * process_bootstrap_test.c — behavioral tests for the process_bootstrap.c
 * port (agent/process_bootstrap.py gaps): _SafeWriter, _OpenAIProxy,
 * build_keepalive_http_client.
 *
 * No real network: build_keepalive_http_client constructs a client with the
 * env proxy policy but never performs a request.
 */
#include "process_bootstrap.h"
#include "hermes_proxy_utils.h"
#include "hermes_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

/* ---- _SafeWriter ------------------------------------------------------ */
static void test_safe_writer(void) {
    char tmpl[] = "/tmp/pb_test_XXXXXX";
    int fd = mkstemp(tmpl);
    CHECK(fd >= 0, "mkstemp for safe_writer");
    if (fd < 0) return;

    FILE *f = fdopen(fd, "w+");
    CHECK(f != NULL, "fdopen safe_writer target");
    if (!f) { close(fd); return; }

    safe_writer_t *w = safe_writer_create(f);
    CHECK(w != NULL, "safe_writer_create");

    const char *msg = "hello safe writer\n";
    size_t n = safe_writer_write(w, msg, strlen(msg));
    CHECK(n == strlen(msg), "safe_writer_write returns full length");
    safe_writer_flush(w);
    CHECK(safe_writer_fileno(w) == fd, "safe_writer_fileno matches underlying fd");
    CHECK(safe_writer_isatty(w) == 0, "safe_writer_isatty false for file");
    CHECK(safe_writer_inner(w) == f, "safe_writer_inner forwards stream");

    /* Broken-pipe behaviour: writing to a closed pipe must not crash and
     * must return the requested length (Python swallows OSError/ValueError). */
    int pfd[2];
    CHECK(pipe(pfd) == 0, "pipe for broken-write test");
    FILE *wp = fdopen(pfd[1], "w");
    safe_writer_t *bw = safe_writer_create(wp);
    close(pfd[0]); /* reader gone -> writes will EPIPE */
    size_t bn = safe_writer_write(bw, "x", 1);
    CHECK(bn == 1, "safe_writer_write swallows EPIPE (returns len)");
    safe_writer_flush(bw); /* must not crash */

    fclose(wp);
    safe_writer_free(bw);
    safe_writer_free(w);
    fclose(f);
    unlink(tmpl);
}

/* ---- _OpenAIProxy ----------------------------------------------------- */
static void test_openai_proxy(void) {
    CHECK(strcmp(openai_proxy_repr(), "<lazy openai.OpenAI proxy>") == 0,
          "openai_proxy_repr literal");
    CHECK(openai_proxy_is_instance(NULL) == 0, "is_instance(NULL) false");
    /* A real http_t is a member of the OpenAI-compatible family. */
    struct http_t *h = build_keepalive_http_client("https://api.openai.com/v1", 0, 1);
    CHECK(openai_proxy_is_instance(h) == 1, "is_instance(http_t) true");
    if (h) http_client_free(h);
}

/* ---- build_keepalive_http_client -------------------------------------- */
static void test_keepalive_client(void) {
    /* Copilot endpoint: bare client, no proxy. */
    struct http_t *c = build_keepalive_http_client("https://api.githubcopilot.com", 0, 1);
    CHECK(c != NULL, "keepalive client for copilot base_url");
    if (c) http_client_free(c);

    /* No proxy configured -> client built, proxy cleared. */
    unsetenv("HTTPS_PROXY"); unsetenv("HTTP_PROXY");
    unsetenv("https_proxy"); unsetenv("http_proxy");
    unsetenv("ALL_PROXY"); unsetenv("all_proxy");
    struct http_t *d = build_keepalive_http_client("https://api.anthropic.com", 0, 1);
    CHECK(d != NULL, "keepalive client built when no proxy env");
    if (d) http_client_free(d);

    /* Proxy configured and not excluded -> proxy applied. */
    setenv("HTTPS_PROXY", "http://proxy.example:3128", 1);
    struct http_t *e = build_keepalive_http_client("https://api.openai.com/v1", 0, 1);
    CHECK(e != NULL, "keepalive client built with proxy env");
    if (e) http_client_free(e);
    unsetenv("HTTPS_PROXY");
}

int main(void) {
    printf("=== process_bootstrap_test ===\n");
    /* Production installs crash-resistant stdio (SIGPIPE -> SIG_IGN) so that
     * broken-pipe writes are reported as errors rather than killing the
     * process. The _SafeWriter relies on this, mirroring the Python
     * try/except OSError guard. */
    install_safe_stdio();
    test_safe_writer();
    test_openai_proxy();
    test_keepalive_client();
    if (g_fail == 0) { printf("ALL PROCESS_BOOTSTRAP TESTS PASSED\n"); return 0; }
    printf("%d PROCESS_BOOTSTRAP CHECK(S) FAILED\n", g_fail);
    return 1;
}
