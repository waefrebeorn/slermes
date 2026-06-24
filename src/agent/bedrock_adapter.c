/*
 * bedrock_adapter.c — Port of Python agent/bedrock_adapter.py
 *
 * Python API → C implementation mapping:
 *   bedrock_adapter_process_message()  → provider_bedrock.c
 *   bedrock_adapter_stream()           → provider_bedrock.c
 *   bedrock_adapter_build_request()    → provider_bedrock.c
 *   bedrock_adapter_parse_response()   → provider_bedrock.c
 *   bedrock_adapter_count_tokens()     → provider_bedrock.c
 *
 * AWS Bedrock provider adapter implemented in provider_bedrock.c.
 */

#include "provider.h"   /* provider_bedrock interface via provider dispatch */
