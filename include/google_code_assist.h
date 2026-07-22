/**
 * @file google_code_assist.h
 * @brief Google Code Assist API client wrappers.
 *
 * Port of Python agent/google_code_assist.py.
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef GOOGLE_CODE_ASSIST_H
#define GOOGLE_CODE_ASSIST_H

#include "hermes_core_types.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return Code Assist client metadata JSON object.
 * Port of Python: _client_metadata()
 */
json_t *client_metadata(void);

/**
 * Detect VPC-SC violation from response body.
 * Port of Python: _is_vpc_sc_violation()
 */
bool is_vpc_sc_violation(const char *body);

/**
 * POST JSON to Code Assist API.
 * Returns parsed JSON response or empty object on error.
 * Port of Python: _post_json()
 */
json_t *google_code_assist_post_json(
    const char *url,
    json_t *body_json,
    const char *access_token,
    int timeout_seconds);

#ifdef __cplusplus
}
#endif

#endif /* GOOGLE_CODE_ASSIST_H */
