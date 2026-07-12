/*
 * chat_render.c — Markdown/code message rendering for C11 desktop app
 *
 * Parses markdown text into render tokens for display.
 * Handles: bold, italic, inline code, code blocks, links, headings,
 * blockquotes, lists, tool calls, and thinking blocks.
 *
 * PoP: render_message       @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_markdown      @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_code_block   @ apps/desktop/src/app/chat/index.tsx
 * PoP: render_tool_call    @ apps/desktop/src/app/chat/index.tsx
 */

#include "chat_render.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Internal helpers ────────────────────────────────────────────────────── */

static chat_rendered_msg_t *msg_alloc(const char *role) {
    chat_rendered_msg_t *msg = calloc(1, sizeof(chat_rendered_msg_t));
    if (!msg) return NULL;
    msg->token_capacity = 64;
    msg->tokens = calloc(msg->token_capacity, sizeof(chat_render_token_t));
    if (!msg->tokens) { free(msg); return NULL; }
    if (role) strncpy(msg->role, role, sizeof(msg->role) - 1);
    return msg;
}

static bool msg_add_token(chat_rendered_msg_t *msg, chat_token_type_t type,
                        const char *text, int heading_level) {
    if (msg->token_count >= msg->token_capacity) {
        int new_cap = msg->token_capacity * 2;
        chat_render_token_t *new_tokens = realloc(msg->tokens,
                                                   new_cap * sizeof(chat_render_token_t));
        if (!new_tokens) return false;
        msg->tokens = new_tokens;
        msg->token_capacity = new_cap;
    }
    chat_render_token_t *tok = &msg->tokens[msg->token_count++];
    tok->type = type;
    tok->text = text ? strdup(text) : NULL;
    tok->lang[0] = '\0';
    tok->heading_level = heading_level;
    return true;
}

static bool parse_tool_call(const char *text, char **name_out, char **args_out, char **result_out) {
    /* Parse tool call format: ⟦tool:name|args...⟧ or similar */
    const char *start = strstr(text, "tool:");
    if (!start) return false;
    start += 5;
    const char *end = strchr(start, '|');
    if (end && name_out) {
        size_t len = (size_t)(end - start);
        *name_out = malloc(len + 1);
        strncpy(*name_out, start, len);
        (*name_out)[len] = '\0';
    }
    if (end && args_out) {
        const char *args_start = end + 1;
        const char *args_end = strstr(args_start, "⟧");
        if (!args_end) args_end = args_start + strlen(args_start);
        size_t len = (size_t)(args_end - args_start);
        *args_out = malloc(len + 1);
        strncpy(*args_out, args_start, len);
        (*args_out)[len] = '\0';
    }
    return true;
}

/* ── Markdown parser ────────────────────────────────────────────────────── */

