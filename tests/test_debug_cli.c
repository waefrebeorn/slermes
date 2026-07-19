/*
 * test_debug_cli.c — Faithful port of hermes_cli/debug.py pure logic.
 */

#include "debug_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

/* count how many times delete_cb called */
static int g_deleted_calls = 0;
static int fake_delete(const char *url) { (void)url; g_deleted_calls++; return 1; }
static int fake_delete_fail(const char *url) { (void)url; g_deleted_calls++; return 0; }

int main(void) {
    char tmpl[] = "/tmp/dbgcli_XXXXXX";
    char *home = mkdtemp(tmpl);

    /* extract_paste_id */
    char *id = debug_extract_paste_id("https://paste.rs/AbC123");
    CHECK(id && strcmp(id, "AbC123") == 0, "extract paste.rs id");
    free(id);
    CHECK(debug_extract_paste_id("http://paste.rs/xYz") && strcmp(debug_extract_paste_id("http://paste.rs/xYz"),"xYz")==0, "extract http paste.rs id");
    CHECK(debug_extract_paste_id("https://dpaste.com/abc") == NULL, "dpaste not a paste.rs id");
    CHECK(debug_extract_paste_id("not a url") == NULL, "garbage not a paste.rs id");
    CHECK(debug_extract_paste_id("https://paste.rs/") == NULL, "trailing slash -> no id");

    /* record_pending + load */
    const char *urls[] = { "https://paste.rs/AAA", "https://paste.rs/BBB", "https://dpaste.com/ignore" };
    double now = 1000000.0;
    debug_record_pending(home, urls, 3, now, 21600);
    int n; debug_pending_t *pend = debug_load_pending(home, &n);
    CHECK(pend != NULL && n == 2, "record_pending stored 2 paste.rs urls (dpaste ignored)");
    /* expire_at = now + 21600 */
    bool ok_exp = false;
    for (int i=0;i<n;i++) if (pend[i].expire_at == now + 21600) ok_exp = true;
    CHECK(ok_exp, "expire_at = now + delay");
    debug_free_pending(pend, n);

    /* dedup: record same url again at the SAME now -> expiry unchanged (max keeps later) */
    const char *urls2[] = { "https://paste.rs/AAA" };
    debug_record_pending(home, urls2, 1, now, 21600);
    pend = debug_load_pending(home, &n);
    CHECK(n == 2, "duplicate url not added again");
    debug_free_pending(pend, n);

    /* sweep: nothing expired yet */
    g_deleted_calls = 0;
    int del, rem;
    debug_sweep_expired_pastes(home, now + 100, fake_delete, &del, &rem);
    CHECK(del == 0 && rem == 2, "sweep before expiry: 0 deleted, 2 remaining");
    CHECK(g_deleted_calls == 0, "delete not called before expiry");

    /* sweep: past expiry -> deletes both */
    g_deleted_calls = 0;
    debug_sweep_expired_pastes(home, now + 21600 + 1, fake_delete, &del, &rem);
    CHECK(del == 2 && rem == 0, "sweep after expiry: 2 deleted, 0 remaining");
    CHECK(g_deleted_calls == 2, "delete called twice");

    /* sweep with failing delete: retained within grace */
    const char *urls3[] = { "https://paste.rs/CCC" };
    debug_record_pending(home, urls3, 1, now, 21600); /* expire at now+21600 */
    g_deleted_calls = 0;
    debug_sweep_expired_pastes(home, now + 21600 + 1, fake_delete_fail, &del, &rem);
    CHECK(del == 0 && rem == 1, "failing delete retained within 24h grace");
    /* past grace -> reaped */
    debug_sweep_expired_pastes(home, now + 21600 + 86400 + 1, fake_delete_fail, &del, &rem);
    CHECK(del == 1 && rem == 0, "failing delete reaped after 24h grace");

    /* redact_log_text: masks secrets + emails via hermes_redact */
    char *r = debug_redact_log_text("contact me at bob@example.com please");
    CHECK(strstr(r, "[REDACTED_EMAIL]") != NULL, "email masked");
    CHECK(strstr(r, "bob@example.com") == NULL, "email removed");
    free(r);
    char *r2 = debug_redact_log_text("api_key=sk-1234567890abcdef secret");
    CHECK(r2 != NULL, "redact returns non-null");
    free(r2);
    CHECK(strcmp(debug_redact_log_text(""), "") == 0, "empty text -> empty copy");

    rmdir(home); /* pending.json remains but dir cleanup best-effort */
    rmdir(home);

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
