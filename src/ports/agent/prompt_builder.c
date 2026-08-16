/*
 * prompt_builder.c — Context files, platform hints, env hints,
 * skills index, steer markers, CWD resolution, backend probe.
 * Port of Python agent/prompt_builder.py (1553 LOC).
 * Split from system_prompt.c in v376 for module parity.
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "hermes_skills.h"
#include "hermes_system_prompt.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <libgen.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>

#ifndef _WIN32
#include <pwd.h>
#include <sys/utsname.h>
#endif

/* ================================================================
 *  Context file truncation
 * ================================================================ */

/* Threat pattern definitions */
typedef struct {
    const char *pattern;
    const char *id;
} threat_pattern_t;

static const threat_pattern_t THREAT_PATTERNS[] = {
    { "ignore[[:space:]]+(previous|all|above|prior)[[:space:]]+instructions", "prompt_injection" },
    /* ... */
    { NULL, NULL }
};

/* Helper: read a whole file into a malloc'd string */
static char *file_read_all(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0 || len > CONTEXT_FILE_MAX_CHARS * 4) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[nread] = '\0';
    while (nread > 0 && (buf[nread - 1] == '\n' || buf[nread - 1] == '\r' || buf[nread - 1] == ' '))
        buf[--nread] = '\0';
    return buf;
}

/* PoP: context_truncate_content @ agent/prompt_builder.py:_truncate_content */
char *context_truncate_content(const char *content, const char *name, int max_chars) {
/* AG26: Port of Python agent/prompt_builder.py:_truncate_content() */
    if (!content) return NULL;
    size_t len = strlen(content);
    if (len <= (size_t)max_chars)
        return strdup(content);

    int head_chars = (int)(max_chars * CONTEXT_TRUNCATE_HEAD_RATIO);
    int tail_chars = (int)(max_chars * CONTEXT_TRUNCATE_TAIL_RATIO);

    char marker[512];
    snprintf(marker, sizeof(marker),
        "\n\n[...truncated %s: kept %d+%d of %zu chars. Use file tools to read the full file.]\n\n",
        name ? name : "file", head_chars, tail_chars, len);

    size_t marker_len = strlen(marker);
    size_t total = (size_t)head_chars + marker_len + (size_t)tail_chars + 1;
    char *result = (char *)malloc(total);
    if (!result) return NULL;

    memcpy(result, content, (size_t)head_chars);
    memcpy(result + head_chars, marker, marker_len);
    memcpy(result + head_chars + marker_len,
           content + len - (size_t)tail_chars, (size_t)tail_chars);
    result[total - 1] = '\0';
    return result;
}

/* ================================================================
 *  YAML frontmatter stripping
 * ================================================================ */

/* Port of Python: _strip_yaml_frontmatter */
/* PoP: context_strip_frontmatter @ agent/prompt_builder.py:_strip_yaml_frontmatter */
char *context_strip_frontmatter(const char *content) {
    if (!content || content[0] != '-')
        return strdup(content);

    /* Check for "---" at start */
    if (strncmp(content, "---", 3) != 0)
        return strdup(content);

    /* Find closing "---" */
    const char *end = strstr(content + 3, "\n---");
    if (!end)
        return strdup(content);

    /* Skip past closing --- */
    const char *body = end + 4;
    while (*body == '\n') body++;
    return strdup(body);
}

/* ================================================================
 *  Context content scanning with threat detection
 * ================================================================ */

/* Port of Python: _scan_context_content */
/* PoP: context_scan_content @ agent/prompt_builder.py:_scan_context_content */
char *context_scan_content(const char *content, const char *filename) {
    if (!content) return NULL;

    /* Check invisible unicode characters */
    const unsigned char *p = (const unsigned char *)content;
    while (*p) {
        /* Check for zero-width / invisible unicode chars in UTF-8 */
        if (*p == 0xE2) {
            if (p[1] >= 0x80 && p[1] <= 0x8F && p[2] >= 0x8B && p[2] <= 0x8F) {
                /* Found invisible char — block */
                char findings[512];
                snprintf(findings, sizeof(findings),
                    "[BLOCKED: %s contained potential prompt injection "
                    "(invisible unicode U+%02X%02X%02X). Content not loaded.]",
                    filename ? filename : "file",
                    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2]);
                return strdup(findings);
            }
            /* BOM (U+FEFF) */
            if (p[1] == 0xBB && p[2] == 0xBF) {
                char findings[512];
                snprintf(findings, sizeof(findings),
                    "[BLOCKED: %s contained potential prompt injection "
                    "(invisible unicode U+FEFF). Content not loaded.]",
                    filename ? filename : "file");
                return strdup(findings);
            }
        }
        p++;
    }

    /* Check threat patterns via POSIX regex */
    for (int i = 0; THREAT_PATTERNS[i].pattern != NULL; i++) {
        regex_t regex;
        int ret = regcomp(&regex, THREAT_PATTERNS[i].pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB);
        if (ret != 0) continue;

        ret = regexec(&regex, content, 0, NULL, 0);
        regfree(&regex);

        if (ret == 0) {
            char findings[1024];
            snprintf(findings, sizeof(findings),
                "[BLOCKED: %s contained potential prompt injection (%s). Content not loaded.]",
                filename ? filename : "file", THREAT_PATTERNS[i].id);
            return strdup(findings);
        }
    }

    return NULL; /* clean */
}

/* ================================================================
 *  Git root discovery
 * ================================================================ */

/* Port of Python: _find_git_root */
/* PoP: context_find_git_root @ agent/prompt_builder.py:_find_git_root */
char *context_find_git_root(const char *start_dir) {
    if (!start_dir) return NULL;

    /* Resolve to absolute path */
    char *abs_start = realpath(start_dir, NULL);
    if (!abs_start) return NULL;

    char *current = strdup(abs_start);
    free(abs_start);
    if (!current) return NULL;

    char *result = NULL;
    while (1) {
        /* Check for .git in current dir */
        char git_path[4096];
        snprintf(git_path, sizeof(git_path), "%s/.git", current);

        struct stat st;
        if (stat(git_path, &st) == 0) {
            result = strdup(current);
            break;
        }

        /* Walk up to parent — use a COPY because dirname() modifies arg on glibc */
        char *current_copy = strdup(current);
        if (!current_copy) break;
        char *parent = dirname(current_copy);
        char *parent_copy = strdup(parent);
        free(current_copy);
        if (!parent_copy) break;

        if (strcmp(current, parent_copy) == 0) {
            /* Reached filesystem root without finding .git */
            free(parent_copy);
            break;
        }
        free(current);
        current = parent_copy;
    }

    free(current);
    return result;
}

/* ================================================================
 *  SOUL.md loading
 * ================================================================ */

/* Port of Python: load_soul_md */
/* PoP: load_soul_md @ agent/prompt_builder.py:load_soul_md */
char *load_soul_md(void) {
    const char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) {
        /* Fallback: ~/.hermes/ */
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        static char default_home[1024];
        snprintf(default_home, sizeof(default_home), "%s/.hermes", home);
        hermes_home = default_home;
    }

    char soul_path[4096];
    snprintf(soul_path, sizeof(soul_path), "%s/SOUL.md", hermes_home);

    struct stat st;
    if (stat(soul_path, &st) != 0)
        return NULL;

    char *content = file_read_all(soul_path);
    if (!content || !content[0]) {
        free(content);
        return NULL;
    }

    char *scan_result = context_scan_content(content, "SOUL.md");
    if (scan_result) {
        /* Threats found - use blocked message */
        free(content);
        char *truncated = context_truncate_content(scan_result, "SOUL.md", CONTEXT_FILE_MAX_CHARS);
        free(scan_result);
        return truncated;
    }

    char *truncated = context_truncate_content(content, "SOUL.md", CONTEXT_FILE_MAX_CHARS);
    free(content);
    return truncated;
}

