/*
 * approval_windows_hardline.c -- real C11 port of upstream Hermes
 * tools/approval_windows_hardline.py.
 *
 * PoP: w21_approvalwi_56c32a_win_cmdpos @ tools/approval_windows_hardline.py:_win_cmdpos
 * PoP: w21_approvalwi_56c32a_windows_hardline_patterns @ tools/approval_windows_hardline.py:windows_hardline_patterns
 * PoP: w21_approvalwi_56c32a_windows_whatif_strict_patterns @ tools/approval_windows_hardline.py:windows_whatif_strict_patterns
 * PoP: w21_approvalwi_56c32a_whatif_preview_is_real @ tools/approval_windows_hardline.py:whatif_preview_is_real
 *
 * The pattern fragments below are the Python raw strings verbatim (only the
 * C-string escapes differ); `whatif_preview_is_real` runs them through PCRE2,
 * which accepts Python `re` syntax as written.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "noc_w21_approvalwi_56c32a.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define HL_HARDLINE_DRIVE_ROOT "[a-z]:/+(?:\\*(?:\\.\\*)?)?"
#define HL_HARDLINE_DRIVE_QUOTED "[\"\\'](?:[a-z]:/+(?:\\*(?:\\.\\*)?)?)[\"\\'](?:(?:\\.\\.?)?/)*(?:\\.\\.?)?(?:\\*(?:\\.\\*)?)?(?=\\s|$|[)`;|&<>])"
#define HL_PS_RECURSE_HL "-r(?:e(?:c(?:u(?:r(?:s(?:e)?)?)?)?)?)?\\b"
#define HL_PS_INCLUDE_NARROWS "(?!(?!(?:[^\\r\\n;&|]*\\s)?(?:/s\\b|--(?:\\s|$)))(?:[^\\r\\n;&|#\"\\'`)]|\"[^\"\\n]*\"|\\'[^\\'\\n]*\\')*[^\\S\\r\\n]-inc[a-z]*(?::|\\s+)[\"\\']?(?=[^\\s,;&|\"\\'$`(){}@]*[^\\s,;&|\"\\'$`(){}@*?.])[^\\s,;&|\"\\'$`(){}@]+[\"\\']?(?!\\s*,)(?:[\\s;&|)]|$))"
#define HL_CMDLET_END "(?![\\w.\\\\/-])"
#define HL_PS_NOT_WHATIF "(?!(?=(?:[^\\r\\n;&|\"\\'`(){}$@#]*[^\\r\\n;&|\"\\'`(){}$@#\\s,])?\\s+-whatif(?:\\s|[\"\\']?$))(?![^\\r\\n;&|\"\\'`(){}$@#]*-whatif:)(?![^\\r\\n;&|]*\\s(?:clear-disk|(?:restart|stop)-computer)(?![\\w.\\\\/-])))"
#define HL_WHATIF_OTHER "(?<![\\w.\\\\/$-])(?:format-volume|format(?:\\.com)?|remove-item|ri|rm|rd|rmdir|del|erase|mkfs(?:\\.\\w+)?|dd|kill|shutdown|reboot|halt|poweroff|init|telinit|systemctl)(?![\\w-])"
#define HL_WINCMD_HEAD "(?:\\r[^\\S\\r]*|"
#define HL_WINCMD_TAIL ")(?:(?:[^\\s;&|\"\\'`]*[\\\\/])?(?:powershell|pwsh|cmd)(?:\\.exe)?(?:[^\\S\\r\\n]+(?:-(?!f(?:i(?:le?)?)?(?:[\\s:]|$))|/)[^\\s\"\\']+(?:[^\\S\\r\\n]+\\w+(?=[^\\S\\r\\n]+[-/\"\\']))?)*[^\\S\\r\\n]+[\"\\']?)?"
#define HL_WHATIF_STMT "(?<![\\w.\\\\/-])(?:clear-disk|(?:restart|stop)-computer)(?![\\w.\\\\/-])(?!(?=(?:[^\\r\\n;&|\"\\'`(){}$@#]*[^\\r\\n;&|\"\\'`(){}$@#\\s,])?\\s+-whatif(?:\\s|[\"\\']?$))(?![^\\r\\n;&|\"\\'`(){}$@#]*-whatif:)(?![^\\r\\n;&|]*\\s(?:clear-disk|(?:restart|stop)-computer)(?![\\w.\\\\/-])))"

#define HL_RM_CMD "rm\\s+"
#define HL_RM_VERBS "(?:remove-item|ri|rm|rd|rmdir|del|erase)\\b(?=[^\\r\\n;&|]*\\s(?:/s\\b|"
#define HL_RM_VERBS_TAIL "))"
#define HL_ROOT_TAIL "[^\\r\\n;&|]*[\\s,](?:"
#define HL_ROOT_TAIL_RM "(?:-[^\\s]*\\s+)*(?:"
#define HL_ROOT_END "(?:\\s|$|[)`;|&<>]))"
#define HL_ROOT_END_WIN "(?:\\s|$|[)`;|&\"\\'<>,]))"

static const char DESC_SHUTDOWN[] = "system shutdown/reboot (PowerShell)";
static const char DESC_FORMAT_VOLUME[] = "format filesystem (Format-Volume)";
static const char DESC_FORMAT_DRIVE[] = "format drive (format.com)";
static const char DESC_CLEAR_DISK[] = "wipe disk (Clear-Disk)";
static const char DESC_ROOT_RM[] = "recursive delete of drive root";

/* --- small string builder ---------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} hl_buf;

static int hb_add(hl_buf *b, const char *text)
{
    size_t n = strlen(text);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        char *grown;
        while (cap < b->len + n + 1) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) return 0;
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, text, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return 1;
}

static char *hb_take(hl_buf *b)
{
    char *out = b->buf;
    b->buf = NULL;
    b->len = b->cap = 0;
    return out;
}

static char *hl_cat3(const char *a, const char *b, const char *c)
{
    hl_buf buf = {NULL, 0, 0};
    if (!hb_add(&buf, a) || !hb_add(&buf, b) || !hb_add(&buf, c)) {
        free(buf.buf);
        return NULL;
    }
    return hb_take(&buf);
}

char *w21_approvalwi_56c32a_win_cmdpos(const char *cmdpos)
{
    return hl_cat3(HL_WINCMD_HEAD, cmdpos ? cmdpos : "", HL_WINCMD_TAIL);
}

/* --- rule tables ------------------------------------------------------------ */

