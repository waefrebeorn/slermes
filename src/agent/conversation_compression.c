/*
 * conversation_compression.c — Name parity wrapper for Python agent/conversation_compression.py
 *
 * NOTE: The C implementation lives in src/agent/llm_client.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/conversation_compression.py.
 * C implementation: src/agent/llm_client.c
 *
 * Key functions ported:
 *   Conversation compression via LLM summary. C implementation in llm_client.c: compress_conversation, build_compression_prompt, extract_summary_from_response, replay_compression_warning.
 */