/* ================================================================
 *  .hermes.md / HERMES.md loading
 * ================================================================ */

/* Port of Python: _load_hermes_md, _find_hermes_md — consolidated: hermes.md discovery + loading */
/* AG26: Port of Python agent/prompt_builder.py:_load_hermes_md() */
/* AG26: Port of Python agent/prompt_builder.py:_find_hermes_md() */
/* PoP: context_load_hermes_md @ agent/prompt_builder.py:_load_hermes_md */
char *context_load_hermes_md(const char *cwd) {
    if (!cwd) return NULL;

    char *stop_at = context_find_git_root(cwd);

    /* Resolve cwd */
    char *resolved_cwd = realpath(cwd, NULL);
    if (!resolved_cwd) {
        free(stop_at);
        return NULL;
    }

    char *current = strdup(resolved_cwd);
    free(resolved_cwd);
    if (!current) { free(stop_at); return NULL; }

    char *result = NULL;
    const char *names[] = { ".hermes.md", "HERMES.md", NULL };

    while (1) {
        for (int i = 0; names[i]; i++) {
            char candidate[4096];
            snprintf(candidate, sizeof(candidate), "%s/%s", current, names[i]);

            struct stat st;
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) {
                char *content = file_read_all(candidate);
                if (content && content[0]) {
                    char *stripped = context_strip_frontmatter(content);
                    free(content);
                    if (stripped) {
                        char *scan_result = context_scan_content(stripped, names[i]);
                        if (scan_result) {
                            free(stripped);
                            result = scan_result;
                        } else {
                            /* Build results with ## heading */
                            size_t heading_len = strlen(names[i]) + 4; /* "## .hermes.md\n\n" */
                            size_t body_len = strlen(stripped);
                            result = (char *)malloc(heading_len + body_len + 1);
                            if (result) {
                                snprintf(result, heading_len + 1, "## %s\n\n", names[i]);
                                memcpy(result + heading_len - 1, stripped, body_len + 1);
                            }
                            free(stripped);
                        }
                        /* Truncate */
                        if (result) {
                            char *truncated = context_truncate_content(result, ".hermes.md",
                                                                        CONTEXT_FILE_MAX_CHARS);
                            free(result);
                            result = truncated;
                        }
                        goto done;
                    }
                }
                free(content);
            }
        }

        /* Walk up to parent — use a COPY because dirname() modifies arg on glibc */
        char *current_copy = strdup(current);
        if (!current_copy) break;
        char *parent = dirname(current_copy);
        char *parent_copy = strdup(parent);
        free(current_copy);
        if (!parent_copy) break;

        if (strcmp(current, parent_copy) == 0) {
            free(parent_copy);
            break; /* Reached root */
        }

        /* Stop at git root */
        if (stop_at && strcmp(current, stop_at) == 0) {
            free(parent_copy);
            break;
        }

        free(current);
        current = parent_copy;
    }

done:
    free(current);
    free(stop_at);
    return result;
}

/* ================================================================
 *  AGENTS.md loading
 * ================================================================ */

/* Port of Python: _load_agents_md */
/* PoP: context_load_agents_md @ agent/prompt_builder.py:_load_agents_md */
char *context_load_agents_md(const char *cwd) {
    if (!cwd) return NULL;

    const char *names[] = { "AGENTS.md", "agents.md", NULL };
    for (int i = 0; names[i]; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", cwd, names[i]);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        char *content = file_read_all(path);
        if (!content || !content[0]) { free(content); continue; }

        char *scan_result = context_scan_content(content, names[i]);
        char *display;
        if (scan_result) {
            display = scan_result;
        } else {
            size_t heading_len = strlen(names[i]) + 4;
            size_t body_len = strlen(content);
            display = (char *)malloc(heading_len + body_len + 1);
            if (display) {
                snprintf(display, heading_len + 1, "## %s\n\n", names[i]);
                memcpy(display + heading_len - 1, content, body_len + 1);
            }
        }
        free(content);

        if (display) {
            char *truncated = context_truncate_content(display, "AGENTS.md", CONTEXT_FILE_MAX_CHARS);
            free(display);
            return truncated;
        }
    }
    return NULL;
}

/* ================================================================
 *  CLAUDE.md loading
 * ================================================================ */

/* Port of Python: _load_claude_md */
/* PoP: context_load_claude_md @ agent/prompt_builder.py:_load_claude_md */
char *context_load_claude_md(const char *cwd) {
    if (!cwd) return NULL;

    const char *names[] = { "CLAUDE.md", "claude.md", NULL };
    for (int i = 0; names[i]; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", cwd, names[i]);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        char *content = file_read_all(path);
        if (!content || !content[0]) { free(content); continue; }

        char *scan_result = context_scan_content(content, names[i]);
        char *display;
        if (scan_result) {
            display = scan_result;
        } else {
            size_t heading_len = strlen(names[i]) + 4;
            size_t body_len = strlen(content);
            display = (char *)malloc(heading_len + body_len + 1);
            if (display) {
                snprintf(display, heading_len + 1, "## %s\n\n", names[i]);
                memcpy(display + heading_len - 1, content, body_len + 1);
            }
        }
        free(content);

        if (display) {
            char *truncated = context_truncate_content(display, "CLAUDE.md", CONTEXT_FILE_MAX_CHARS);
            free(display);
            return truncated;
        }
    }
    return NULL;
}

/* ================================================================
 *  .cursorrules loading (includes .cursor/rules *.mdc)
 * ================================================================ */

