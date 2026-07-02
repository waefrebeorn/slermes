/*
 * chat_render.h — Markdown/code message rendering for C11 desktop app
 *
 * Renders chat messages (markdown, code blocks, tool calls) into
 * a format suitable for display in the C11 UI layer.
 *
 * PoP: render_markdown       @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_code_block    @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_tool_call     @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_message       @ apps/desktop/src/app/chat/index.tsx
 */

#ifndef CHAT_RENDER_H
#define CHAT_RENDER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define CHAT_RENDER_MAX_MSG    4096
#define CHAT_RENDER_MAX_LINES  256
#define CHAT_RENDER_MAX_TOKENS 512
#define CHAT_RENDER_MAX_CODE_LANG 64

/* ── Render token types ────────────────────────────────────────────────── */
typedef enum {
    TOKEN_TEXT = 0,
    TOKEN_BOLD_START,
    TOKEN_BOLD_END,
    TOKEN_ITALIC_START,
    TOKEN_ITALIC_END,
    TOKEN_CODE_INLINE,
    TOKEN_CODE_BLOCK_START,
    TOKEN_CODE_BLOCK_END,
    TOKEN_CODE_LANG,
    TOKEN_LINK,
    TOKEN_LINK_TEXT,
    TOKEN_LIST_ITEM,
    TOKEN_HEADING,
    TOKEN_BLOCKQUOTE,
    TOKEN_NEWLINE,
    TOKEN_TOOL_CALL_START,
    TOKEN_TOOL_CALL_END,
    TOKEN_TOOL_NAME,
    TOKEN_TOOL_ARGS,
    TOKEN_TOOL_RESULT,
    TOKEN_THINKING_START,
    TOKEN_THINKING_END,
    /* Syntax highlighting tokens */
    TOKEN_KEYWORD,          /* Language keyword (if, for, return, etc.) */
    TOKEN_STRING,           /* String literal */
    TOKEN_COMMENT,          /* Comment */
    TOKEN_NUMBER,           /* Numeric literal */
    TOKEN_OPERATOR,         /* Operator (+, -, =, etc.) */
    TOKEN_TYPE,             /* Type name (int, char, bool, etc.) */
    TOKEN_FUNCTION,         /* Function call */
    TOKEN_VARIABLE,         /* Variable identifier */
    TOKEN_DECORATOR,        /* Decorator / annotation */
    TOKEN_PREPROCESSOR,     /* Preprocessor directive */
    TOKEN_BRACKET,          /* Brackets/parens */
    TOKEN_TOOL_RESULT_ERROR,/* Tool call error result */
    TOKEN_TOOL_RESULT_SUCCESS, /* Tool call success result */
} chat_token_type_t;

/* ── Render token ──────────────────────────────────────────────────────── */
typedef struct {
    chat_token_type_t type;
    char             *text;     /* owned, must be freed */
    char              lang[CHAT_RENDER_MAX_CODE_LANG]; /* for code blocks */
    int               heading_level; /* 1-6 for TOKEN_HEADING */
} chat_render_token_t;

/* ── Rendered message ──────────────────────────────────────────────────── */
typedef struct {
    chat_render_token_t *tokens;
    int                  token_count;
    int                  token_capacity;
    char                 role[16];  /* "user", "assistant", "system" */
    char                 raw[CHAT_RENDER_MAX_MSG];
} chat_rendered_msg_t;

/* ── Code highlight info ───────────────────────────────────────────────── */
typedef struct {
    char lang[CHAT_RENDER_MAX_CODE_LANG];
    int  line_count;
    bool highlighted;
} chat_code_info_t;

/* ── Rendering API ──────────────────────────────────────────────────────── */

/* PoP: render_message @ apps/desktop/src/app/chat/index.tsx */
/* Parse a raw message string into render tokens.
 * Returns a rendered message that must be freed with chat_render_free(). */
chat_rendered_msg_t *chat_render_message(const char *raw_text, const char *role);

/* PoP: render_markdown @ apps/desktop/src/app/chat/index.tsx */
/* Render markdown text into tokens (simplified markdown parser). */
chat_rendered_msg_t *chat_render_markdown(const char *md_text);

/* PoP: render_code_block @ apps/desktop/src/app/chat/index.tsx */
/* Render a code block with language info. */
chat_rendered_msg_t *chat_render_code_block(const char *code, const char *lang);

/* PoP: render_tool_call @ apps/desktop/src/app/chat/index.tsx */
/* Render a tool call with name, args, and result. */
chat_rendered_msg_t *chat_render_tool_call(const char *tool_name,
                                            const char *tool_args,
                                            const char *tool_result);

/* Free a rendered message and all its tokens. */
void chat_render_free(chat_rendered_msg_t *msg);

/* ── Token access ───────────────────────────────────────────────────────── */

int chat_render_token_count(const chat_rendered_msg_t *msg);
const chat_render_token_t *chat_render_get_token(const chat_rendered_msg_t *msg, int idx);

/* ── Plain text extraction ──────────────────────────────────────────────── */

/* Extract plain text from a rendered message (strips formatting). */
/* Caller must free the returned string. */
char *chat_render_plain_text(const chat_rendered_msg_t *msg);

/* ── Syntax highlighting ────────────────────────────────────────────────── */

/* PoP: highlight_code @ apps/desktop/src/app/chat/index.tsx */
/* Highlight code text with keyword-based syntax coloring. */
/* Returns a new rendered message with syntax tokens. */
chat_rendered_msg_t *chat_render_highlight(const char *code, const char *lang);

/* PoP: highlight_language_name @ apps/desktop/src/app/chat/index.tsx */
/* Normalize language name (e.g., "js" → "javascript"). */
const char *chat_render_language_normalize(const char *lang);

/* PoP: highlight_language_from_extension @ apps/desktop/src/app/chat/index.tsx */
/* Detect language from file extension (e.g., ".py" → "python"). */
const char *chat_render_language_from_filename(const char *filename);

/* ── Tool result rendering ───────────────────────────────────────────────── */

/* PoP: chat_render_tool_result @ apps/desktop/src/app/chat/index.tsx */
/* Render a tool call result with success/error distinction. */
chat_rendered_msg_t *chat_render_tool_result(const char *tool_name,
                                              const char *tool_args,
                                              const char *tool_result,
                                              bool is_error);

/* PoP: chat_render_thinking_block @ apps/desktop/src/app/chat/index.tsx */
/* Render a thinking/reasoning block. */
chat_rendered_msg_t *chat_render_thinking_block(const char *thinking_text,
                                                 const char *model_name);

/* PoP: chat_render_expandable @ apps/desktop/src/app/chat/index.tsx */
/* Render an expandable/collapsible section. */
chat_rendered_msg_t *chat_render_expandable(const char *title,
                                             const char *content,
                                             bool initially_expanded);

#ifdef __cplusplus
}
#endif

#endif /* CHAT_RENDER_H */