static void parse_inline(chat_rendered_msg_t *msg, const char *start, size_t len) {
    /* Simple inline parser: handles **bold**, *italic*, `code`, [link](url) */
    size_t i = 0;
    while (i < len) {
        const char *s = start + i;

        if (i + 1 < len && s[0] == '*' && s[1] == '*') {
            /* Bold: **text** */
            const char *end = strstr(s + 2, "**");
            if (end && (size_t)(end - s) < len - i) {
                msg_add_token(msg, TOKEN_BOLD_START, NULL, 0);
                size_t tlen = (size_t)(end - s - 2);
                char *text = malloc(tlen + 1);
                strncpy(text, s + 2, tlen); text[tlen] = '\0';
                msg_add_token(msg, TOKEN_TEXT, text, 0);
                free(text);
                msg_add_token(msg, TOKEN_BOLD_END, NULL, 0);
                i += (size_t)(end - s) + 2;
                continue;
            }
        }

        if (s[0] == '*' && (i + 1 >= len || s[1] != '*')) {
            /* Italic: *text* */
            const char *end = strchr(s + 1, '*');
            if (end && (size_t)(end - s) < len - i) {
                msg_add_token(msg, TOKEN_ITALIC_START, NULL, 0);
                size_t tlen = (size_t)(end - s - 1);
                char *text = malloc(tlen + 1);
                strncpy(text, s + 1, tlen); text[tlen] = '\0';
                msg_add_token(msg, TOKEN_TEXT, text, 0);
                free(text);
                msg_add_token(msg, TOKEN_ITALIC_END, NULL, 0);
                i += (size_t)(end - s) + 1;
                continue;
            }
        }

        if (s[0] == '`') {
            /* Inline code: `text` */
            const char *end = strchr(s + 1, '`');
            if (end && (size_t)(end - s) < len - i) {
                size_t tlen = (size_t)(end - s - 1);
                char *text = malloc(tlen + 1);
                strncpy(text, s + 1, tlen); text[tlen] = '\0';
                msg_add_token(msg, TOKEN_CODE_INLINE, text, 0);
                free(text);
                i += (size_t)(end - s) + 1;
                continue;
            }
        }

        if (s[0] == '[') {
            /* Link: [text](url) */
            const char *close = strchr(s + 1, ']');
            if (close && close[1] == '(') {
                const char *url_end = strchr(close + 2, ')');
                if (url_end) {
                    size_t tlen = (size_t)(close - s - 1);
                    char *text = malloc(tlen + 1);
                    strncpy(text, s + 1, tlen); text[tlen] = '\0';
                    msg_add_token(msg, TOKEN_LINK_TEXT, text, 0);
                    free(text);
                    size_t ulen = (size_t)(url_end - close - 2);
                    char *url = malloc(ulen + 1);
                    strncpy(url, close + 2, ulen); url[ulen] = '\0';
                    msg_add_token(msg, TOKEN_LINK, url, 0);
                    free(url);
                    i += (size_t)(url_end - s) + 1;
                    continue;
                }
            }
        }

        /* Plain text: collect until next special char */
        size_t j = i + 1;
        while (j < len) {
            char c = start[j];
            if (c == '*' || c == '`' || c == '[' || c == '\n') break;
            j++;
        }
        size_t tlen = j - i;
        char *text = malloc(tlen + 1);
        strncpy(text, s, tlen); text[tlen] = '\0';
        msg_add_token(msg, TOKEN_TEXT, text, 0);
        free(text);
        i = j;
    }
}

/* PoP: render_markdown @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_markdown(const char *md_text) {
    if (!md_text) return NULL;

    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    strncpy(msg->raw, md_text, CHAT_RENDER_MAX_MSG - 1);

    size_t len = strlen(md_text);
    size_t i = 0;
    bool in_code_block = false;
    char code_lang[CHAT_RENDER_MAX_CODE_LANG] = "";
    size_t code_start = 0;

    while (i < len) {
        const char *line = md_text + i;
        size_t line_end = i;
        while (line_end < len && md_text[line_end] != '\n') line_end++;
        size_t line_len = line_end - i;

        /* Code block: ```lang ... ``` */
        if (line_len >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
            if (!in_code_block) {
                in_code_block = true;
                code_start = line_end + 1;
                /* Extract language */
                size_t lang_start = 3;
                while (lang_start < line_len && isspace((unsigned char)line[lang_start])) lang_start++;
                size_t lang_end = lang_start;
                while (lang_end < line_len && !isspace((unsigned char)line[lang_end])) lang_end++;
                size_t lang_len = lang_end - lang_start;
                if (lang_len > 0 && lang_len < CHAT_RENDER_MAX_CODE_LANG) {
                    strncpy(code_lang, line + lang_start, lang_len);
                    code_lang[lang_len] = '\0';
                }
                msg_add_token(msg, TOKEN_CODE_BLOCK_START, code_lang, 0);
            } else {
                /* End code block */
                in_code_block = false;
                if (line_end > code_start) {
                    size_t code_len = line_end - code_start;
                    char *code = malloc(code_len + 1);
                    strncpy(code, md_text + code_start, code_len);
                    code[code_len] = '\0';
                    msg_add_token(msg, TOKEN_CODE_INLINE, code, 0);
                    free(code);
                }
                msg_add_token(msg, TOKEN_CODE_BLOCK_END, NULL, 0);
            }
            i = line_end + 1;
            continue;
        }

        if (in_code_block) {
            i = line_end + 1;
            continue;
        }

        /* Headings: ## text */
        if (line[0] == '#') {
            int level = 0;
            while (level < 6 && level < (int)line_len && line[level] == '#') level++;
            if (level > 0 && level < (int)line_len && line[level] == ' ') {
                while (level < (int)line_len && isspace((unsigned char)line[level])) level++;
                msg_add_token(msg, TOKEN_HEADING, line + level, level);
                i = line_end + 1;
                continue;
            }
        }

        /* Blockquote: > text */
        if (line[0] == '>') {
            size_t start = 1;
            while (start < line_len && isspace((unsigned char)line[start])) start++;
            msg_add_token(msg, TOKEN_BLOCKQUOTE, line + start, 0);
            i = line_end + 1;
            continue;
        }

        /* List item: - text or * text or 1. text */
        if ((line[0] == '-' || line[0] == '*') && line_len > 1 && line[1] == ' ') {
            msg_add_token(msg, TOKEN_LIST_ITEM, line + 2, 0);
            i = line_end + 1;
            continue;
        }

        /* Empty line */
        if (line_len == 0) {
            msg_add_token(msg, TOKEN_NEWLINE, NULL, 0);
            i = line_end + 1;
            continue;
        }

        /* Regular paragraph text */
        parse_inline(msg, line, line_len);
        msg_add_token(msg, TOKEN_NEWLINE, NULL, 0);
        i = line_end + 1;
    }

    return msg;
}