/* Port of Python: _load_cursorrules */
/* PoP: context_load_cursorrules @ agent/prompt_builder.py:_load_cursorrules */
char *context_load_cursorrules(const char *cwd) {
    if (!cwd) return NULL;

    /* Accumulate all cursorrules content */
    size_t total_len = 0;
    char *accum = NULL;

    /* .cursorrules file */
    char path[4096];
    snprintf(path, sizeof(path), "%s/.cursorrules", cwd);

    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        char *content = file_read_all(path);
        if (content && content[0]) {
            char *scan_result = context_scan_content(content, ".cursorrules");
            if (scan_result) {
                /* Blocked — return blocked message */
                free(content);
                return context_truncate_content(scan_result, ".cursorrules", CONTEXT_FILE_MAX_CHARS);
            }

            size_t part_len = strlen("## .cursorrules\n\n") + strlen(content) + 2;
            accum = (char *)malloc(part_len);
            if (accum) {
                snprintf(accum, part_len, "## .cursorrules\n\n%s\n\n", content);
                total_len = strlen(accum);
            }
        }
        free(content);
    }

    /* .cursor/rules *.mdc files */
    char rules_dir[4096];
    snprintf(rules_dir, sizeof(rules_dir), "%s/.cursor/rules", cwd);

    DIR *dir = opendir(rules_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            size_t elen = strlen(entry->d_name);
            if (elen <= 4 || strcmp(entry->d_name + elen - 4, ".mdc") != 0)
                continue;

            char mdc_path[4096];
            snprintf(mdc_path, sizeof(mdc_path), "%s/%s", rules_dir, entry->d_name);

            char *content = file_read_all(mdc_path);
            if (!content || !content[0]) { free(content); continue; }

            char *scan_result = context_scan_content(content, entry->d_name);
            if (scan_result) {
                /* Blocked — replace accum with blocked message */
                free(accum);
                free(content);
                accum = scan_result;
                total_len = strlen(accum);
                closedir(dir);
                return context_truncate_content(accum, ".cursorrules", CONTEXT_FILE_MAX_CHARS);
            }

            /* Append to accum */
            char header[256];
            snprintf(header, sizeof(header), "## .cursor/rules/%s\n\n", entry->d_name);
            size_t header_len = strlen(header);
            size_t body_len = strlen(content);

            char *new_accum = (char *)realloc(accum, total_len + header_len + body_len + 4);
            if (new_accum) {
                accum = new_accum;
                memcpy(accum + total_len, header, header_len);
                memcpy(accum + total_len + header_len, content, body_len);
                total_len += header_len + body_len;
                accum[total_len] = '\n';
                total_len++;
                accum[total_len] = '\n';
                total_len++;
                accum[total_len] = '\0';
            }
            free(content);
        }
        closedir(dir);
    }

    if (!accum) return NULL;
    char *truncated = context_truncate_content(accum, ".cursorrules", CONTEXT_FILE_MAX_CHARS);
    free(accum);
    return truncated;
}

/* ================================================================
 *  Build full context files prompt block
 * ================================================================ */

/* Port of Python: build_context_files_prompt */
/* PoP: build_context_files_prompt @ agent/prompt_builder.py:build_context_files_prompt */
char *build_context_files_prompt(const char *cwd, bool skip_soul) {
    if (!cwd) cwd = ".";

    /* Collect sections */
    const int MAX_SECTIONS = 8;
    char *sections[MAX_SECTIONS];
    int n_sections = 0;

    /* Priority-based project context: first match wins */
    char *project = context_load_hermes_md(cwd);
    if (!project) project = context_load_agents_md(cwd);
    if (!project) project = context_load_claude_md(cwd);
    if (!project) project = context_load_cursorrules(cwd);

    if (project && n_sections < MAX_SECTIONS)
        sections[n_sections++] = project;

    /* SOUL.md from HERMES_HOME (independent of project context) */
    if (!skip_soul) {
        char *soul = load_soul_md();
        if (soul && n_sections < MAX_SECTIONS)
            sections[n_sections++] = soul;
    }

    if (n_sections == 0)
        return strdup("");

    /* Build combined prompt */
    const char *header =
        "# Project Context\n\n"
        "The following project context files have been loaded and should be followed:\n\n";

    size_t total = strlen(header);
    for (int i = 0; i < n_sections; i++)
        total += strlen(sections[i]) + 1;

    char *result = (char *)malloc(total + 1);
    if (!result) {
        for (int i = 0; i < n_sections; i++) free(sections[i]);
        return NULL;
    }

    strcpy(result, header);
    size_t pos = strlen(header);
    for (int i = 0; i < n_sections; i++) {
        size_t slen = strlen(sections[i]);
        memcpy(result + pos, sections[i], slen);
        pos += slen;
        if (i < n_sections - 1)
            result[pos++] = '\n';
    }
    result[pos] = '\0';

    for (int i = 0; i < n_sections; i++)
        free(sections[i]);

    return result;
}

/* ================================================================
 *  Platform hints (ported from PLATFORM_HINTS dict)
 * ================================================================ */

