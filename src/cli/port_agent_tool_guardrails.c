/*
 * port_agent_tool_guardrails.c — C port of agent/tool_guardrails.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

/* PoP: cli_agent_tool_guardrails__sha256 @ agent/tool_guardrails.py:_sha256 */

/* Port of Python agent/tool_guardrails.py:_sha256 */
/* Compute SHA-256 hex digest of a string value. */
char *cli_agent_tool_guardrails__sha256(const char *value)
{
    if (!value) return strdup("");

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)value, strlen(value), hash);

    /* Convert to hex string */
    char *result = (char *)malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    if (!result) return strdup("");

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(result + i * 2, "%02x", hash[i]);
    }
    result[SHA256_DIGEST_LENGTH * 2] = '\0';

    return result;
}
