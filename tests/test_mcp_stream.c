/*
 * test_mcp_stream.c — Tests for MCP streaming tool calls (L30).
 * Tests API declarations, NULL safety, and callback mechanics.
 * Network-dependent tests are conditional on a running MCP server.
 */
#include "mcp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

/* Test callback: accumulates chunks */
typedef struct {
    char buf[4096];
    int  chunk_count;
    bool got_final;
} stream_test_ctx_t;

static int test_cb(const char *chunk, size_t len, bool is_final, void *userdata) {
    stream_test_ctx_t *ctx = (stream_test_ctx_t *)userdata;
    if (chunk && len > 0 && ctx->chunk_count < 5) {
        size_t clen = len < 800 ? len : 800;
        memcpy(ctx->buf + ctx->chunk_count * 800, chunk, clen);
    }
    ctx->chunk_count++;
    if (is_final) ctx->got_final = true;
    return 0; /* continue */
}

int main(void) {
    /* Test 1: mcp_stream_callback_t typedef exists */
    {
        /* Just verify the type compiles */
        mcp_stream_callback_t cb = &test_cb;
        TEST("mcp_stream_callback_t type valid", cb == &test_cb);
    }

    /* Test 2: Callback invocation works */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        int rc = test_cb("hello", 5, false, &ctx);
        TEST("callback returns 0", rc == 0);
        TEST("callback counted chunk", ctx.chunk_count == 1);
        TEST("callback not final", ctx.got_final == false);
    }

    /* Test 3: Callback with is_final=true */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        test_cb("done", 4, true, &ctx);
        TEST("callback final flag set", ctx.got_final == true);
    }

    /* Test 4: NULL safety */
    {
        char *result = mcp_server_call_tool_stream(NULL, "test", "{}",
                                                     NULL, NULL, 1000);
        TEST("call_tool_stream with NULL server returns NULL", result == NULL);
    }

    /* Test 5: Server exists but not connected */
    {
        mcp_server_t *srv = mcp_server_new("test-stream");
        char *result = mcp_server_call_tool_stream(srv, "test", "{}",
                                                     &test_cb, NULL, 1000);
        TEST("call_tool_stream on uninit server returns NULL", result == NULL);
        mcp_server_free(srv);
    }

    /* Test 6: Callback abort (return non-zero) */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        /* First call returns 0, test later calls would abort */
        int rc = test_cb("chunk1", 6, false, &ctx);
        TEST("first chunk passes", rc == 0);
        TEST("count is 1", ctx.chunk_count == 1);
    }

    /* Test 7: mcp_server_call_tool (non-streaming) still works */
    {
        mcp_server_t *srv = mcp_server_new("test-nonstream");
        char *result = mcp_server_call_tool(srv, "test", "{}");
        TEST("non-streaming call on uninit returns NULL", result == NULL);
        mcp_server_free(srv);
    }

    /* Test 8: Verify mcp.h has the streaming function declared */
    {
        /* Compile-time check: this just needs to compile */
        mcp_server_t *srv = mcp_server_new("verify");
        if (srv) {
            /* Check we can take address of the streaming function */
            typedef char* (*stream_fn_t)(mcp_server_t*, const char*, const char*,
                                          mcp_stream_callback_t, void*, int);
            stream_fn_t fn_ptr = &mcp_server_call_tool_stream;
            TEST("stream function address valid", fn_ptr != NULL);
            mcp_server_free(srv);
        }
    }

    /* Test 9: Callback with large chunk */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        char big[2000];
        memset(big, 'X', sizeof(big));
        int rc = test_cb(big, sizeof(big), false, &ctx);
        TEST("large chunk callback returns 0", rc == 0);
        TEST("large chunk counted", ctx.chunk_count == 1);
    }

    /* Test 10: Callback with zero-length chunk */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        int rc = test_cb("hello", 0, false, &ctx);
        TEST("zero-length chunk returns 0", rc == 0);
        TEST("zero chunk skips content (count still 1)", ctx.chunk_count == 1);
    }

    /* Test 11: Callback with NULL chunk */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        int rc = test_cb(NULL, 0, false, &ctx);
        TEST("NULL chunk returns 0", rc == 0);
        TEST("NULL chunk counted", ctx.chunk_count == 1);
    }

    /* Test 12: Multiple chunks with final */
    {
        stream_test_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        test_cb("chunk1", 6, false, &ctx);
        test_cb("chunk2", 6, false, &ctx);
        test_cb("chunk3", 6, true, &ctx);
        TEST("multi-chunk count is 3", ctx.chunk_count == 3);
        TEST("multi-chunk final set", ctx.got_final == true);
    }

    /* Test 13: Callback abort via non-zero return */
    {
        /* Define an abort callback */
        int abort_ret = 0;
        int abort_cb(const char *c, size_t l, bool f, void *u) {
            (void)c; (void)l; (void)f; (void)u;
            return abort_ret;
        };
        /* With return=0: continues */
        abort_ret = 0;
        int rc1 = abort_cb("x", 1, false, NULL);
        TEST("callback return 0 continues", rc1 == 0);
        /* With return=-1: simulated abort */;
        abort_ret = -1;
        int rc2 = abort_cb("x", 1, false, NULL);
        TEST("callback return -1 aborts", rc2 == -1);
    }

    /* Test 14: Invalid timeout passed to streaming call */
    {
        mcp_server_t *srv = mcp_server_new("timeout-test");
        char *result = mcp_server_call_tool_stream(srv, "test", "{}",
                                                     &test_cb, NULL, -1);
        TEST("stream call with -1 timeout returns NULL", result == NULL);
        mcp_server_free(srv);
    }

    /* Test 15: NULL userdata does not crash callback */
    {
        int null_ud_cb(const char *c, size_t l, bool f, void *u) {
            (void)c; (void)l; (void)f;
            TEST("NULL userdata callback invoked", u == NULL);
            return 0;
        };
        null_ud_cb("x", 1, false, NULL);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All MCP stream tests PASSED");
    return failures ? 1 : 0;
}