/* PoP: render_message @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_message(const char *raw_text, const char *role) {
    if (!raw_text) return NULL;

    chat_rendered_msg_t *msg = msg_alloc(role);
    if (!msg) return NULL;

    strncpy(msg->raw, raw_text, CHAT_RENDER_MAX_MSG - 1);

    /* Check for thinking blocks: <think>...</think> */
    const char *think_start = strstr(raw_text, "<think>");
    const char *think_end   = strstr(raw_text, "</think>");

    if (think_start && think_end && think_end > think_start) {
        /* Render text before thinking */
        if (think_start > raw_text) {
            size_t pre_len = (size_t)(think_start - raw_text);
            char *pre = malloc(pre_len + 1);
            strncpy(pre, raw_text, pre_len); pre[pre_len] = '\0';
            parse_inline(msg, pre, pre_len);
            free(pre);
        }

        msg_add_token(msg, TOKEN_THINKING_START, NULL, 0);
        size_t think_len = (size_t)(think_end - think_start - 7);
        char *think = malloc(think_len + 1);
        strncpy(think, think_start + 7, think_len); think[think_len] = '\0';
        parse_inline(msg, think, think_len);
        free(think);
        msg_add_token(msg, TOKEN_THINKING_END, NULL, 0);

        /* Render text after thinking */
        const char *after = think_end + 8;
        if (*after) {
            parse_inline(msg, after, strlen(after));
        }
    } else {
        /* Regular message */
        parse_inline(msg, raw_text, strlen(raw_text));
    }

    return msg;
}

/* PoP: render_code_block @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_code_block(const char *code, const char *lang) {
    if (!code) return NULL;

    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    if (lang) {
        strncpy(msg->tokens[msg->token_count].lang, lang, CHAT_RENDER_MAX_CODE_LANG - 1);
    }
    msg_add_token(msg, TOKEN_CODE_BLOCK_START, lang, 0);
    msg_add_token(msg, TOKEN_CODE_INLINE, code, 0);
    msg_add_token(msg, TOKEN_CODE_BLOCK_END, NULL, 0);

    return msg;
}

/* PoP: render_tool_call @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_tool_call(const char *tool_name,
                                            const char *tool_args,
                                            const char *tool_result) {
    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    msg_add_token(msg, TOKEN_TOOL_CALL_START, NULL, 0);
    if (tool_name) msg_add_token(msg, TOKEN_TOOL_NAME, tool_name, 0);
    if (tool_args) msg_add_token(msg, TOKEN_TOOL_ARGS, tool_args, 0);
    if (tool_result) msg_add_token(msg, TOKEN_TOOL_RESULT, tool_result, 0);
    msg_add_token(msg, TOKEN_TOOL_CALL_END, NULL, 0);

    return msg;
}

void chat_render_free(chat_rendered_msg_t *msg) {
    if (!msg) return;
    for (int i = 0; i < msg->token_count; i++) {
        free(msg->tokens[i].text);
    }
    free(msg->tokens);
    free(msg);
}

int chat_render_token_count(const chat_rendered_msg_t *msg) {
    return msg ? msg->token_count : 0;
}

const chat_render_token_t *chat_render_get_token(const chat_rendered_msg_t *msg, int idx) {
    if (!msg || idx < 0 || idx >= msg->token_count) return NULL;
    return &msg->tokens[idx];
}

char *chat_render_plain_text(const chat_rendered_msg_t *msg) {
    if (!msg) return NULL;

    size_t total = 0;
    for (int i = 0; i < msg->token_count; i++) {
        if (msg->tokens[i].text) total += strlen(msg->tokens[i].text);
        total += 1; /* space/separator */
    }

    char *out = malloc(total + 1);
    if (!out) return NULL;
    out[0] = '\0';

    for (int i = 0; i < msg->token_count; i++) {
        chat_token_type_t t = msg->tokens[i].type;
        if (t == TOKEN_TEXT || t == TOKEN_CODE_INLINE || t == TOKEN_LINK_TEXT ||
            t == TOKEN_TOOL_NAME || t == TOKEN_TOOL_ARGS || t == TOKEN_TOOL_RESULT ||
            t == TOKEN_HEADING || t == TOKEN_BLOCKQUOTE || t == TOKEN_LIST_ITEM) {
            if (msg->tokens[i].text) strcat(out, msg->tokens[i].text);
        } else if (t == TOKEN_NEWLINE) {
            strcat(out, "\n");
        }
    }

    return out;
}