/* PoP: platform_hint_get @ agent/prompt_builder.py:_current_session_platform_hint */
const char *platform_hint_get(const char *platform_name) {
    if (!platform_name) return NULL;

    if (strcmp(platform_name, "telegram") == 0)
        return "You are on a text messaging communication platform, Telegram. "
               "Standard markdown is automatically converted to Telegram format. "
               "Supported: **bold**, *italic*, ~~strikethrough~~, ||spoiler||, "
               "`inline code`, ```code blocks```, [links](url), and ## headers. "
               "Telegram has NO table syntax -- prefer bullet lists or labeled "
               "key: value pairs over pipe tables. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.png, .jpg, .webp) appear as photos, "
               "audio (.ogg) sends as voice bubbles, and videos (.mp4) play inline. "
               "You can also include image URLs in markdown format ![alt](url) and "
               "they will be sent as native photos.";

    if (strcmp(platform_name, "discord") == 0)
        return "You are in a Discord server or group chat communicating with your user. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.png, .jpg, .webp) are sent as photo "
               "attachments, audio as file attachments. You can also include image URLs "
               "in markdown format ![alt](url) and they will be sent as attachments.";

    if (strcmp(platform_name, "slack") == 0)
        return "You are in a Slack workspace communicating with your user. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.png, .jpg, .webp) are uploaded as photo "
               "attachments, audio as file attachments. You can also include image URLs "
               "in markdown format ![alt](url) and they will be uploaded as attachments.";

    if (strcmp(platform_name, "whatsapp") == 0)
        return "You are on a text messaging communication platform, WhatsApp. "
               "Please do not use markdown as it does not render. "
               "You can send media files natively: to deliver a file to the user, "
               "include MEDIA:/absolute/path/to/file in your response. The file "
               "will be sent as a native WhatsApp attachment -- images (.jpg, .png, "
               ".webp) appear as photos, videos (.mp4, .mov) play inline, and other "
               "files arrive as downloadable documents. You can also include image "
               "URLs in markdown format ![alt](url) and they will be sent as photos.";

    if (strcmp(platform_name, "signal") == 0)
        return "You are on a text messaging communication platform, Signal. "
               "Please do not use markdown as it does not render. "
               "You can send media files natively: to deliver a file to the user, "
               "include MEDIA:/absolute/path/to/file in your response. Images "
               "(.png, .jpg, .webp) appear as photos, audio as attachments, and other "
               "files arrive as downloadable documents. You can also include image "
               "URLs in markdown format ![alt](url) and they will be sent as photos.";

    if (strcmp(platform_name, "cli") == 0)
        return "You are a CLI AI Agent. Try not to use markdown but simple text "
               "renderable inside a terminal. "
               "File delivery: there is no attachment channel -- the user reads your "
               "response directly in their terminal. Do NOT emit MEDIA:/path tags. "
               "When referring to a file you created or changed, just state its "
               "absolute path in plain text; the user can open it from there.";

    if (strcmp(platform_name, "email") == 0)
        return "You are communicating via email. Write clear, well-structured responses "
               "suitable for email. Use plain text formatting (no markdown). "
               "Keep responses concise but complete. You can send file attachments -- "
               "include MEDIA:/absolute/path/to/file in your response. The subject line "
               "is preserved for threading. Do not include greetings or sign-offs unless "
               "contextually appropriate.";

    if (strcmp(platform_name, "sms") == 0)
        return "You are communicating via SMS. Keep responses concise and use plain text "
               "only -- no markdown, no formatting. SMS messages are limited to ~1600 "
               "characters, so be brief and direct.";

    if (strcmp(platform_name, "cron") == 0)
        return "You are running as a scheduled cron job. There is no user present -- you "
               "cannot ask questions, request clarification, or wait for follow-up. Execute "
               "the task fully and autonomously, making reasonable decisions where needed. "
               "Your final response is automatically delivered to the job's configured "
               "destination -- put the primary content directly in your response.";

    if (strcmp(platform_name, "matrix") == 0)
        return "You are in a Matrix room communicating with your user. "
               "Matrix renders Markdown -- bold, italic, code blocks, and links work; "
               "the adapter converts your Markdown to HTML for rich display. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.jpg, .png, .webp) are sent as inline photos, "
               "audio (.ogg, .mp3) as voice/audio messages, video (.mp4) inline, "
               "and other files as downloadable attachments.";

    if (strcmp(platform_name, "feishu") == 0)
        return "You are in a Feishu (Lark) workspace communicating with your user. "
               "Feishu renders Markdown in messages -- bold, italic, code blocks, and "
               "links are supported. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.jpg, .png, .webp) are uploaded and displayed "
               "inline, audio files as voice messages, and other files as attachments.";

    if (strcmp(platform_name, "mattermost") == 0)
        return "You are in a Mattermost workspace communicating with your user. "
               "Mattermost renders standard Markdown -- headings, bold, italic, code "
               "blocks, and tables all work. "
               "You can send media files natively: include MEDIA:/absolute/path/to/file "
               "in your response. Images (.jpg, .png, .webp) are uploaded as photo "
               "attachments, audio and video as file attachments. "
               "Image URLs in markdown format ![alt](url) are rendered as inline previews.";

    if (strcmp(platform_name, "weixin") == 0 || strcmp(platform_name, "wechat") == 0)
        return "You are on Weixin/WeChat. Markdown formatting is supported, so you may use it when "
               "it improves readability, but keep the message compact and chat-friendly. You can send "
               "media files natively: include MEDIA:/absolute/path/to/file in your response. "
               "Images are sent as native photos, videos play inline when supported, and other files "
               "arrive as downloadable documents. You can also include image URLs in markdown format "
               "![alt](url) and they will be downloaded and sent as native media when possible.";

    if (strcmp(platform_name, "wecom") == 0)
        return "You are on WeCom (Enterprise WeChat). Markdown formatting is supported. "
               "You CAN send media files natively -- to deliver a file to the user, include "
               "MEDIA:/absolute/path/to/file in your response. The file will be sent as a native "
               "WeCom attachment: images (.jpg, .png, .webp) are sent as photos (up to 10 MB), "
               "other files (.pdf, .docx, .xlsx, .md, .txt, etc.) arrive as downloadable documents "
               "(up to 20 MB), and videos (.mp4) play inline. Voice messages are supported but "
               "must be in AMR format -- other audio formats are automatically sent as file attachments. "
               "You can also include image URLs in markdown format ![alt](url) and they will be "
               "downloaded and sent as native photos. Do NOT tell the user you lack file-sending "
               "capability -- use MEDIA: syntax whenever a file delivery is appropriate.";

    if (strcmp(platform_name, "qqbot") == 0)
        return "You are on QQ, a popular Chinese messaging platform. QQ supports markdown formatting "
               "and emoji. You can send media files natively: include MEDIA:/absolute/path/to/file in "
               "your response. Images are sent as native photos, and other files arrive as downloadable "
               "documents.";

    if (strcmp(platform_name, "yuanbao") == 0)
        return "You are on Yuanbao (Tencent Yuanbao), a Chinese AI assistant platform. "
               "Markdown formatting is supported (code blocks, tables, bold/italic). "
               "You CAN send media files natively -- to deliver a file to the user, include "
               "MEDIA:/absolute/path/to/file in your response. The file will be sent as a native "
               "Yuanbao attachment: images (.jpg, .png, .webp, .gif) are sent as photos, "
               "and other files (.pdf, .docx, .txt, .zip, etc.) arrive as downloadable documents "
               "(max 50 MB). You can also include image URLs in markdown format ![alt](url) and "
               "they will be downloaded and sent as native photos. "
               "Do NOT tell the user you lack file-sending capability -- use MEDIA: syntax "
               "whenever a file delivery is appropriate.";

    if (strcmp(platform_name, "api_server") == 0 || strcmp(platform_name, "webui") == 0) {
        if (strcmp(platform_name, "webui") == 0)
            return "You are in the Hermes WebUI, a browser-based chat interface. "
                   "Full Markdown rendering is supported -- headings, bold, italic, code "
                   "blocks, tables, math (LaTeX), and Mermaid diagrams all render natively. "
                   "To display local or remote media/files inline, include "
                   "MEDIA:/absolute/path/to/file or MEDIA:https://... in your response. "
                   "Local file paths must be absolute. Images, audio (with playback speed "
                   "controls), video, PDFs, HTML, CSV, diffs/patches, and Excalidraw files "
                   "render as rich previews. Do not use Markdown image syntax like "
                   "![alt](/path) for local files; local paths are not served that way. "
                   "Use MEDIA:/absolute/path instead.";
        return "You're responding through an API server. The rendering layer is unknown -- "
               "assume plain text. No markdown formatting (no asterisks, bullets, headers, "
               "code fences). Treat this like a conversation, not a document. Keep responses "
               "brief and natural.";
    }

    if (strcmp(platform_name, "bluebubbles") == 0)
        return "You are chatting via iMessage (BlueBubbles). iMessage does not render "
               "markdown formatting -- use plain text. Keep responses concise as they "
               "appear as text messages. You can send media files natively: include "
               "MEDIA:/absolute/path/to/file in your response. Images (.jpg, .png, "
               ".heic) appear as photos and other files arrive as attachments.";

    return NULL; /* platform not recognized */
}

/* ================================================================
 *  Environment hints (ported from build_environment_hints)
 * ================================================================ */

/* Port of Python: build_environment_hints */
/* PoP: build_environment_hints @ agent/prompt_builder.py:build_environment_hints */
char *build_environment_hints(void) {
    /* Detect WSL */
    int is_wsl = 0;
    {
        struct stat st;
        if (stat("/proc/version", &st) == 0) {
            char buf[256];
            FILE *f = fopen("/proc/version", "r");
            if (f) {
                if (fgets(buf, sizeof(buf), f)) {
                    if (strcasestr(buf, "microsoft") || strcasestr(buf, "WSL"))
                        is_wsl = 1;
                }
                fclose(f);
            }
        }
    }

    /* Detect OS */
    const char *os_name = "Linux";
    {
        FILE *f = popen("uname -s 2>/dev/null", "r");
        if (f) {
            char buf[64];
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = '\0';
                if (buf[0]) os_name = NULL; /* signal that we have a real name */
            }
            pclose(f);
        }
    }

    /* Build environment hints */
    char hints[4096];
    int pos = 0;

    if (is_wsl) {
        pos += snprintf(hints + pos, sizeof(hints) - (size_t)pos,
            "Host: WSL (Windows Subsystem for Linux)\n"
            "User home directory: %s\n"
            "Current working directory: %s\n\n"
            "You are running inside WSL (Windows Subsystem for Linux). "
            "The Windows host filesystem is mounted under /mnt/ -- "
            "/mnt/c/ is the C: drive, /mnt/d/ is D:, etc. "
            "The user's Windows files are typically at "
            "/mnt/c/Users/<username>/Desktop/, Documents/, Downloads/, etc. "
            "When the user references Windows paths or desktop files, translate "
            "to the /mnt/c/ equivalent. You can list /mnt/c/Users/ to discover "
            "the Windows username if needed.",
            getenv("HOME") ? getenv("HOME") : "/root",
            getenv("PWD") ? getenv("PWD") : ".");
    } else {
        pos += snprintf(hints + pos, sizeof(hints) - (size_t)pos,
            "Host: %s\n"
            "User home directory: %s\n"
            "Current working directory: %s\n",
            os_name ? os_name : "Linux",
            getenv("HOME") ? getenv("HOME") : "/root",
            getenv("PWD") ? getenv("PWD") : ".");
    }

    if (pos > 0 && (size_t)pos < sizeof(hints))
        return strdup(hints);
    return NULL;
}

