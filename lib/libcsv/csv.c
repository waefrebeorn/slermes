/*
 * csv.c — C CSV parsing/writing library (J06: Python csv port).
 *
 * RFC 4180 compliant CSV reader and writer with configurable delimiter.
 * Handles: quoted fields, escaped quotes (""), embedded delimiters,
 * embedded newlines in quoted fields, and variable-width rows.
 */

#define _POSIX_C_SOURCE 200809L
#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ================================================================
 *  Writer
 * ================================================================ */

struct csv_writer_t {
    FILE *fp;
    char  delimiter;
    bool  owned_fp;  /* true if we should fclose on close */
    csv_quote_mode_t quote_mode;  /* default MINIMAL (0 via calloc) */
};

csv_writer_t *csv_writer_open(const char *path, char delimiter) {
    if (!path || !*path) return NULL;
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    csv_writer_t *w = (csv_writer_t *)calloc(1, sizeof(csv_writer_t));
    if (!w) { fclose(fp); return NULL; }
    w->fp = fp;
    w->delimiter = delimiter;
    w->owned_fp = true;
    return w;
}

csv_writer_t *csv_writer_open_file(FILE *fp, char delimiter) {
    if (!fp) return NULL;
    csv_writer_t *w = (csv_writer_t *)calloc(1, sizeof(csv_writer_t));
    if (!w) return NULL;
    w->fp = fp;
    w->delimiter = delimiter;
    w->owned_fp = false;
    return w;
}

bool csv_writer_set_quote_mode(csv_writer_t *w, csv_quote_mode_t mode) {
    if (!w) return false;
    w->quote_mode = mode;
    return true;
}

csv_quote_mode_t csv_writer_quote_mode(csv_writer_t *w) {
    return w ? w->quote_mode : CSV_QUOTE_MINIMAL;
}

/* PoP: Python csv writer's _needs_ quoting decision + QUOTE_NONNUMERIC's
 * numeric test (float(value) parseability). */
static bool field_is_numeric(const char *field) {
    if (!field || !*field) return false;
    char *end = NULL;
    (void)strtod(field, &end);
    return end && *end == '\0';
}