/* ── Syntax highlighting ──────────────────────────────────────────────────── */

/* Language keyword tables */
typedef struct {
    const char *name;
    const char **keywords;
    int          keyword_count;
    const char *single_comment;  /* e.g., "//" or "#" */
    const char *multi_comment_start;
    const char *multi_comment_end;
    const char *string_chars;    /* e.g., "\"'" */
    const char *type_keywords;  /* comma-separated type keywords */
} lang_def_t;

static const char *c_keywords[] = {
    "auto", "break", "case", "const", "continue", "default", "do", "else",
    "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", "NULL", "true", "false"
};

static const char *py_keywords[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
    "try", "while", "with", "yield"
};

static const char *js_keywords[] = {
    "break", "case", "catch", "class", "const", "continue", "debugger",
    "default", "delete", "do", "else", "export", "extends", "false",
    "finally", "for", "function", "if", "import", "in", "instanceof",
    "let", "new", "null", "return", "super", "switch", "this", "throw",
    "true", "try", "typeof", "var", "void", "while", "with", "yield",
    "async", "await", "of"
};

static const char *ts_keywords[] = {
    "abstract", "as", "asserts", "async", "await", "break", "case", "catch",
    "class", "const", "continue", "debugger", "declare", "default", "delete",
    "do", "else", "enum", "export", "extends", "false", "finally", "for",
    "function", "if", "implements", "import", "in", "instanceof", "interface",
    "keyof", "let", "module", "namespace", "new", "null", "of", "package",
    "private", "protected", "public", "readonly", "return", "static", "super",
    "switch", "this", "throw", "true", "try", "type", "typeof", "var",
    "void", "while", "with", "yield"
};

static const char *bash_keywords[] = {
    "if", "then", "else", "elif", "fi", "case", "esac", "for", "while",
    "until", "do", "done", "in", "function", "return", "exit", "break",
    "continue", "export", "readonly", "local", "declare", "typeset",
    "unset", "shift", "trap", "wait", "eval", "exec", "source"
};

static const char *json_keywords[] = { "true", "false", "null" };

static const char *yaml_keywords[] = { "true", "false", "null", "yes", "no" };

static const lang_def_t lang_defs[] = {
    { "c", c_keywords, sizeof(c_keywords)/sizeof(c_keywords[0]),
      "//", "/*", "*/", "\"'", "int,char,float,double,void,short,long,unsigned,signed,const,static,extern,size_t,bool" },
    { "python", py_keywords, sizeof(py_keywords)/sizeof(py_keywords[0]),
      "#", NULL, NULL, "\"'", "int,float,str,bool,list,dict,tuple,set,None" },
    { "javascript", js_keywords, sizeof(js_keywords)/sizeof(js_keywords[0]),
      "//", "/*", "*/", "\"'`", "number,string,boolean,object,undefined,null,Array,Promise" },
    { "typescript", ts_keywords, sizeof(ts_keywords)/sizeof(ts_keywords[0]),
      "//", "/*", "*/", "\"'`", "number,string,boolean,any,void,never,unknown,object,Array,Promise" },
    { "bash", bash_keywords, sizeof(bash_keywords)/sizeof(bash_keywords[0]),
      "#", NULL, NULL, "\"'", NULL },
    { "sh", bash_keywords, sizeof(bash_keywords)/sizeof(bash_keywords[0]),
      "#", NULL, NULL, "\"'", NULL },
    { "json", json_keywords, sizeof(json_keywords)/sizeof(json_keywords[0]),
      NULL, NULL, NULL, "\"'", "number,boolean,object,array,null" },
    { "yaml", yaml_keywords, sizeof(yaml_keywords)/sizeof(yaml_keywords[0]),
      "#", NULL, NULL, "\"'", NULL },
    { "toml", yaml_keywords, sizeof(yaml_keywords)/sizeof(yaml_keywords[0]),
      "#", NULL, NULL, "\"'", NULL },
    { "markdown", NULL, 0,
      NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL }
};