json_t *w21_approvalwi_56c32a_windows_hardline_patterns(const char *cmdpos)
{
    json_t *rules = json_array();
    char *win = w21_approvalwi_56c32a_win_cmdpos(cmdpos);
    char *pattern;
    size_t i;

    if (!rules || !win) { json_free(rules); free(win); return NULL; }

    for (i = 0; i < 6; i++) {
        const char *desc = NULL;
        switch (i) {
        case 0:
            pattern = hl_cat3(win, "(?:restart|stop)-computer",
                              HL_CMDLET_END HL_PS_NOT_WHATIF);
            desc = DESC_SHUTDOWN;
            break;
        case 1:
            pattern = hl_cat3(win, "format-volume", HL_CMDLET_END);
            desc = DESC_FORMAT_VOLUME;
            break;
        case 2:
            pattern = hl_cat3(win, "format(?:\\.com)?\\s+[a-z]:", "");
            desc = DESC_FORMAT_DRIVE;
            break;
        case 3:
            pattern = hl_cat3(win, "clear-disk", HL_CMDLET_END HL_PS_NOT_WHATIF);
            desc = DESC_CLEAR_DISK;
            break;
        case 4: {
            hl_buf buf = {NULL, 0, 0};
            if (!hb_add(&buf, win) || !hb_add(&buf, HL_RM_VERBS) ||
                !hb_add(&buf, HL_PS_RECURSE_HL) || !hb_add(&buf, HL_RM_VERBS_TAIL) ||
                !hb_add(&buf, HL_PS_INCLUDE_NARROWS) || !hb_add(&buf, HL_ROOT_TAIL) ||
                !hb_add(&buf, HL_HARDLINE_DRIVE_QUOTED) || !hb_add(&buf, "|") ||
                !hb_add(&buf, HL_HARDLINE_DRIVE_ROOT) || !hb_add(&buf, HL_ROOT_END_WIN)) {
                free(buf.buf);
                pattern = NULL;
            } else {
                pattern = hb_take(&buf);
            }
            desc = DESC_ROOT_RM;
            break;
        }
        default: {
            hl_buf buf = {NULL, 0, 0};
            if (!hb_add(&buf, cmdpos ? cmdpos : "") || !hb_add(&buf, HL_RM_CMD) ||
                !hb_add(&buf, HL_PS_INCLUDE_NARROWS) || !hb_add(&buf, HL_ROOT_TAIL_RM) ||
                !hb_add(&buf, HL_HARDLINE_DRIVE_QUOTED) || !hb_add(&buf, "|") ||
                !hb_add(&buf, HL_HARDLINE_DRIVE_ROOT) || !hb_add(&buf, HL_ROOT_END)) {
                free(buf.buf);
                pattern = NULL;
            } else {
                pattern = hb_take(&buf);
            }
            desc = DESC_ROOT_RM;
            break;
        }
        }
        if (!pattern) { json_free(rules); free(win); return NULL; }
        {
            json_t *pair = json_array();
            json_append(pair, json_string(pattern));
            json_append(pair, json_string(desc));
            json_append(rules, pair);
        }
        free(pattern);
    }
    free(win);
    return rules;
}

