/*
 * environments.c — Port of Python tools/environments/base.py
 *
 * Implements the low-level execution environment bridge functions.
 * The actual environment management (local subprocess, SSH, docker,
 * daytona, modal, etc.) lives in tools/environment_gaps.c and the
 * per-backend files under src/tools/.
 *
 * This file holds the utility functions that the Python
 * tools/environments/base.py provides, specifically the drain
 * helpers for streaming subprocess output.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* Port of Python tools/environments/base.py:_drain_iterable
 *
 * Fallback path for draining output from a stream that is NOT backed
 * by a real OS file descriptor (no usable fileno()). This covers
 * in-memory ProcessHandle adapters that expose stdout as a plain
 * iterator of already-collected output rather than a live pipe.
 *
 * Arguments:
 *   p1: void* — opaque stream context (e.g. a buffer or FILE* wrapper)
 *   p2: function pointer for reading the next piece from the stream
 *       signature: const char* (*next_piece)(void* ctx)
 *   p3: function pointer for checking if the stream is done
 *       signature: int (*is_done)(void* ctx)
 *   p4: string buffer to append output to (char** or string builder handle)
 *   p5: size of the buffer / capacity
 *
 * Returns: char* — the drained output, or NULL on error.
 *
 * The Python original iterates a stream and accumulates output via
 * a UTF-8 incremental decoder. In C we do the equivalent with a
 * dynamic buffer and the next_piece callback.
 */
void* cli_tools_environments_base__drain_iterable(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    if (!p1) return NULL;

    /* Callers may provide these; we can work without them */
    const char* (*next_piece)(void*) = (const char* (*)(void*))p2;
    int (*is_done)(void*) = (int (*)(void*))p3;

    /* If no callbacks, we can't drain anything — return empty */
    if (!next_piece) {
        char *empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    /* Initial buffer — 4KB, grows as needed */
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = (char*)malloc(capacity);
    if (!buffer) return NULL;
    buffer[0] = '\0';

    while (1) {
        /* Check if stream is done */
        if (is_done && is_done(p1)) {
            break;
        }

        /* Get next piece */
        const char *piece = next_piece(p1);
        if (!piece) {
            break;  /* EOF or error */
        }

        size_t piece_len = strlen(piece);
        if (piece_len == 0) {
            /* Some streams emit empty bytes between chunks */;
            continue;
        }

        /* Grow buffer if needed */
        if (length + piece_len + 1 > capacity) {
            while (length + piece_len + 1 > capacity) {
                capacity *= 2;
                if (capacity > 1024 * 1024 * 64) {
                    /* Safety cap at 64MB */
                    if (length > 0) {
                        buffer[length] = '\0';
                    }
                    return buffer;
                }
            }
            char *newbuf = (char*)realloc(buffer, capacity);
            if (!newbuf) {
                /* On realloc failure, return what we have so far */
                if (length > 0) {
                    buffer[length] = '\0';
                }
                return buffer;
            }
            buffer = newbuf;
        }

        /* Append this piece */
        memcpy(buffer + length, piece, piece_len);
        length += piece_len;
        buffer[length] = '\0';
    }

    /* Trim buffer to actual length */
    char *result = (char*)realloc(buffer, length + 1);
    if (!result) {
        if (length > 0) {
            buffer[length] = '\0';
        }
        return buffer;
    }
    return result;
}