static const lang_def_t *find_lang_def(const char *lang) {
    if (!lang || !*lang) return NULL;
    for (int i = 0; lang_defs[i].name; i++) {
        if (strcasecmp(lang, lang_defs[i].name) == 0)
            return &lang_defs[i];
    }
    return NULL;
}

/* PoP: highlight_language_name @ apps/desktop/src/app/chat/index.tsx */
const char *chat_render_language_normalize(const char *lang) {
    if (!lang) return "text";
    if (strcasecmp(lang, "js") == 0 || strcasecmp(lang, "jsx") == 0) return "javascript";
    if (strcasecmp(lang, "ts") == 0 || strcasecmp(lang, "tsx") == 0) return "typescript";
    if (strcasecmp(lang, "py") == 0 || strcasecmp(lang, "python") == 0) return "python";
    if (strcasecmp(lang, "sh") == 0 || strcasecmp(lang, "shell") == 0) return "bash";
    if (strcasecmp(lang, "yml") == 0) return "yaml";
    if (strcasecmp(lang, "md") == 0) return "markdown";
    if (strcasecmp(lang, "c") == 0 || strcasecmp(lang, "h") == 0) return "c";
    if (strcasecmp(lang, "json") == 0) return "json";
    if (strcasecmp(lang, "toml") == 0) return "toml";
    return lang;
}

/* PoP: highlight_language_from_extension @ apps/desktop/src/app/chat/index.tsx */
const char *chat_render_language_from_filename(const char *filename) {
    if (!filename) return "text";
    const char *ext = strrchr(filename, '.');
    if (!ext) return "text";
    ext++;
    if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "h") == 0) return "c";
    if (strcasecmp(ext, "py") == 0) return "python";
    if (strcasecmp(ext, "js") == 0) return "javascript";
    if (strcasecmp(ext, "ts") == 0) return "typescript";
    if (strcasecmp(ext, "jsx") == 0) return "javascript";
    if (strcasecmp(ext, "tsx") == 0) return "typescript";
    if (strcasecmp(ext, "sh") == 0 || strcasecmp(ext, "bash") == 0) return "bash";
    if (strcasecmp(ext, "json") == 0) return "json";
    if (strcasecmp(ext, "yaml") == 0 || strcasecmp(ext, "yml") == 0) return "yaml";
    if (strcasecmp(ext, "toml") == 0) return "toml";
    if (strcasecmp(ext, "md") == 0) return "markdown";
    return "text";
}

static bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool is_keyword(const char *word, int word_len, const lang_def_t *lang) {
    if (!lang || !lang->keywords) return false;
    for (int i = 0; i < lang->keyword_count; i++) {
        if (strlen(lang->keywords[i]) == (size_t)word_len &&
            strncasecmp(word, lang->keywords[i], word_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_type_word(const char *word, int word_len, const lang_def_t *lang) {
    if (!lang || !lang->type_keywords) return false;
    /* Copy type keywords to buffer and tokenize */
    char buf[1024];
    size_t tlen = strlen(lang->type_keywords);
    if (tlen >= sizeof(buf)) tlen = sizeof(buf) - 1;
    strncpy(buf, lang->type_keywords, tlen);
    buf[tlen] = '\0';

    char *saveptr;
    char *tok = strtok_r(buf, ",", &saveptr);
    while (tok) {
        /* Trim whitespace */
        while (*tok == ' ') tok++;
        if (strlen(tok) == (size_t)word_len && strncasecmp(word, tok, word_len) == 0)
            return true;
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return false;
}

static void highlight_line(chat_rendered_msg_t *msg, const char *line, size_t line_len,
                           const lang_def_t *lang, bool *in_multiline_comment) {
    if (!lang) {
        /* No language — just add as plain text */
        char *text = malloc(line_len + 1);
        strncpy(text, line, line_len); text[line_len] = '\0';
        msg_add_token(msg, TOKEN_TEXT, text, 0);
        free(text);
        return;
    }

    size_t i = 0;
    bool in_mc = *in_multiline_comment;

    while (i < line_len) {
        /* Multi-line comment tracking */
        if (lang->multi_comment_start && !in_mc) {
            size_t mclen = strlen(lang->multi_comment_start);
            if (i + mclen <= line_len && strncmp(line + i, lang->multi_comment_start, mclen) == 0) {
                in_mc = true;
                /* Find end of multi-line comment */
                const char *end = strstr(line + i + mclen, lang->multi_comment_end);
                size_t mc_end = end ? (size_t)(end - (line + i)) + strlen(lang->multi_comment_end) : line_len;
                size_t mc_len = mc_end - i;
                char *comment = malloc(mc_len + 1);
                strncpy(comment, line + i, mc_len); comment[mc_len] = '\0';
                msg_add_token(msg, TOKEN_COMMENT, comment, 0);
                free(comment);
                i = mc_end;
                if (!end) *in_multiline_comment = true;
                continue;
            }
        }

        if (in_mc && lang->multi_comment_end) {
            size_t mcelen = strlen(lang->multi_comment_end);
            const char *end = strstr(line + i, lang->multi_comment_end);
            if (end) {
                size_t mc_len = (size_t)(end - (line + i)) + mcelen;
                char *comment = malloc(mc_len + 1);
                strncpy(comment, line + i, mc_len); comment[mc_len] = '\0';
                msg_add_token(msg, TOKEN_COMMENT, comment, 0);
                free(comment);
                i += mc_len;
                in_mc = false;
                *in_multiline_comment = false;
                continue;
            } else {
                /* Rest of line is comment */
                size_t mc_len = line_len - i;
                char *comment = malloc(mc_len + 1);
                strncpy(comment, line + i, mc_len); comment[mc_len] = '\0';
                msg_add_token(msg, TOKEN_COMMENT, comment, 0);
                free(comment);
                i = line_len;
                continue;
            }
        }

        /* Single-line comment */
        if (lang->single_comment && !in_mc) {
            size_t sclen = strlen(lang->single_comment);
            if (i + sclen <= line_len && strncmp(line + i, lang->single_comment, sclen) == 0) {
                size_t comment_len = line_len - i;
                char *comment = malloc(comment_len + 1);
                strncpy(comment, line + i, comment_len); comment[comment_len] = '\0';
                msg_add_token(msg, TOKEN_COMMENT, comment, 0);
                free(comment);
                i = line_len;
                continue;
            }
        }

        /* String literals */
        if (lang->string_chars) {
            const char *sc = lang->string_chars;
            bool is_string = false;
            for (; *sc; sc++) {
                if (line[i] == *sc) {
                    is_string = true;
                    break;
                }
            }
            if (is_string) {
                char quote = line[i];
                size_t j = i + 1;
                while (j < line_len && line[j] != quote) {
                    if (line[j] == '\\') j++; /* skip escaped char */
                    j++;
                }
                if (j < line_len) j++; /* include closing quote */
                size_t str_len = j - i;
                char *str = malloc(str_len + 1);
                strncpy(str, line + i, str_len); str[str_len] = '\0';
                msg_add_token(msg, TOKEN_STRING, str, 0);
                free(str);
                i = j;
                continue;
            }
        }

        /* Numbers */
        if (line[i] >= '0' && line[i] <= '9') {
            size_t j = i;
            while (j < line_len && ((line[j] >= '0' && line[j] <= '9') || line[j] == '.' ||
                   line[j] == 'x' || line[j] == 'X' || line[j] == 'a' || line[j] == 'b' ||
                   line[j] == 'B' || line[j] == 'c' || line[j] == 'C' || line[j] == 'd' ||
                   line[j] == 'D' || line[j] == 'e' || line[j] == 'E' || line[j] == 'f' ||
                   line[j] == 'F' || line[j] == 'u' || line[j] == 'U' || line[j] == 'l' ||
                   line[j] == 'L')) {
                j++;
            }
            size_t num_len = j - i;
            char *num = malloc(num_len + 1);
            strncpy(num, line + i, num_len); num[num_len] = '\0';
            msg_add_token(msg, TOKEN_NUMBER, num, 0);
            free(num);
            i = j;
            continue;
        }

        /* Preprocessor directives (#) */
        if (line[i] == '#' && lang->single_comment == NULL) {
            /* Check if it's a preprocessor directive (C-like) */
            size_t j = i + 1;
            while (j < line_len && (line[j] == ' ' || line[j] == '\t')) j++;
            if (j < line_len && is_word_char(line[j])) {
                /* Find end of directive word */
                size_t k = j;
                while (k < line_len && is_word_char(line[k])) k++;
                size_t pp_len = k - i;
                char *pp = malloc(pp_len + 1);
                strncpy(pp, line + i, pp_len); pp[pp_len] = '\0';
                msg_add_token(msg, TOKEN_PREPROCESSOR, pp, 0);
                free(pp);
                i = k;
                continue;
            }
        }

        /* Words (identifiers/keywords) */
        if (is_word_char(line[i])) {
            size_t j = i;
            while (j < line_len && is_word_char(line[j])) j++;
            size_t word_len = j - i;
            char *word = malloc(word_len + 1);
            strncpy(word, line + i, word_len); word[word_len] = '\0';

            /* Check if followed by ( → function call */
            size_t k = j;
            while (k < line_len && (line[k] == ' ' || line[k] == '\t')) k++;

            if (is_keyword(word, word_len, lang)) {
                msg_add_token(msg, TOKEN_KEYWORD, word, 0);
            } else if (is_type_word(word, word_len, lang)) {
                msg_add_token(msg, TOKEN_TYPE, word, 0);
            } else if (k < line_len && line[k] == '(') {
                msg_add_token(msg, TOKEN_FUNCTION, word, 0);
            } else {
                msg_add_token(msg, TOKEN_VARIABLE, word, 0);
            }
            free(word);
            i = j;
            continue;
        }

        /* Operators */
        if (strchr("+-*/%=<>!&|^~?:.,;{}[]()", line[i])) {
            size_t j = i + 1;
            /* Multi-char operators */
            if (j < line_len && (line[i] == '=' || line[j] == '=' ||
                line[i] == '+' || line[i] == '-' || line[i] == '&' ||
                line[i] == '|' || line[i] == '<' || line[i] == '>')) {
                j = i + 2;
            }
            size_t op_len = j - i;
            char *op = malloc(op_len + 1);
            strncpy(op, line + i, op_len); op[op_len] = '\0';
            msg_add_token(msg, TOKEN_OPERATOR, op, 0);
            free(op);
            i = j;
            continue;
        }

        /* Whitespace and other chars */
        {
            size_t j = i + 1;
            /* Collect consecutive whitespace */
            while (j < line_len && (line[j] == ' ' || line[j] == '\t')) j++;
            size_t ws_len = j - i;
            char *ws = malloc(ws_len + 1);
            strncpy(ws, line + i, ws_len); ws[ws_len] = '\0';
            msg_add_token(msg, TOKEN_TEXT, ws, 0);
            free(ws);
            i = j;
        }
    }
}

/* PoP: highlight_code @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_highlight(const char *code, const char *lang) {
    if (!code) return NULL;

    const char *norm_lang = chat_render_language_normalize(lang);
    const lang_def_t *lang_def = find_lang_def(norm_lang);

    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    msg_add_token(msg, TOKEN_CODE_BLOCK_START, norm_lang, 0);

    size_t len = strlen(code);
    size_t i = 0;
    bool in_multiline_comment = false;

    while (i < len) {
        /* Find end of line */
        size_t line_end = i;
        while (line_end < len && code[line_end] != '\n') line_end++;
        size_t line_len = line_end - i;

        /* Handle preprocessor directives at line start for C */
        if (lang_def && lang_def->single_comment == NULL && line_len > 0 && code[i] == '#') {
            /* Check if it's a preprocessor directive */
            size_t k = i + 1;
            while (k < line_end && (code[k] == ' ' || code[k] == '\t')) k++;
            if (k < line_end && is_word_char(code[k])) {
                /* Find end of directive */
                size_t de = k;
                while (de < line_end && is_word_char(code[de])) de++;
                size_t pp_len = de - i;
                char *pp = malloc(pp_len + 1);
                strncpy(pp, code + i, pp_len); pp[pp_len] = '\0';
                msg_add_token(msg, TOKEN_PREPROCESSOR, pp, 0);
                free(pp);
                /* Continue with rest of line as normal */
                i = de;
                if (i < line_end) {
                    size_t rest_len = line_end - i;
                    char *rest = malloc(rest_len + 1);
                    strncpy(rest, code + i, rest_len); rest[rest_len] = '\0';
                    highlight_line(msg, rest, line_len - (de - i), lang_def, &in_multiline_comment);
                    free(rest);
                }
            } else {
                highlight_line(msg, code + i, line_len, lang_def, &in_multiline_comment);
            }
        } else {
            highlight_line(msg, code + i, line_len, lang_def, &in_multiline_comment);
        }

        /* Add newline */
        if (line_end < len) {
            msg_add_token(msg, TOKEN_NEWLINE, NULL, 0);
            i = line_end + 1;
        } else {
            i = line_end;
        }
    }

    msg_add_token(msg, TOKEN_CODE_BLOCK_END, NULL, 0);
    return msg;
}

/* ── Tool result rendering ───────────────────────────────────────────────── */

/* PoP: chat_render_tool_result @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_tool_result(const char *tool_name,
                                              const char *tool_args,
                                              const char *tool_result,
                                              bool is_error) {
    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    msg_add_token(msg, TOKEN_TOOL_CALL_START, NULL, 0);
    if (tool_name) msg_add_token(msg, TOKEN_TOOL_NAME, tool_name, 0);
    if (tool_args) msg_add_token(msg, TOKEN_TOOL_ARGS, tool_args, 0);

    if (tool_result) {
        if (is_error) {
            msg_add_token(msg, TOKEN_TOOL_RESULT_ERROR, tool_result, 0);
        } else {
            msg_add_token(msg, TOKEN_TOOL_RESULT_SUCCESS, tool_result, 0);
        }
    }
    msg_add_token(msg, TOKEN_TOOL_CALL_END, NULL, 0);

    return msg;
}

/* PoP: chat_render_thinking_block @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_thinking_block(const char *thinking_text,
                                                 const char *model_name) {
    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    msg_add_token(msg, TOKEN_THINKING_START, NULL, 0);

    /* Optional model name header */
    if (model_name) {
        char header[512];
        snprintf(header, sizeof(header), "Reasoning (%s):\n", model_name);
        msg_add_token(msg, TOKEN_TEXT, header, 0);
    }

    if (thinking_text) {
        /* Render thinking text as markdown */
        chat_rendered_msg_t *thinking_md = chat_render_markdown(thinking_text);
        if (thinking_md) {
            /* Append tokens from thinking markdown */
            for (int i = 0; i < thinking_md->token_count; i++) {
                msg_add_token(msg, thinking_md->tokens[i].type,
                              thinking_md->tokens[i].text,
                              thinking_md->tokens[i].heading_level);
            }
            /* Free the source message (but not the tokens we just copied) */
            free(thinking_md->tokens);
            free(thinking_md);
        }
    }

    msg_add_token(msg, TOKEN_THINKING_END, NULL, 0);
    return msg;
}

/* PoP: chat_render_expandable @ apps/desktop/src/app/chat/index.tsx */
chat_rendered_msg_t *chat_render_expandable(const char *title,
                                             const char *content,
                                             bool initially_expanded) {
    chat_rendered_msg_t *msg = msg_alloc("assistant");
    if (!msg) return NULL;

    /* Title as heading */
    if (title) {
        char expand_marker[256];
        snprintf(expand_marker, sizeof(expand_marker), "%s %s",
                 initially_expanded ? "▼" : "▶", title);
        msg_add_token(msg, TOKEN_HEADING, expand_marker, 3);
    }

    if (initially_expanded && content) {
        /* Render content as markdown */
        chat_rendered_msg_t *content_md = chat_render_markdown(content);
        if (content_md) {
            for (int i = 0; i < content_md->token_count; i++) {
                msg_add_token(msg, content_md->tokens[i].type,
                              content_md->tokens[i].text,
                              content_md->tokens[i].heading_level);
            }
            free(content_md->tokens);
            free(content_md);
        }
    }

    return msg;
}
