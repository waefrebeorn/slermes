/*
 * test_skill_ledger.c — behavioral oracle for the skill_ledger port.
 * Exercises store_blob/read_blob/snapshot_paths/append_entry/list_entries/
 * get_entry/rollback_entry and asserts against expected semantics.
 * Build: cc -D_POSIX_C_SOURCE=200809L -I../include -I../lib/libjson -I../lib/libhash \
 *        -I../lib/libuuid ../src/tools/skill_ledger.c test_skill_ledger.c \
 *        ../lib/libjson/json.o ../lib/libhash/hash.o -o t && ./t
 */
#include "skill_ledger.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CK(cond) do { if(!(cond)){ printf("FAIL: %s (line %d)\n", #cond, __LINE__); fails++; } else { printf("ok: %s\n", #cond); } } while(0)

static int fails = 0;

/* Minimal get_hermes_home stub matching src/hermes_cli/web_server_paths.c. */
const char *get_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp/.hermes";
    return home;
}

int main(void) {
    setenv("HERMES_HOME", "/tmp/skill_ledger_oracle", 1);
    setenv("HOME", "/tmp/skill_ledger_oracle", 1);

    /* wipe */
    system("rm -rf /tmp/skill_ledger_oracle");
    mkdir("/tmp/skill_ledger_oracle", 0700);
    mkdir("/tmp/skill_ledger_oracle/skills", 0700);
    mkdir("/tmp/skill_ledger_oracle/.curator_backups", 0700);
    mkdir("/tmp/skill_ledger_oracle/.curator_backups/blobs", 0700);

    /* store_blob + read_blob round trip (sha256 dedup, content-addressed) */
    const char *payload = "hello curator ledger";
    char *digest = store_blob((const unsigned char*)payload, strlen(payload));
    CK(digest != NULL);
    size_t bl = 0;
    unsigned char *back = read_blob(digest, &bl);
    CK(back != NULL && bl == strlen(payload) && memcmp(back, payload, bl) == 0);
    free(back);
    /* dedup: second store returns same digest, no error */
    char *digest2 = store_blob((const unsigned char*)payload, strlen(payload));
    CK(digest2 && strcmp(digest, digest2) == 0);
    free(digest); free(digest2);

    /* read_blob rejects invalid hex */
    CK(read_blob("ZZZ", NULL) == NULL);
    CK(read_blob("", NULL) == NULL);

    /* snapshot_paths on a dir */
    mkdir("/tmp/skill_ledger_oracle/skills/foo", 0700);
    FILE *f = fopen("/tmp/skill_ledger_oracle/skills/foo/SKILL.md", "w");
    fputs("# Foo\n", f); fclose(f);
    skill_ledger_manifest_t *snap = snapshot_paths("/tmp/skill_ledger_oracle/skills/foo");
    CK(snap && snap->count == 1);
    skill_ledger_manifest_free(snap);

    /* snapshot_paths on a single file */
    skill_ledger_manifest_t *snapf = snapshot_paths("/tmp/skill_ledger_oracle/skills/foo/SKILL.md");
    CK(snapf && snapf->count == 1);
    skill_ledger_manifest_free(snapf);

    /* capture_before + record_mutation appends JSONL entry */
    char *bid = record_mutation("edit", "foo",
                                 "/tmp/skill_ledger_oracle/skills/foo",
                                 NULL,
                                 "/tmp/skill_ledger_oracle/skills/foo",
                                 "user", "{\"k\":1}");
    CK(bid != NULL);
    free(bid);

    /* list_entries newest-first */
    size_t n = 0;
    skill_ledger_entry_t **rows = list_entries(NULL, -1, &n);
    CK(rows != NULL && n >= 1);
    CK(strcmp(rows[0]->action, "edit") == 0);
    CK(strcmp(rows[0]->actor, "user") == 0);
    skill_ledger_entries_free(rows, n);

    /* get_entry fetches by id — record one then fetch */
    char *bid2 = record_mutation("create", "bar", NULL, NULL,
                                 "/tmp/skill_ledger_oracle/skills/foo", NULL, NULL);
    skill_ledger_entry_t *e = get_entry(bid2);
    CK(e != NULL && strcmp(e->skill, "bar") == 0);
    skill_ledger_entry_free(e);
    free(bid2);

    /* get_entry missing */
    CK(get_entry("deadbeef0000") == NULL);

    /* ledger_enabled default true */
    CK(ledger_enabled() == true);

    /* ── rollback_entry: fail-closed on out-of-home path (defense in depth) ── */
    char *bad_id = record_mutation("edit", "evil", NULL, NULL,
                                   "/etc/passwd", NULL, NULL);
    if (bad_id) {
        char msg[512];
        bool ok = rollback_entry(bad_id, msg, sizeof(msg));
        CK(ok == false);  /* refused: references a path outside HERMES_HOME */
        free(bad_id);
    }

    /* ── rollback_entry: create-then-mutate, then undo restores original ── */
    /* Reset ledger to a clean state for a deterministic rollback. */
    system("rm -rf /tmp/skill_ledger_oracle2");
    setenv("HERMES_HOME", "/tmp/skill_ledger_oracle2", 1);
    mkdir("/tmp/skill_ledger_oracle2", 0700);
    mkdir("/tmp/skill_ledger_oracle2/skills", 0700);
    mkdir("/tmp/skill_ledger_oracle2/.curator_backups", 0700);
    mkdir("/tmp/skill_ledger_oracle2/.curator_backups/blobs", 0700);
    mkdir("/tmp/skill_ledger_oracle2/skills/baz", 0700);
    FILE *bf = fopen("/tmp/skill_ledger_oracle2/skills/baz/SKILL.md", "w");
    fputs("# original\n", bf); fclose(bf);

    /* Capture BEFORE state and AFTER state of the same skill root. A faithful
     * caller snapshots before_root while the file is still original, mutates,
     * then record_mutation snapshots after_root. Here we record with
     * before_root == after_root == the still-original skill dir, then mutate,
     * so rollback restores the original to the (same) skill-file path. */
    char *edit_id = record_mutation("edit", "baz",
                                     "/tmp/skill_ledger_oracle2/skills/baz",
                                     NULL,
                                     "/tmp/skill_ledger_oracle2/skills/baz",
                                     "agent", NULL);
    CK(edit_id != NULL);
    FILE *mf = fopen("/tmp/skill_ledger_oracle2/skills/baz/SKILL.md", "w");
    fputs("# mutated by operator\n", mf); fclose(mf);

    /* Now roll back the "edit": restores the before-state (original) blob. */
    char rmsg[512];
    bool rok = rollback_entry(edit_id, rmsg, sizeof(rmsg));
    CK(rok == true);
    FILE *vf = fopen("/tmp/skill_ledger_oracle2/skills/baz/SKILL.md", "r");
    char buf2[256] = {0};
    if (vf) { fread(buf2, 1, sizeof(buf2)-1, vf); fclose(vf); }
    CK(strstr(buf2, "# original") != NULL);
    free(edit_id);

    printf("\n%s (%d failures)\n", fails ? "ORACLE FAILED" : "ORACLE PASSED", fails);
    return fails ? 1 : 0;
}