char *csv_escape_field(const char *field, char delimiter, csv_quote_mode_t mode) {
    if (!field) field = "";
    bool need_quote = false;
    bool special = false;
    for (const char *p = field; *p; p++) {
        if (*p == '"' || *p == delimiter || *p == '\n' || *p == '\r') {
            special = true;
            break;
        }
    }
    switch (mode) {
    case CSV_QUOTE_ALL:
        need_quote = true;
        break;
    case CSV_QUOTE_NONNUMERIC:
        need_quote = !field_is_numeric(field);
        break;
    case CSV_QUOTE_NONE:
        if (special) return NULL;   /* PoP: csv.Error("need to escape...") */
        need_quote = false;
        break;
    case CSV_QUOTE_MINIMAL:
    default:
        need_quote = special;
        break;
    }

    size_t flen = strlen(field);
    if (!need_quote) return strdup(field);
    /* quoted: len + 2 quotes + doubling of internal quotes */
    char *out = (char *)malloc(flen * 2 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = '"';
    for (const char *p = field; *p; p++) {
        if (*p == '"') out[o++] = '"';
        out[o++] = *p;
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

/* Escape and write a single field. Handles: embedded delimiter, quote, newline. */
static void write_escaped_field(FILE *fp, const char *field, char delimiter,
                                csv_quote_mode_t mode) {
    char *esc = csv_escape_field(field, delimiter, mode);
    if (!esc) {
        /* QUOTE_NONE data loss — write raw (writer returns false via
         * csv_escape_field contract; here we preserve legacy raw write). */
        fputs(field ? field : "", fp);
        return;
    }
    fputs(esc, fp);
    free(esc);
}

bool csv_writer_write_row(csv_writer_t *w, const char **fields, int count) {
    if (!w || !w->fp || !fields || count < 0) return false;

    for (int i = 0; i < count; i++) {
        if (i > 0) fputc(w->delimiter, w->fp);
        if (w->quote_mode == CSV_QUOTE_NONE) {
            char *chk = csv_escape_field(fields[i], w->delimiter, CSV_QUOTE_NONE);
            if (!chk) return false;   /* PoP: csv.Error */
            free(chk);
        }
        write_escaped_field(w->fp, fields[i], w->delimiter, w->quote_mode);
    }
    fputc('\n', w->fp);
    return true;
}

bool csv_writer_write_row_va(csv_writer_t *w, int count, ...) {
    if (!w || count < 0) return false;

    va_list args;
    va_start(args, count);
    const char **fields = (const char **)calloc((size_t)count, sizeof(char *));
    if (!fields) { va_end(args); return false; }

    for (int i = 0; i < count; i++)
        fields[i] = va_arg(args, const char *);

    bool result = csv_writer_write_row(w, fields, count);
    free(fields);
    va_end(args);
    return result;
}

void csv_writer_close(csv_writer_t *w) {
    if (!w) return;
    if (w->owned_fp && w->fp)
        fclose(w->fp);
    w->fp = NULL;
    free(w);
}

/* ================================================================
 *  Reader
 * ================================================================ */

struct csv_reader_t {
    char  *data;         /* owned copy of string data (or NULL for file mode) */
    size_t data_len;
    size_t data_pos;
    FILE  *fp;           /* file pointer (or NULL for string mode) */
    bool   owned_fp;     /* true if we should fclose */
    char   delimiter;
    int    row_number;
    char   line_buf[CSV_MAX_LINE];
};

csv_reader_t *csv_reader_open(const char *path, char delimiter) {
    if (!path || !*path) return NULL;
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    csv_reader_t *r = (csv_reader_t *)calloc(1, sizeof(csv_reader_t));
    if (!r) { fclose(fp); return NULL; }
    r->fp = fp;
    r->delimiter = delimiter;
    r->owned_fp = true;
    r->row_number = 0;
    return r;
}

csv_reader_t *csv_reader_open_file(FILE *fp, char delimiter) {
    if (!fp) return NULL;
    csv_reader_t *r = (csv_reader_t *)calloc(1, sizeof(csv_reader_t));
    if (!r) return NULL;
    r->fp = fp;
    r->delimiter = delimiter;
    r->owned_fp = false;
    r->row_number = 0;
    return r;
}

csv_reader_t *csv_reader_open_string(const char *data, size_t len, char delimiter) {
    if (!data) return NULL;
    if (len == 0) len = strlen(data);
    csv_reader_t *r = (csv_reader_t *)calloc(1, sizeof(csv_reader_t));
    if (!r) return NULL;
    r->data = (char *)malloc(len + 1);
    if (!r->data) { free(r); return NULL; }
    memcpy(r->data, data, len);
    r->data[len] = '\0';
    r->data_len = len;
    r->data_pos = 0;
    r->delimiter = delimiter;
    r->row_number = 0;
    return r;
}

/* Read one line from string source. Returns false at EOF. */
static bool read_line_string(csv_reader_t *r) {
    if (r->data_pos >= r->data_len) return false;

    size_t i = 0;
    bool in_quotes = false;
    while (r->data_pos < r->data_len && i < CSV_MAX_LINE - 1) {
        char c = r->data[r->data_pos++];
        r->line_buf[i++] = c;

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == '\n' && !in_quotes) {
            break;
        }
    }
    r->line_buf[i] = '\0';
    /* Strip trailing newline/carriage return */
    while (i > 0 && (r->line_buf[i-1] == '\n' || r->line_buf[i-1] == '\r'))
        r->line_buf[--i] = '\0';
    return i > 0;
}

/* Read one line from file source. Returns false at EOF. */
static bool read_line_file(csv_reader_t *r) {
    if (!r->fp || feof(r->fp)) return false;

    /* Handle quoted fields with embedded newlines */
    size_t i = 0;
    bool in_quotes = false;
    while (i < CSV_MAX_LINE - 1) {
        int c = fgetc(r->fp);
        if (c == EOF) break;

        r->line_buf[i++] = (char)c;

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == '\n' && !in_quotes) {
            break;
        }
    }
    r->line_buf[i] = '\0';
    /* Strip trailing newline/carriage return */
    while (i > 0 && (r->line_buf[i-1] == '\n' || r->line_buf[i-1] == '\r'))
        r->line_buf[--i] = '\0';
    return i > 0;
}

/* Read next line from the source. */
static bool csv_read_next_line(csv_reader_t *r) {
    if (r->data)
        return read_line_string(r);
    else
        return read_line_file(r);
}

/* Parse a single CSV line into fields. Handles RFC 4180 quoting. */
static char **parse_csv_line(const char *line, char delimiter, int *out_count) {
    if (!line || !out_count) return NULL;

    const char **fields = (const char **)calloc(CSV_MAX_FIELDS, sizeof(char *));
    if (!fields) return NULL;

    int count = 0;
    const char *p = line;

    while (*p && count < CSV_MAX_FIELDS) {
        /* Skip leading whitespace? No — CSV doesn't trim by default. */

        /* Allocate buffer for this field (max line length) */
        size_t cap = strlen(line) + 1;
        char *buf = (char *)malloc(cap);
        if (!buf) goto error;
        size_t pos = 0;

        if (*p == '"') {
            /* Quoted field */
            p++; /* skip opening quote */
            while (*p && pos < cap - 1) {
                if (*p == '"') {
                    p++;
                    if (*p == '"') {
                        /* Escaped quote ("") */
                        buf[pos++] = '"';
                        p++;
                    } else {
                        /* End of quoted field */
                        break;
                    }
                } else {
                    buf[pos++] = *p++;
                }
            }
            /* Skip trailing whitespace before delimiter/end */
            while (*p == ' ' || *p == '\t') p++;
        } else {
            /* Unquoted field — read until delimiter or end */
            while (*p && *p != delimiter && *p != '\n' && *p != '\r' && pos < cap - 1) {
                buf[pos++] = *p++;
            }
        }

        buf[pos] = '\0';
        fields[count++] = buf;

        /* Skip delimiter */
        if (*p == delimiter) {
            p++;
            /* Skip whitespace after delimiter */
            while (*p == ' ' || *p == '\t') p++;
        }
    }

    *out_count = count;
    return (char **)fields;

error:
    for (int i = 0; i < count; i++) free((void *)fields[i]);
    free(fields);
    return NULL;
}

char **csv_reader_read_row(csv_reader_t *r, int *count) {
    if (!r || !count) return NULL;
    *count = 0;

    if (!csv_read_next_line(r))
        return NULL;

    r->row_number++;
    return parse_csv_line(r->line_buf, r->delimiter, count);
}

int csv_reader_row_number(csv_reader_t *r) {
    return r ? r->row_number : 0;
}

void csv_reader_close(csv_reader_t *r) {
    if (!r) return;
    if (r->owned_fp && r->fp)
        fclose(r->fp);
    free(r->data);
    free(r);
}

/* ================================================================
 *  String helpers
 * ================================================================ */

char *csv_escape(const char *field, char delimiter) {
    if (!field) return strdup("");

    bool need_quote = false;
    for (const char *p = field; *p; p++) {
        if (*p == '"' || *p == delimiter || *p == '\n' || *p == '\r') {
            need_quote = true;
            break;
        }
    }

    if (!need_quote)
        return strdup(field);

    /* Quote + escape */
    size_t cap = strlen(field) * 2 + 3;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;

    size_t j = 0;
    result[j++] = '"';
    for (const char *p = field; *p; p++) {
        if (*p == '"') result[j++] = '"';  /* double it */
        result[j++] = *p;
    }
    result[j++] = '"';
    result[j] = '\0';
    return result;
}

char *csv_unescape(const char *field, char delimiter) {
    (void)delimiter;
    if (!field) return strdup("");
    if (*field != '"') {
        /* Also unescape internal double-quotes that might be inside an unquoted field */
        return strdup(field);
    }

    /* Remove surrounding quotes and unescape doubled quotes.
     * Input: "he""llo" → Output: he"llo
     * The field starts with " and ends with ". */
    size_t len = strlen(field);
    if (len < 2) return strdup(field);

    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    size_t j = 0;
    const char *p = field + 1;  /* skip opening quote */
    while (*p) {
        if (*p == '"') {
            if (*(p + 1) == '"') {
                /* Escaped double quote "" → " */
                result[j++] = '"';
                p += 2;
            } else {
                /* Closing quote — skip it and any trailing whitespace */
                p++;
                break;
            }
        } else {
            result[j++] = *p++;
        }
    }
    result[j] = '\0';
    return result;
}

char **csv_parse_line(const char *line, char delimiter, int *count) {
    return parse_csv_line(line, delimiter, count);
}

void csv_free_fields(char **fields, int count) {
    if (!fields) return;
    for (int i = 0; i < count; i++)
        free(fields[i]);
    free(fields);
}