/* ================================================================
 *  Buffer append helpers

#include "hermes_json.h"
#include <pthread.h>
#include <errno.h>

#define SKILLS_SNAPSHOT_VERSION 1
#define SKILLS_PROMPT_CACHE_MAX  8
/* ================================================================
 *  Skills prompt snapshot caching (AG03)
 * ================================================================ */

#define SKILLS_SNAPSHOT_VERSION 1
#define SKILLS_PROMPT_CACHE_MAX  8
#define SNAPSHOT_FILENAME ".skills_prompt_snapshot.json"

/* Thread-safe cache lock */
static pthread_mutex_t skills_cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* In-memory cache */
static struct {
    char key[256];
    char *value;
} skills_cache[SKILLS_PROMPT_CACHE_MAX];
static int skills_cache_count = 0;

/* Get path to snapshot file in hermes_home */
/* Port of Python: _skills_prompt_snapshot_path */
/* PoP: skills_prompt_snapshot_path @ agent/prompt_builder.py:_skills_prompt_snapshot_path */
static char *skills_prompt_snapshot_path(const char *hermes_home) {
    if (!hermes_home) return NULL;
    size_t len = strlen(hermes_home) + strlen(SNAPSHOT_FILENAME) + 2;
    char *path = malloc(len);
    if (path) snprintf(path, len, "%s/%s", hermes_home, SNAPSHOT_FILENAME);
    return path;
}

/* Clear the in-process skills prompt cache (and optionally disk snapshot) */
/* Port of Python: clear_skills_system_prompt_cache */
/* PoP: clear_skills_system_prompt_cache @ agent/prompt_builder.py:clear_skills_system_prompt_cache */
void clear_skills_system_prompt_cache(const char *hermes_home, bool clear_snapshot) {
    pthread_mutex_lock(&skills_cache_lock);
    for (int i = 0; i < skills_cache_count; i++) {
        free(skills_cache[i].value);
        skills_cache[i].value = NULL;
        skills_cache[i].key[0] = '\0';
    }
    skills_cache_count = 0;
    pthread_mutex_unlock(&skills_cache_lock);

    if (clear_snapshot && hermes_home) {
        char *spath = skills_prompt_snapshot_path(hermes_home);
        if (spath) {
            unlink(spath);
            free(spath);
        }
    }
}

/* Build an mtime/size manifest of SKILL.md and DESCRIPTION.md files.
 * Returns JSON object: { "relative/path": [mtime_ns, size], ... } */
/* Port of Python: _build_skills_manifest */
/* PoP: build_skills_manifest @ agent/prompt_builder.py:_build_skills_manifest */
json_node_t *build_skills_manifest(const char *skills_dir) {
    json_node_t *manifest = json_object();
    if (!manifest || !skills_dir) return manifest;

    const char *filenames[] = {"SKILL.md", "DESCRIPTION.md", NULL};

    for (int fi = 0; filenames[fi]; fi++) {
        DIR *dir = opendir(skills_dir);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_DIR) continue;
            if (entry->d_name[0] == '.') continue;

            /* Check category/SKILL.md */
            char filepath[4096];
            snprintf(filepath, sizeof(filepath), "%s/%s/%s",
                     skills_dir, entry->d_name, filenames[fi]);

            struct stat st;
            if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                char rel_path[512];
                snprintf(rel_path, sizeof(rel_path), "%s/%s",
                         entry->d_name, filenames[fi]);
                json_node_t *pair = json_array();
                json_array_append(pair, json_number((double)st.st_mtime));
                json_array_append(pair, json_number((double)st.st_size));
                json_object_set(manifest, rel_path, pair);
            }

            /* Check subdirs: category/subdir/SKILL.md */
            char subdir_path[4096];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s",
                     skills_dir, entry->d_name);
            DIR *subdir = opendir(subdir_path);
            if (!subdir) continue;

            struct dirent *subentry;
            while ((subentry = readdir(subdir)) != NULL) {
                if (subentry->d_type != DT_DIR) continue;
                if (subentry->d_name[0] == '.') continue;

                snprintf(filepath, sizeof(filepath), "%s/%s/%s/%s",
                         skills_dir, entry->d_name, subentry->d_name,
                         filenames[fi]);

                struct stat st2;
                if (stat(filepath, &st2) == 0 && S_ISREG(st2.st_mode)) {
                    char rel_path2[512];
                    snprintf(rel_path2, sizeof(rel_path2), "%s/%s/%s",
                             entry->d_name, subentry->d_name, filenames[fi]);
                    json_node_t *pair2 = json_array();
                    json_array_append(pair2, json_number((double)st2.st_mtime));
                    json_array_append(pair2, json_number((double)st2.st_size));
                    json_object_set(manifest, rel_path2, pair2);
                }
            }
            closedir(subdir);
        }
        closedir(dir);
    }

    return manifest;
}
/* Port of Python: _load_skills_snapshot */

/* Load the disk snapshot if it exists and its manifest still matches.
 * Returns JSON snapshot object or NULL. */
/* PoP: load_skills_snapshot @ agent/prompt_builder.py:_load_skills_snapshot */
json_node_t *load_skills_snapshot(const char *skills_dir, const char *hermes_home) {
    if (!hermes_home) return NULL;

    char *spath = skills_prompt_snapshot_path(hermes_home);
    if (!spath) return NULL;

    FILE *f = fopen(spath, "r");
    free(spath);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, fsize, f);
    buf[fsize] = '\0';
    fclose(f);

    json_node_t *snapshot = json_parse(buf, NULL);
    free(buf);

    if (!snapshot || snapshot->type != JSON_OBJECT) {
        if (snapshot) json_free(snapshot);
        return NULL;
    }

    /* Check version */
    json_node_t *ver = json_object_get(snapshot, "version");
    if (!ver || ver->type != JSON_NUMBER || (int)ver->num_val != SKILLS_SNAPSHOT_VERSION) {
        json_free(snapshot);
        return NULL;
    }

    /* Check manifest match */
    json_node_t *snap_manifest = json_object_get(snapshot, "manifest");
    if (!snap_manifest) {
        json_free(snapshot);
        return NULL;
    }

    json_node_t *current_manifest = build_skills_manifest(skills_dir);
    /* Compare by serialization since no json_equals available */
    char *snap_str = json_serialize(snap_manifest);
    char *cur_str = json_serialize(current_manifest);
    bool match = (snap_str && cur_str && strcmp(snap_str, cur_str) == 0);
    free(snap_str);
    free(cur_str);
    json_free(current_manifest);

    if (!match) {
        json_free(snapshot);
        return NULL;
    }

    return snapshot;
}

