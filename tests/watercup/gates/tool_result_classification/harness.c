/* water-cup gate: tool_result_classification — traced copy of
 * src/tools/tool_result_classification.c::tool_classify_error /
 * tool_error_with_class / tool_result_is_error.
 * Asserts the retry-class contract:
 *   - success/non-error -> NONE
 *   - explicit "retryable":"true"/"false" wins
 *   - transient keywords (timeout/5xx/429/rate limit/network) -> TRANSIENT
 *   - permanent keywords (404/permission/invalid/400) -> PERMANENT
 *   - default unknown error -> PERMANENT (fail-closed)
 *   - tool_error_with_class appends _retry_class non-destructively
 * The twin (twin.py) re-implements the same contract in Python (no LIVE source;
 * this is a language-agnostic contract gate) and must agree.
 */
#include "tool_result_classification.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){printf("  FAIL: %s\n",msg);failures++;} else {printf("  ok:   %s\n",msg);} } while(0)

static const char *name_of(tool_error_class_t c){ return tool_error_class_name(c); }

int main(void) {
    /* Non-error -> NONE */
    CHECK(tool_classify_error(NULL) == TOOL_ERR_NONE, "NULL -> NONE");
    CHECK(tool_classify_error("{\"success\":true}") == TOOL_ERR_NONE, "success -> NONE");
    CHECK(!tool_result_is_error("{\"success\":true}"), "is_error(success)=false");

    /* Explicit retryable wins */
    CHECK(tool_classify_error("{\"error\":\"boom\",\"retryable\":\"true\"}") == TOOL_ERR_TRANSIENT,
          "retryable:true -> TRANSIENT");
    CHECK(tool_classify_error("{\"error\":\"boom\",\"retryable\":\"false\"}") == TOOL_ERR_PERMANENT,
          "retryable:false -> PERMANENT");

    /* Transient heuristics */
    CHECK(tool_classify_error("{\"error\":\"connection timed out\"}") == TOOL_ERR_TRANSIENT,
          "timeout -> TRANSIENT");
    CHECK(tool_classify_error("{\"error\":\"upstream 503 service unavailable\"}") == TOOL_ERR_TRANSIENT,
          "503 -> TRANSIENT");
    CHECK(tool_classify_error("{\"error\":\"429 too many requests\"}") == TOOL_ERR_TRANSIENT,
          "429 rate limit -> TRANSIENT");
    CHECK(tool_classify_error("{\"error\":\"ECONNREFUSED\",\"code\":\"econnrefused\"}") == TOOL_ERR_TRANSIENT,
          "econnrefused -> TRANSIENT");

    /* Permanent heuristics */
    CHECK(tool_classify_error("{\"error\":\"file not found\"}") == TOOL_ERR_PERMANENT,
          "not found -> PERMANENT");
    CHECK(tool_classify_error("{\"error\":\"permission denied\"}") == TOOL_ERR_PERMANENT,
          "permission -> PERMANENT");
    CHECK(tool_classify_error("{\"error\":\"invalid argument\"}") == TOOL_ERR_PERMANENT,
          "invalid -> PERMANENT");
    CHECK(tool_classify_error("{\"error\":\"bad request\",\"status\":\"400\"}") == TOOL_ERR_PERMANENT,
          "400 -> PERMANENT");

    /* Fail-closed default */
    CHECK(tool_classify_error("{\"error\":\"something weird happened\"}") == TOOL_ERR_PERMANENT,
          "unknown error -> PERMANENT (fail-closed)");

    /* tool_error_with_class appends non-destructively */
    char *tagged = tool_error_with_class("{\"error\":\"timeout\",\"retryable\":\"true\"}", TOOL_ERR_TRANSIENT);
    CHECK(tagged && strstr(tagged, "\"_retry_class\":\"transient\"") != NULL, "with_class adds _retry_class");
    CHECK(tagged && strstr(tagged, "\"error\"") != NULL, "with_class keeps error field");
    free(tagged);
    char *passthru = tool_error_with_class("{\"success\":true}", TOOL_ERR_NONE);
    CHECK(passthru && strcmp(passthru, "{\"success\":true}") == 0, "with_class passes success through");
    free(passthru);

    if (failures) { printf("RESULT: %d FAILURES\n", failures); return 1; }
    printf("RESULT: tool_result_classification contract holds (%s)\n", name_of(TOOL_ERR_NONE));
    return 0;
}
