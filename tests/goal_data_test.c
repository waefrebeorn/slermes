/*
 * goal_data_test.c — real behavioral test for port_goals_data.c
 *
 * Mirrors the documented semantics of hermes_cli/goals.py:
 *   - GoalContract emptiness + field set/get + render block
 *   - parse_contract inline "field: value" parsing + headline join
 *   - GoalState JSON round-trip + subgoals + meta-key
 */

#include "goal_contract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;

#define CHECK(cond, msg) do { \
    checks++; \
    if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } \
} while (0)

#define EQ_STR(a, b, msg) do { \
    checks++; \
    const char *_a = (a), *_b = (b); \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        failures++; printf("FAIL: %s\n  got=%s\n  exp=%s\n", msg, _a ? _a : "(null)", _b ? _b : "(null)"); \
    } \
} while (0)

static void test_contract_basic(void) {
    goal_contract_t *c = goal_contract_new();
    CHECK(goal_contract_is_empty(c), "new contract is empty");

    goal_contract_set(c, "verification", "the auth test suite passes");
    goal_contract_set(c, "constraints", "keep /login response shape unchanged");
    CHECK(!goal_contract_is_empty(c), "contract non-empty after set");
    EQ_STR(goal_contract_get(c, "verification"), "the auth test suite passes", "get verification");
    EQ_STR(goal_contract_get(c, "constraints"), "keep /login response shape unchanged", "get constraints");
    EQ_STR(goal_contract_get(c, "outcome"), "", "unset outcome is empty");
    EQ_STR(goal_contract_get(c, "not_a_field"), "", "unknown field returns empty");

    char *render = goal_contract_render(c);
    CHECK(render && strstr(render, "- Verification: the auth test suite passes"), "render has verification line");
    CHECK(render && strstr(render, "- Constraints: keep /login response shape unchanged"), "render has constraints line");
    CHECK(render && !strstr(render, "Outcome:"), "render omits empty outcome");
    free(render);
    goal_contract_free(c);
}

static void test_contract_from_json(void) {
    goal_contract_t *c = goal_contract_new();
    const char *json = "{\"outcome\":\"ship it\",\"verification\":\"tests green\",\"constraints\":\"\",\"boundaries\":\"\",\"stop_when\":\"\"}";
    CHECK(goal_contract_from_json(c, json), "from_json parses");
    EQ_STR(goal_contract_get(c, "outcome"), "ship it", "json outcome");
    EQ_STR(goal_contract_get(c, "verification"), "tests green", "json verification");
    CHECK(!goal_contract_is_empty(c), "json contract non-empty");
    goal_contract_free(c);
}

static void test_parse_contract(void) {
    /* Free-form headline with incidental colon must NOT be mangled. */
    char *head = NULL; goal_contract_t *contract = NULL;
    CHECK(parse_contract("Fix bug: the parser", &head, &contract), "parse free-form");
    EQ_STR(head, "Fix bug: the parser", "free-form headline preserved");
    CHECK(goal_contract_is_empty(contract), "free-form has empty contract");
    free(head); goal_contract_free(contract);

    /* Inline field parsing. */
    const char *text =
        "Migrate auth to JWT\n"
        "verify: the auth test suite passes\n"
        "constraints: keep the public /login response shape unchanged\n"
        "boundaries: only touch services/auth and its tests\n"
        "stop when: a schema change needs product sign-off";
    CHECK(parse_contract(text, &head, &contract), "parse structured");
    EQ_STR(head, "Migrate auth to JWT", "structured headline");
    EQ_STR(goal_contract_get(contract, "verification"), "the auth test suite passes", "parse verify alias");
    EQ_STR(goal_contract_get(contract, "constraints"), "keep the public /login response shape unchanged", "parse constraints");
    EQ_STR(goal_contract_get(contract, "boundaries"), "only touch services/auth and its tests", "parse boundaries");
    EQ_STR(goal_contract_get(contract, "stop_when"), "a schema change needs product sign-off", "parse stop when");
    free(head); goal_contract_free(contract);

    /* "done when:" alias maps to outcome. */
    CHECK(parse_contract("Ship the feature\ndone when: CI is green", &head, &contract), "parse done when");
    EQ_STR(goal_contract_get(contract, "outcome"), "CI is green", "done when -> outcome");
    free(head); goal_contract_free(contract);

    /* Empty input. */
    CHECK(parse_contract("", &head, &contract), "parse empty");
    EQ_STR(head, "", "empty headline");
    CHECK(goal_contract_is_empty(contract), "empty contract");
    free(head); goal_contract_free(contract);
}

static void test_goal_state_roundtrip(void) {
    goal_state_t *s = goal_state_new("Write the report");
    CHECK(s != NULL, "new goal state");
    char *js_free = goal_state_to_json(s);
    free(js_free); /* smoke: serialize returns a valid buffer */
    goal_state_free(s); /* no leak */

    /* populate */
    goal_state_t *g = goal_state_new("Build the parser");
    goal_state_add_subgoal(g, "write tokenizer");
    goal_state_add_subgoal(g, "write parser");
    goal_state_add_subgoal(g, "write tokenizer"); /* dup ignored */
    CHECK(goal_state_subgoal_count(g) == 2, "subgoal dedupe");

    char *json = goal_state_to_json(g);
    CHECK(json && strstr(json, "\"goal\":\"Build the parser\""), "json has goal");
    CHECK(json && strstr(json, "\"status\":\"active\""), "json has status active");
    CHECK(json && strstr(json, "write tokenizer"), "json has subgoal 1");
    CHECK(json && strstr(json, "write parser"), "json has subgoal 2");

    goal_state_t *g2 = goal_state_new("placeholder");
    CHECK(goal_state_from_json(json, g2), "from_json loads");
    char *json2 = goal_state_to_json(g2);
    CHECK(json2 && strstr(json2, "\"goal\":\"Build the parser\""), "roundtrip goal via json");
    CHECK(json2 && strstr(json2, "\"status\":\"active\""), "roundtrip status via json");
    CHECK(strstr(json2, "write tokenizer"), "roundtrip subgoal 1 via json");
    CHECK(strstr(json2, "write parser"), "roundtrip subgoal 2 via json");
    free(json);
    free(json2);
    goal_state_free(g);
    goal_state_free(g2);

    /* meta key */
    char *k = goal_meta_key("sess-123");
    EQ_STR(k, "goal:sess-123", "meta key format");
    free(k);
}

static void test_subgoal_render(void) {
    goal_state_t *g = goal_state_new("goal");
    char *empty = goal_state_render_subgoals(g);
    EQ_STR(empty, "", "no subgoals -> empty");
    free(empty);
    goal_state_add_subgoal(g, "first");
    goal_state_add_subgoal(g, "second");
    char *blk = goal_state_render_subgoals(g);
    CHECK(strstr(blk, "- 1. first"), "subgoal 1 numbered");
    CHECK(strstr(blk, "- 2. second"), "subgoal 2 numbered");
    free(blk);
    goal_state_free(g);
}

int main(void) {
    test_contract_basic();
    test_contract_from_json();
    test_parse_contract();
    test_goal_state_roundtrip();
    test_subgoal_render();
    printf("goal_data_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
