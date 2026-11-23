/*
 * messages.c — C home for upstream Hermes persistence/conversation/messages.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: persistence_conversation_messages_pending @ persistence/conversation/messages.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const persistence_conversation_messages_CONST_CONTENT_JSON_PREFIX = "\\x00json:";
static const char *const persistence_conversation_messages_CONST_DB_PERSISTED_MARKER_KEY = "_db_persisted";
static const char *const persistence_conversation_messages_CONST_QUEUED_PROMPT_METADATA_KEY = "_queued_prompt";
static const char *const persistence_conversation_messages_CONST_CONTENT_REFUSED_DISPLAY_KIND = "content_refused";
static const char *const persistence_conversation_messages_CONST_CONTENT_REFUSED_METADATA_KEY = "content_refused";
static const char *const persistence_conversation_messages_CONST_CONTENT_REFUSED_NOTICE = "This message was refused by the model's safety filter \\u2014 try a different query.";

void persistence_conversation_messages_port_pending(void) { (void)0; }
