/*
 * port_base.c — Port of Python gateway/platforms/base.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Default max inbound media size: 10MB */
#define DEFAULT_MAX_MEDIA_BYTES (10 * 1024 * 1024)

/*
 * get_inbound_media_max_bytes — Return the configured max inbound media size.
 */
static int get_inbound_media_max_bytes(void)
{
    const char* env = getenv("HERMES_MAX_MEDIA_BYTES");
    if (env) {
        int val = atoi(env);
        if (val > 0) return val;
    }
    return DEFAULT_MAX_MEDIA_BYTES;
}

/*
 * _read_httpx_body_with_limit — Read an HTTP response body without exceeding the media cap.
 *
 * Python: async def _read_httpx_body_with_limit(response, *, media_type: str) -> bytes:
 *   max_bytes = get_inbound_media_max_bytes()
 *   content_length = response.headers.get("content-length")
 *   if content_length:
 *       if int(content_length) > max_bytes: raise ValueError(...)
 *   chunks = []
 *   total = 0
 *   async for chunk in response.aiter_bytes():
 *       total += len(chunk)
 *       if total > max_bytes: raise ValueError(...)
 *       chunks.append(chunk)
 *   return b"".join(chunks)
 *
 * In C: reads from a buffer with size limit checking.
 */
/* Port of Python: _read_httpx_body_with_limit */
json_t* _read_httpx_body_with_limit(const char* body_data, size_t body_len, const char* content_length_header)
{
    if (!body_data) {
        return json_new_string("");
    }

    int max_bytes = get_inbound_media_max_bytes();

    /* Check Content-Length header if provided */
    if (content_length_header) {
        long content_len = atol(content_length_header);
        if (content_len > max_bytes) {
            hermes_log(LOG_WARNING, "port", "Content-Length %ld exceeds max %d", content_len, max_bytes);
            return NULL;
        }
    }

    /* Check actual body length */
    if (body_len > (size_t)max_bytes) {
        hermes_log(LOG_WARNING, "port", "Body length %zu exceeds max %d", body_len, max_bytes);
        return NULL;
    }

    /* Return body as JSON string */
    return json_new_string(body_data);
}
