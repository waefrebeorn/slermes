/*
 * test_checkpoint_persist.c — Tests for v322 filesystem persistence.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       test_checkpoint_persist.c src/agent/checkpoint.c src/agent/context.c \
 *       lib/libjson/json.c -o /tmp/t_chkpersist -lm
 *
 * Run:
 *   /tmp/t_chkpersist
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes.h"

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        printf("  \xe2\x9c\x93 %s\n", name); \
        passed++; \
    } else { \
        printf("  \xe2\x9c\x97 %s (FAILED)\n", name); \
        failed++; \
        fflush(stdout); \
    } \
} while(0)

static agent_state_t *make_state(void) {
    agent_state_t *s = (agent_state_t *)calloc(1, sizeof(agent_state_t));
    if (!s) return NULL;
    s->message_capacity = 16;
    s->messages = (message_t **)calloc(s->message_capacity, sizeof(message_t *));
    return s;
}

static void free_state(agent_state_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->message_count; i++)
        message_free(s->messages[i]);
    free(s->messages);
    free(s);
}

/* Stub: checkpoint_branch_restore needs agent_session_branch from agent_loop.c */
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point) {
    (void)state; (void)new_id; (void)branch_point;
    return false;
}

int main(void) {
    printf("=== Checkpoint Persistence Tests (v322) ===\n\n");

    /* --- Test 1: init_dir --- */
    printf("[PERSIST] init_dir:\n");
    bool dir_ok = checkpoint_init_dir();
    TEST("init_dir success", dir_ok);
    /* Call again — should be idempotent */
    TEST("init_dir idempotent", checkpoint_init_dir());

    /* --- Test 2: save, list, load roundtrip --- */
    printf("\n[PERSIST] Save/Load roundtrip:\n");

    checkpoint_manager_t mgr;
    checkpoint_init(&mgr);

    agent_state_t *st = make_state();
    st->message_count = 2;
    st->messages[0] = (message_t *)calloc(1, sizeof(message_t));
    st->messages[0]->content = strdup("persist test msg 1");
    st->messages[0]->role = MSG_USER;
    st->messages[1] = (message_t *)calloc(1, sizeof(message_t));
    st->messages[1]->content = strdup("persist test msg 2");
    st->messages[1]->role = MSG_ASSISTANT;

    /* Save in-memory checkpoint */
    bool saved = checkpoint_save(&mgr, st, "persist-test");
    TEST("in-memory save success", saved);
    TEST("checkpoint has id", mgr.checkpoints[0].id[0] != '\0');

    /* Persist to filesystem */
    bool persist_ok = checkpoint_persist_save(&mgr.checkpoints[0]);
    TEST("persist_save success", persist_ok);

    /* List persisted files */
    char ids[8][64] = {{0}};
    size_t n = checkpoint_persist_list(ids, 8);
    TEST("persist_list > 0", n > 0);
    if (n > 0) {
        TEST("first id non-empty", ids[0][0] != '\0');
    }

    /* Load from filesystem */
    if (n > 0 && ids[0][0]) {
        checkpoint_t *loaded = checkpoint_persist_load(ids[0]);
        TEST("persist_load non-NULL", loaded != NULL);
        if (loaded) {
            TEST("loaded id matches", strcmp(loaded->id, mgr.checkpoints[0].id) == 0);
            TEST("loaded has msgs", loaded->count > 0);
            TEST("loaded content", loaded->count > 0 &&
                 loaded->messages[0] && loaded->messages[0]->content &&
                 strncmp(loaded->messages[0]->content, "persist test", 12) == 0);
            /* Free loaded checkpoint's messages manually */
            for (size_t i = 0; i < loaded->count; i++)
                if (loaded->messages[i]) message_free(loaded->messages[i]);
            free(loaded->messages);
            free(loaded);
        }
    }

    /* --- Test 3: Load nonexistent --- */
    printf("\n[PERSIST] Edge cases:\n");
    checkpoint_t *bad = checkpoint_persist_load("nonexistent_checkpoint_id");
    TEST("load nonexistent returns NULL", bad == NULL);

    /* Load NULL */
    bad = checkpoint_persist_load(NULL);
    TEST("load NULL returns NULL", bad == NULL);

    /* List with NULL */
    size_t zero = checkpoint_persist_list(NULL, 5);
    TEST("list NULL returns 0", zero == 0);

    /* List with 0 count */
    char dummy[2][64] = {{0}};
    zero = checkpoint_persist_list(dummy, 0);
    TEST("list max_count=0 returns 0", zero == 0);

    /* Save NULL checkpoint */
    bool null_save = checkpoint_persist_save(NULL);
    TEST("save NULL returns false", !null_save);

    /* --- Test 4: Prune with invalid args --- */
    printf("\n[PERSIST] Prune safety:\n");
    TEST("prune 0 returns 0", checkpoint_persist_prune(0) == 0);
    TEST("prune -1 returns 0", checkpoint_persist_prune(-1) == 0);
    TEST("prune -100 returns 0", checkpoint_persist_prune(-100) == 0);

    /* --- Cleanup test data --- */
    /* Remove our test file by ID */
    if (n > 0 && ids[0][0]) {
        const char *base = getenv("HERMES_HOME");
        if (!base) base = getenv("HOME");
        if (base) {
            char fpath[512];
            snprintf(fpath, sizeof(fpath), "%s/.hermes/checkpoints/%s.json", base, ids[0]);
            remove(fpath);
        }
    }

    free_state(st);
    checkpoint_free(&mgr);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