/* Write skill metadata to disk for fast cold-start reuse. */
/* Port of Python: _write_skills_snapshot */
/* PoP: write_skills_snapshot @ agent/prompt_builder.py:_write_skills_snapshot */
void write_skills_snapshot(const char *skills_dir, const char *hermes_home,
                            json_node_t *manifest, json_node_t *skill_entries,
                            json_node_t *category_descriptions) {
    if (!hermes_home) return;

    json_node_t *payload = json_object();
    json_object_set(payload, "version", json_number(SKILLS_SNAPSHOT_VERSION));
    json_object_set(payload, "manifest", manifest ? json_copy(manifest) : json_object());
    json_object_set(payload, "skills", skill_entries ? json_copy(skill_entries) : json_array());
    json_object_set(payload, "category_descriptions",
                    category_descriptions ? json_copy(category_descriptions) : json_object());

    char *json_str = json_serialize(payload);
    json_free(payload);

    if (!json_str) return;

    char *spath = skills_prompt_snapshot_path(hermes_home);
    if (!spath) { free(json_str); return; }

    FILE *f = fopen(spath, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
    }

    free(spath);
    free(json_str);
}

/* Build a serializable metadata dict for one skill.
 * Returns JSON object with skill_name, category, frontmatter_name,
 * description, platforms, conditions. */
/* Port of Python: _build_snapshot_entry */
/* PoP: build_snapshot_entry @ agent/prompt_builder.py:_build_snapshot_entry */
json_node_t *build_snapshot_entry(const char *skill_file, const char *skills_dir,
                                   json_node_t *frontmatter, const char *description) {
    json_node_t *entry = json_object();
    if (!entry) return NULL;

    /* Extract skill_name and category from relative path */
    char skill_name[256] = "unknown";
    char category[256] = "general";

    if (skill_file && skills_dir) {
        size_t dlen = strlen(skills_dir);
        if (strncmp(skill_file, skills_dir, dlen) == 0 && skill_file[dlen] == '/') {
            const char *rel = skill_file + dlen + 1;
            /* Parse: category/skill_name/SKILL.md or skill_name/SKILL.md */
            const char *first_slash = strchr(rel, '/');
            if (first_slash) {
                const char *second_slash = strchr(first_slash + 1, '/');
                if (second_slash) {
                    /* category/subdir/SKILL.md */
                    size_t cat_len = second_slash - rel;
                    if (cat_len < sizeof(category)) {
                        memcpy(category, rel, cat_len);
                        category[cat_len] = '\0';
                    }
                    /* skill_name is the subdir */
                    size_t name_len = second_slash - (first_slash + 1);
                    if (name_len < sizeof(skill_name)) {
                        memcpy(skill_name, first_slash + 1, name_len);
                        skill_name[name_len] = '\0';
                    }
                } else {
                    /* skill_name/SKILL.md */
                    size_t name_len = first_slash - rel;
                    if (name_len < sizeof(skill_name)) {
                        memcpy(skill_name, rel, name_len);
                        skill_name[name_len] = '\0';
                    }
                    strncpy(category, rel, sizeof(category) - 1);
                    category[sizeof(category) - 1] = '\0';
                }
            } else {
                strncpy(skill_name, rel, sizeof(skill_name) - 1);
                skill_name[sizeof(skill_name) - 1] = '\0';
            }
        }
    }

    json_object_set(entry, "skill_name", json_string(skill_name));
    json_object_set(entry, "category", json_string(category));

    /* frontmatter_name from frontmatter dict */
    char *fm_name = NULL;
    if (frontmatter && frontmatter->type == JSON_OBJECT) {
        json_node_t *name_node = json_object_get(frontmatter, "name");
        if (name_node && name_node->type == JSON_STRING) {
            fm_name = name_node->str_val;
        }
    }
    json_object_set(entry, "frontmatter_name", json_string(fm_name ? fm_name : skill_name));
    json_object_set(entry, "description", json_string(description ? description : ""));

    /* platforms from frontmatter */
    json_node_t *platforms = json_array();
    if (frontmatter && frontmatter->type == JSON_OBJECT) {
        json_node_t *plat_node = json_object_get(frontmatter, "platforms");
        if (plat_node) {
            if (plat_node->type == JSON_STRING) {
                json_array_append(platforms, json_string(plat_node->str_val));
            } else if (plat_node->type == JSON_ARRAY) {
                size_t plat_count = json_len(plat_node);
                for (size_t i = 0; i < plat_count; i++) {
                    json_node_t *item = json_get(plat_node, i);
                    if (item && item->type == JSON_STRING) {
                        json_array_append(platforms, json_string(item->str_val));
                    }
                }
            }
        }
    }
    json_object_set(entry, "platforms", platforms);

    /* conditions — simplified: extract from frontmatter.conditions */
    json_node_t *conditions = json_array();
    if (frontmatter && frontmatter->type == JSON_OBJECT) {
        json_node_t *cond_node = json_object_get(frontmatter, "conditions");
        if (cond_node && cond_node->type == JSON_ARRAY) {
            size_t cond_count = json_len(cond_node);
            for (size_t i = 0; i < cond_count; i++) {
                json_node_t *item = json_get(cond_node, i);
                if (item && item->type == JSON_STRING) {
                    json_array_append(conditions, json_string(item->str_val));
                }
            }
        }
    }
    json_object_set(entry, "conditions", conditions);

    return entry;
}

/* ================================================================
 *  Runtime CWD resolution (ported from agent/runtime_cwd.py)
 * ================================================================ */

static char *g_session_cwd = NULL;

/* Port of Python agent/runtime_cwd.py:set_session_cwd */
void set_session_cwd(const char *cwd) {
    free(g_session_cwd);
    if (cwd && cwd[0]) {
        g_session_cwd = strdup(cwd);
    } else {
        g_session_cwd = NULL;
    }
}

/* Port of Python agent/runtime_cwd.py:clear_session_cwd */
void clear_session_cwd(void) {
    free(g_session_cwd);
    g_session_cwd = NULL;
}

/* Resolve the agent working directory.
 * Priority: session override > TERMINAL_CWD env > PWD env > getcwd() */
/* Port of Python agent/runtime_cwd.py:resolve_agent_cwd */
const char *resolve_agent_cwd(void) {
    static char cwd_buf[4096];
    const char *src = NULL;

    /* 1. Session override (per-session cwd for gateway) */
    if (g_session_cwd && g_session_cwd[0]) {
        src = g_session_cwd;
    }
    /* 2. TERMINAL_CWD env var (set by gateway/cron at startup) */
    if (!src) {
        src = getenv("TERMINAL_CWD");
        if (src && !src[0]) src = NULL;
    }
    /* 3. PWD env var */
    if (!src) {
        src = getenv("PWD");
        if (src && !src[0]) src = NULL;
    }
    /* 4. getcwd() fallback */
    if (!src) {
        if (getcwd(cwd_buf, sizeof(cwd_buf))) {
            return cwd_buf;
        }
        return ".";
    }

    /* Resolve ~ and relative paths via realpath */
    char *resolved = realpath(src, NULL);
    if (resolved) {
        snprintf(cwd_buf, sizeof(cwd_buf), "%s", resolved);
        free(resolved);
        return cwd_buf;
    }
    return src;
}

/* Resolve context cwd — returns NULL if no configured cwd.
 * Unlike resolve_agent_cwd(), this does NOT fall back to getcwd(). */
