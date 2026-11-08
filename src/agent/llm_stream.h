/*
 * llm_stream.h — streaming chunk-callback context + handlers.
 *
 * Module-internal header (llm_client.c <-> llm_stream.c). Holds the
 * per-stream context type and the stream-callback entry points so the
 * callbacks can live in their own self-contained module.
 */
#ifndef LLM_STREAM_H
#define LLM_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_core_types.h"   /* llm_response_t, llm_token_cb_t */
#include "provider.h"            /* provider_ops_t, provider_t */
#include "hermes_http.h"         /* http_resp_t */

/* ================================================================
 *  Streaming: per-chunk callback context
 * ================================================================ */

typedef struct {
    llm_response_t  *resp;      /* accumulated response */
    llm_token_cb_t   token_cb;  /* token callback */
    void            *userdata;  /* callback userdata */
    const provider_ops_t *prov_ops;  /* provider ops for per-chunk parsing (optional) */
    provider_t      *prov;      /* provider instance (optional) */
    /* Track tool_calls across chunks (streaming builds them incrementally) */
    int              max_tc_idx; /* highest tool_call index seen */
    char             tc_id[64][64];  /* id per index */
    char             tc_name[64][128]; /* name per index */
    char             tc_args[64][4096]; /* args buffer per index */
    bool             streaming_done; /* set when streaming is complete */
    /* P95: Stream diagnostic timing */
    double           req_start_time;  /* monotonic time of request send */
    size_t           token_count;     /* running token count */
    bool             first_token_flag; /* first content token tracked */
} stream_ctx_t;

/* Stream chunk callbacks (llm_stream.c) */
int  on_provider_stream_chunk(const char *data, size_t len, void *userdata);
int  on_stream_chunk(const char *data, size_t len, void *userdata);
void finalize_stream_toolcalls(stream_ctx_t *ctx);
void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers);
time_t extract_api_error_context(const http_resp_t *resp);
void finalize_stream_diag(stream_ctx_t *ctx);

#endif /* LLM_STREAM_H */
