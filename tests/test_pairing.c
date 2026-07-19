/*
 * test_pairing.c — Faithful port of gateway/pairing.py PairingStore.
 */

#include "pairing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

int main(void) {
    char tmpl[] = "/tmp/pair_XXXXXX";
    char *dir = mkdtemp(tmpl);
    pairing_store_t *st = pairing_store_open(dir);

    double now = 1000000.0;

    /* generate + approve round trip (no rate-limit interaction across distinct users) */
    char *code1 = pairing_generate_code(st, "telegram", "12345", "Alice", now);
    CHECK(code1 != NULL, "generate_code returns a code");
    CHECK(strlen(code1) == PAIRING_CODE_LENGTH, "code is 8 chars");
    for (size_t i=0;i<strlen(code1);i++) CHECK(strchr(PAIRING_ALPHABET, code1[i]) != NULL, "code chars in alphabet");

    pairing_result_t *r = pairing_approve_code(st, "telegram", code1, now);
    CHECK(r != NULL, "approve_code succeeds");
    CHECK(r && strcmp(r->user_id, "12345") == 0, "approved user_id matches");
    CHECK(r && strcmp(r->user_name, "Alice") == 0, "approved user_name matches");
    pairing_free_result(r);
    free(code1);

    CHECK(pairing_is_approved(st, "telegram", "12345"), "user is approved after approval");
    CHECK(!pairing_is_approved(st, "telegram", "99999"), "other user not approved");

    /* code is single-use: re-approving the same code fails */
    pairing_result_t *r1b = pairing_approve_code(st, "telegram", code1, now);
    CHECK(r1b == NULL, "reused consumed code rejected");
    pairing_free_result(r1b);

    /* revoke */
    CHECK(pairing_revoke(st, "telegram", "12345"), "revoke returns true");
    CHECK(!pairing_is_approved(st, "telegram", "12345"), "revoked user not approved");
    CHECK(!pairing_revoke(st, "telegram", "12345"), "revoke again returns false");

    /* max pending per platform = 3 */
    char *c[4];
    char uidbuf[16];
    for (int i=0;i<3;i++){ snprintf(uidbuf,sizeof(uidbuf),"u%d",i); c[i] = pairing_generate_code(st, "discord", uidbuf, "U", now); }
    CHECK(c[0] && c[1] && c[2], "three pending codes generated");
    c[3] = pairing_generate_code(st, "discord", "u4", "U", now);
    CHECK(c[3] == NULL, "4th pending blocked (max 3)");
    for (int i=0;i<3;i++) free(c[i]);

    /* rate limit: same user can't request again within RATE_LIMIT_SECONDS */
    char *a = pairing_generate_code(st, "slack", "sameuser", "A", now);
    CHECK(a != NULL, "first request ok");
    char *b = pairing_generate_code(st, "slack", "sameuser", "A", now + 10); /* within 600s */
    CHECK(b == NULL, "rate-limited within window");
    char *b2 = pairing_generate_code(st, "slack", "sameuser", "A", now + PAIRING_RATE_LIMIT_SECONDS + 1);
    CHECK(b2 != NULL, "request allowed after window");
    free(a); free(b2);

    /* lockout after MAX_FAILED_ATTEMPTS failed approvals */
    char *badcode = "ZZZZZZZZ";
    for (int i=0;i<PAIRING_MAX_FAILED_ATTEMPTS;i++)
        pairing_approve_code(st, "matrix", badcode, now + i);
    CHECK(pairing_is_locked_out(st, "matrix", now + 100), "platform locked out after failures");
    char *good = pairing_generate_code(st, "matrix", "m1", "M", now);
    CHECK(good == NULL, "generate blocked while locked out");
    free(good);
    CHECK(!pairing_is_locked_out(st, "matrix", now + 100 + PAIRING_LOCKOUT_SECONDS), "lockout expires");

    /* pending TTL: code expires after CODE_TTL_SECONDS */
    char *exp = pairing_generate_code(st, "signal", "s1", "S", now);
    pairing_result_t *exp_r = pairing_approve_code(st, "signal", exp, now + PAIRING_CODE_TTL_SECONDS + 1);
    CHECK(exp_r == NULL, "expired code rejected");
    pairing_free_result(exp_r); free(exp);

    /* list_pending shows entries; list_approved reflects approvals */
    pairing_clear_pending(st, "discord");
    int np; pairing_pending_t *pp;
    np = pairing_list_pending(st, "discord", now, &pp);
    CHECK(np == 0, "clear_pending emptied discord");
    pairing_free_pending(pp, np);

    int na; pairing_approved_t *ap;
    na = pairing_list_approved(st, "telegram", &ap);
    CHECK(na == 0, "telegram no longer approved after revoke");
    pairing_free_approved(ap, na);

    rmdir(dir);
    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