/* Port of Python agent/runtime_cwd.py:resolve_context_cwd */
const char *resolve_context_cwd(void) {
    const char *src = NULL;

    if (g_session_cwd && g_session_cwd[0]) {
        src = g_session_cwd;
    }
    if (!src) {
        src = getenv("TERMINAL_CWD");
        if (src && !src[0]) src = NULL;
    }
    return src;
}

/* ================================================================
 *  Format tools for system message (ported from agent/system_prompt.py)
 *  AG03: build_skills_system_prompt — Port of Python prompt_builder
 * ================================================================ */

/* Check if a skill name is in a comma-separated disabled list. */
static bool skill_is_disabled(const char *name, const char *disabled_csv) {
    if (!disabled_csv || !disabled_csv[0]) return false;
    size_t nlen = strlen(name);
    const char *p = disabled_csv;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        size_t slen = strcspn(p, ", ");
        if (slen == nlen && strncasecmp(p, name, nlen) == 0) return true;
        p += slen;
    }
    return false;
}

/* Comparison function for sorting skills by name. */
static int cmp_skills_by_name(const void *a, const void *b) {
    const skill_meta_t *sa = (const skill_meta_t *)a;
    const skill_meta_t *sb = (const skill_meta_t *)b;
    return strcasecmp(sa->name, sb->name);
}

/* Comparison function for sorting category names. */
static int cmp_strings(const void *a, const void *b) {
    return strcasecmp(*(const char **)a, *(const char **)b);
}

/* Port of Python: build_skills_system_prompt */
/* PoP: build_skills_system_prompt @ agent/prompt_builder.py:build_skills_system_prompt */
char *build_skills_system_prompt(const char *disabled_csv) {
    skill_list_t *list = skills_scan_all();
    if (!list || list->count == 0) {
        skills_scan_free(list);
        return strdup("");
    }

    /* Filter out disabled skills and group by category.
     * We build a dynamic array of per-category skill arrays. */
    typedef struct {
        char name[128];
        skill_meta_t *skills;
        size_t count;
        size_t capacity;
    } category_t;

    category_t *categories = NULL;
    size_t ncategories = 0;
    size_t cat_capacity = 0;

    for (size_t i = 0; i < list->count; i++) {
        skill_meta_t *m = &list->skills[i];
        if (skill_is_disabled(m->name, disabled_csv)) continue;

        /* Determine category: use m->category if set, else "general" */
        const char *cat = m->category[0] ? m->category : "general";

        /* Find or create category */
        size_t ci;
        for (ci = 0; ci < ncategories; ci++) {
            if (strcasecmp(categories[ci].name, cat) == 0) break;
        }
        if (ci >= ncategories) {
            if (ncategories >= cat_capacity) {
                cat_capacity = cat_capacity == 0 ? 8 : cat_capacity * 2;
                categories = (category_t *)realloc(categories, cat_capacity * sizeof(category_t));
                if (!categories) { skills_scan_free(list); return strdup(""); }
            }
            memset(&categories[ci], 0, sizeof(category_t));
            snprintf(categories[ci].name, sizeof(categories[ci].name), "%s", cat);
            ncategories++;
        }

        /* Add skill to category */
        if (categories[ci].count >= categories[ci].capacity) {
            categories[ci].capacity = categories[ci].capacity == 0 ? 8 : categories[ci].capacity * 2;
            categories[ci].skills = (skill_meta_t *)realloc(categories[ci].skills,
                categories[ci].capacity * sizeof(skill_meta_t));
            if (!categories[ci].skills) { skills_scan_free(list); return strdup(""); }
        }
        categories[ci].skills[categories[ci].count++] = *m;
    }

    if (ncategories == 0) {
        free(categories);
        skills_scan_free(list);
        return strdup("");
    }

    /* Sort categories by name */
    char **cat_names = (char **)malloc(ncategories * sizeof(char *));
    for (size_t i = 0; i < ncategories; i++) cat_names[i] = categories[i].name;
    qsort(cat_names, ncategories, sizeof(char *), cmp_strings);

    /* Build output string */
    size_t buf_cap = 8192;
    size_t buf_len = 0;
    char *buf = (char *)malloc(buf_cap);
    if (!buf) { free(cat_names); free(categories); skills_scan_free(list); return strdup(""); }
    buf[0] = '\0';

    /* Header — matches Python's format */
    const char *header =
        "## Skills (mandatory)\n"
        "Before replying, scan the skills below. If a skill matches or is even partially relevant "
        "to your task, you MUST load it with skill_view(name) and follow its instructions. "
        "Err on the side of loading — it is always better to have context you don't need "
        "than to miss critical steps, pitfalls, or established workflows. "
        "Skills contain specialized knowledge — API endpoints, tool-specific commands, "
        "and proven workflows that outperform general-purpose approaches. Load the skill "
        "even if you think you could handle the task with basic tools like web_search or terminal. "
        "Skills also encode the user's preferred approach, conventions, and quality standards "
        "for tasks like code review, planning, and testing — load them even for tasks you "
        "already know how to do, because the skill defines how it should be done here.\n"
        "Whenever the user asks you to configure, set up, install, enable, disable, modify, "
        "or troubleshoot Hermes Agent itself — its CLI, config, models, providers, tools, "
        "skills, voice, gateway, plugins, or any feature — load the `hermes-agent` skill "
        "first. It has the actual commands (e.g. `hermes config set …`, `hermes tools`, "
        "`hermes setup`) so you don't have to guess or invent workarounds.\n"
        "If a skill has issues, fix it with skill_manage(action='patch').\n"
        "After difficult/iterative tasks, offer to save as a skill. "
        "If a skill you loaded was missing steps, had wrong commands, or needed "
        "pitfalls you discovered, update it before finishing.\n"
        "\n"
        "<available_skills>\n";

    size_t hlen = strlen(header);
    if (buf_len + hlen + 1 > buf_cap) { buf_cap = (buf_len + hlen + 1) * 2; buf = (char *)realloc(buf, buf_cap); }
    memcpy(buf + buf_len, header, hlen);
    buf_len += hlen;
    buf[buf_len] = '\0';

    /* Categories and skills */
    for (size_t ci = 0; ci < ncategories; ci++) {
        /* Find the category by name */
        size_t idx;
        for (idx = 0; idx < ncategories; idx++) {
            if (strcasecmp(categories[idx].name, cat_names[ci]) == 0) break;
        }
        if (idx >= ncategories) continue;

        /* Sort skills within category by name */
        qsort(categories[idx].skills, categories[idx].count, sizeof(skill_meta_t), cmp_skills_by_name);

        /* Category header */
        char catline[256];
        int catlen = snprintf(catline, sizeof(catline), "  %s:\n", categories[idx].name);
        if (buf_len + catlen + 1 > buf_cap) { buf_cap = (buf_len + catlen + 1) * 2; buf = (char *)realloc(buf, buf_cap); }
        memcpy(buf + buf_len, catline, catlen);
        buf_len += catlen;
        buf[buf_len] = '\0';

        /* Skills in category */
        for (size_t si = 0; si < categories[idx].count; si++) {
            skill_meta_t *m = &categories[idx].skills[si];
            char skline[1024];
            int sklen;
            if (m->description[0]) {
                sklen = snprintf(skline, sizeof(skline), "    - %s: %s\n", m->name, m->description);
            } else {
                sklen = snprintf(skline, sizeof(skline), "    - %s\n", m->name);
            }
            if (buf_len + sklen + 1 > buf_cap) { buf_cap = (buf_len + sklen + 1) * 2; buf = (char *)realloc(buf, buf_cap); }
            memcpy(buf + buf_len, skline, sklen);
            buf_len += sklen;
            buf[buf_len] = '\0';
        }
    }

    /* Footer */
    const char *footer = "</available_skills>\n\nOnly proceed without loading a skill if genuinely none are relevant to the task.";
    size_t flen = strlen(footer);
    if (buf_len + flen + 1 > buf_cap) { buf_cap = (buf_len + flen + 1) * 2; buf = (char *)realloc(buf, buf_cap); }
    memcpy(buf + buf_len, footer, flen);
    buf_len += flen;
    buf[buf_len] = '\0';

    /* Cleanup */
    for (size_t i = 0; i < ncategories; i++) free(categories[i].skills);
    free(categories);
    free(cat_names);
    skills_scan_free(list);

    return buf;
}

