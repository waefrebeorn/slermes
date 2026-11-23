/*
 * transcription_common.c — C home for upstream Hermes tools/transcription_common.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: tools_transcription_common_pending @ tools/transcription_common.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const tools_transcription_common_CONST_DEFAULT_PROVIDER = "local";
static const char *const tools_transcription_common_CONST_DEFAULT_LOCAL_MODEL = "base";
static const char *const tools_transcription_common_CONST_DEFAULT_LOCAL_STT_LANGUAGE = "en";
static const char *const tools_transcription_common_CONST_XAI_STT_MODEL_ENV = "STT_XAI_MODEL";
static const char *const tools_transcription_common_CONST_LEGACY_XAI_STT_MODEL = "grok-stt";
static const char *const tools_transcription_common_CONST_LOCAL_STT_COMMAND_ENV = "HERMES_LOCAL_STT_COMMAND";
static const char *const tools_transcription_common_CONST_LOCAL_STT_LANGUAGE_ENV = "HERMES_LOCAL_STT_LANGUAGE";

void tools_transcription_common_port_pending(void) { (void)0; }