json_t *w21_approvalwi_56c32a_windows_whatif_strict_patterns(const char *cmdpos)
{
    json_t *out = json_object();
    char *win = w21_approvalwi_56c32a_win_cmdpos(cmdpos);
    char *shutdown_pattern;
    char *clear_disk_pattern;

    if (!out || !win) { json_free(out); free(win); return NULL; }
    shutdown_pattern = hl_cat3(win, "(?:restart|stop)-computer", HL_CMDLET_END);
    clear_disk_pattern = hl_cat3(win, "clear-disk", HL_CMDLET_END);
    free(win);
    if (!shutdown_pattern || !clear_disk_pattern) {
        free(shutdown_pattern);
        free(clear_disk_pattern);
        json_free(out);
        return NULL;
    }
    json_set(out, DESC_SHUTDOWN, json_string(shutdown_pattern));
    json_set(out, DESC_CLEAR_DISK, json_string(clear_disk_pattern));
    free(shutdown_pattern);
    free(clear_disk_pattern);
    return out;
}

/* --- PCRE2 evaluation ------------------------------------------------------- */

static int hl_search(const char *pattern, const char *text, size_t len)
{
    pcre2_code *re;
    pcre2_match_data *md;
    int errorcode = 0;
    PCRE2_SIZE erroffset = 0;
    int matched;

    if (!pattern || !text) return 0;
    re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS,
                       &errorcode, &erroffset, NULL);
    if (!re) return 0;
    md = pcre2_match_data_create_from_pattern(re, NULL);
    if (!md) { pcre2_code_free(re); return 0; }
    matched = pcre2_match(re, (PCRE2_SPTR)text, len, 0, 0, md, NULL) >= 0;
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return matched;
}

/* re.split(r'[\r\n\x85\u2028\u2029;&|]', command): does any statement match? */
static int hl_statement_matches(const char *command)
{
    const char *start = command;
    const char *p = command;

    for (;;) {
        size_t len;
        int boundary = 0;
        size_t skip = 1;

        if (*p == '\0') {
            boundary = 1;
            skip = 0;
        } else if (*p == '\r' || *p == '\n' || *p == ';' || *p == '&' || *p == '|') {
            boundary = 1;
        } else if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) {
            boundary = 1;
            skip = 2;
        } else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 &&
                   ((unsigned char)p[2] == 0xA8 || (unsigned char)p[2] == 0xA9)) {
            boundary = 1;
            skip = 3;
        }
        if (boundary) {
            len = (size_t)(p - start);
            if (hl_search(HL_WHATIF_STMT, start, len)) return 1;
            if (skip == 0) return 0;
            p += skip;
            start = p;
            continue;
        }
        p++;
    }
}

json_t *w21_approvalwi_56c32a_whatif_preview_is_real(const char *command)
{
    const char *text = command ? command : "";

    if (hl_search(HL_WHATIF_OTHER, text, strlen(text))) return json_bool(0);
    return json_bool(hl_statement_matches(text) ? 0 : 1);
}