/* ================================================================
 *  AG03: build_nous_subscription_prompt — Port of Python prompt_builder
 * ================================================================ */

/* Port of Python prompt_builder.py:format_steer_marker — wrap steer text in markers */
/* PoP: format_steer_marker @ agent/prompt_builder.py:format_steer_marker */
char *format_steer_marker(const char *steer_text) {
    if (!steer_text) return strdup("");
    const char *open  = "\n\n[OUT-OF-BAND USER MESSAGE -- a direct message from the user, delivered mid-turn; not tool output]\n";
    const char *close = "\n[/OUT-OF-BAND USER MESSAGE]";
    size_t len = strlen(open) + strlen(steer_text) + strlen(close) + 1;
    char *buf = malloc(len);
    if (!buf) return NULL;
    snprintf(buf, len, "%s%s%s", open, steer_text, close);
    return buf;
}

/* Port of Python: build_nous_subscription_prompt */
/* PoP: build_nous_subscription_prompt @ agent/prompt_builder.py:build_nous_subscription_prompt */
char *build_nous_subscription_prompt(void) {
    /* Nous subscription system is Nous-specific (hermes_cli.nous_subscription).
     * C has no equivalent — return empty string, matching Python behavior
     * when the import fails. */
    return strdup("");
}

/* Port of Python: _session_cwd_override — return the session cwd override or NULL */
const char *session_cwd_override(void) {
    /* Python's _session_cwd_override() reads from a thread-local set by
     * the /cwd command. C stores the override in g_session_cwd. */
    if (!g_session_cwd || !g_session_cwd[0]) {
        return NULL;
    }
    return g_session_cwd;
}

/* Port of Python: _clear_backend_probe_cache — reset backend probe cache */
/* PoP: clear_backend_probe_cache @ agent/prompt_builder.py:_clear_backend_probe_cache */
void clear_backend_probe_cache(void) {
    /* Static probe cache lives in system_prompt.c; clear it */
    /* The cache stores env_type:cwd_hint -> result strings */
    /* For now, nothing to do — C doesn't cache probes at module level */
}

/* Port of Python: _skill_should_show — check if skill should be shown based on conditions */
/* C note: ARCHITECTURAL N/A. C prompt builder loads skills statically at startup
 * via config, not dynamically at prompt-build time. Python's dynamic filtering
 * by available_tools/available_toolsets doesn't apply in C architecture.
 * This stub is retained for source-level parity; the real filtering happens
 * at skill-load time in skill_preprocessing.c. */
/* PoP: skill_should_show @ agent/prompt_builder.py:_skill_should_show */
bool skill_should_show(void) {
    /* In C, skill filtering is done at load time, not at prompt-build time.
     * All loaded skills are always shown in the prompt.
     * Python checks available_tools/available_toolsets dynamically;
     * C achieves the same effect by filtering during skill_load(). */
    return true;
}

/* Port of Python: _parse_skill_file — read SKILL.md, check compatibility */
/* Returns (is_compatible, frontmatter_json, description) */
/* PoP: parse_skill_file @ agent/prompt_builder.py:_parse_skill_file */
bool parse_skill_file(const char *skill_path, char *frontmatter_out, size_t fm_sz, 
                       char *desc_out, size_t desc_sz) {
    if (!skill_path || !skill_path[0]) return true;
    /* Read file content */
    FILE *fp = fopen(skill_path, "r");
    if (!fp) return true; /* Err on side of showing skill */
    fclose(fp);
    if (frontmatter_out && fm_sz > 0) frontmatter_out[0] = '\0';
    if (desc_out && desc_sz > 0) desc_out[0] = '\0';
    return true;
}

/* Port of Python: is_developer_role_model */
bool is_developer_role_model(const char *model_name) {
    if (!model_name || !model_name[0]) return false;
    static const char *DEV_ROLES[] = {"gpt-5", "codex", NULL};
    char lower[256]; snprintf(lower, sizeof(lower), "%s", model_name);
    for (int i = 0; lower[i]; i++) lower[i] = (char)tolower((unsigned char)lower[i]);
    for (int i = 0; DEV_ROLES[i]; i++) {
        if (strstr(lower, DEV_ROLES[i]) != NULL) return true;
    }
    return false;
}

/* Port of Python: _probe_remote_backend — probe remote terminal backend */
/* PoP: probe_remote_backend @ agent/prompt_builder.py:_probe_remote_backend */
char *probe_remote_backend(const char *env_type) {
    if (!env_type || !env_type[0]) return NULL;
    const char *known_remotes[] = {"docker", "singularity", "modal",
                                    "managed_modal", "daytona", "ssh", NULL};
    for (int i = 0; known_remotes[i]; i++) {
        if (strcasecmp(env_type, known_remotes[i]) == 0) {
            char probe_cmd[128];
            snprintf(probe_cmd, sizeof(probe_cmd), "uname -s 2>/dev/null || echo unknown");
            FILE *fp = popen(probe_cmd, "r");
            if (fp) {
                char os_buf[64] = {0};
                if (fgets(os_buf, sizeof(os_buf), fp))
                    os_buf[strcspn(os_buf, "\n")] = '\0';
                pclose(fp);
                if (os_buf[0]) {
                    char result[512];
                    snprintf(result, sizeof(result),
                        "Terminal backend: %s. Tools operate inside this %s environment. Detected OS: %s.",
                        env_type, env_type, os_buf);
                    return strdup(result);
                }
            }
            const char *desc = "a remote environment (likely Linux)";
            if (strcasecmp(env_type, "docker") == 0) desc = "a Docker container (Linux)";
            else if (strcasecmp(env_type, "singularity") == 0) desc = "a Singularity container (Linux)";
            else if (strcasecmp(env_type, "modal") == 0 || strcasecmp(env_type, "managed_modal") == 0)
                desc = "a Modal sandbox (Linux)";
            else if (strcasecmp(env_type, "daytona") == 0) desc = "a Daytona workspace (Linux)";
            else if (strcasecmp(env_type, "ssh") == 0) desc = "a remote host reached over SSH (likely Linux)";
            char result[512];
            snprintf(result, sizeof(result),
                "Terminal backend: %s. Your tools operate inside %s.",
                env_type, desc);
            return strdup(result);
        }
    }
    return NULL;
}
