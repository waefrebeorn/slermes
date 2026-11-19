/*
 * hermes_state_common.c — canonical C11 port of hermes_state_common.py
 *
 * Shared module-level constants + SQL builders for the SessionDB family
 * (hermes_state_search / _schema / _portability reference these without
 * importing hermes_state, mirroring the Python module split).
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Constants (hermes_state_common.py) ──────────────────────────────── */

/* PoP: hs_common_preview_head_chars @ hermes_state_common.py:_PREVIEW_HEAD_CHARS */
int hs_common_preview_head_chars(void) { return 63; }

/* PoP: hs_common_preview_scaffold_window @ hermes_state_common.py:_PREVIEW_SCAFFOLD_WINDOW */
int hs_common_preview_scaffold_window(void) { return 400; }

/* PoP: hs_common_preview_max_chars @ hermes_state_common.py:_PREVIEW_MAX_CHARS */
int hs_common_preview_max_chars(void) { return 60; }

/* PoP: hs_common_schema_version @ hermes_state_common.py:SCHEMA_VERSION */
int hs_common_schema_version(void) { return 26; }

/* PoP: hs_common_fts_storage_version @ hermes_state_common.py:FTS_STORAGE_VERSION */
int hs_common_fts_storage_version(void) { return 1; }

/* SKILL_EXCERPT_JOINT = "\x1e" (agent/skill_commands.py) */
#define HSC_SKILL_EXCERPT_JOINT "\x1e"

/* ── SQL text helpers ────────────────────────────────────────────────── */

/* PoP: hs_common_escape_like @ hermes_state_common.py:escape_like */
void hs_common_escape_like(const char *text, char *out, size_t out_sz) {
    size_t o = 0;
    for (const char *p = text; *p && o + 3 < out_sz; p++) {
        if (*p == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (*p == '%') { out[o++] = '\\'; out[o++] = '%'; }
        else if (*p == '_') { out[o++] = '\\'; out[o++] = '_'; }
        else out[o++] = *p;
    }
    out[o] = '\0';
}

/* PoP: hs_common_sql_literal @ hermes_state_common.py:_sql_literal */
void hs_common_sql_literal(const char *text, char *out, size_t out_sz) {
    size_t o = 0;
    if (o < out_sz) out[o++] = '\'';
    for (const char *p = text; *p && o + 2 < out_sz; p++) {
        out[o++] = *p;
        if (*p == '\'' && o < out_sz - 1) out[o++] = '\'';
    }
    if (o < out_sz) out[o++] = '\'';
    out[o] = '\0';
}

/* PoP: hs_common_sql_ltrim_whitespace @ hermes_state_common.py:_sql_ltrim_whitespace */
void hs_common_sql_ltrim_whitespace(const char *expr, char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "LTRIM(%s, CHAR(9) || CHAR(10) || CHAR(13) || CHAR(32))", expr);
}

/* PoP: hs_common_sql_trim_whitespace @ hermes_state_common.py:_sql_trim_whitespace */
void hs_common_sql_trim_whitespace(const char *expr, char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "TRIM(%s, CHAR(9) || CHAR(10) || CHAR(13) || CHAR(32))", expr);
}

/* PoP: hs_common_sql_starts_with @ hermes_state_common.py:_sql_starts_with */
void hs_common_sql_starts_with(const char *expr, char *const *prefixes,
                               char *out, size_t out_sz) {
    char trimmed[4096];
    hs_common_sql_ltrim_whitespace(expr, trimmed, sizeof trimmed);
    size_t o = 0;
    o += (size_t)snprintf(out + o, out_sz - o, "(");
    for (int i = 0; prefixes[i]; i++) {
        char lit[512];
        hs_common_sql_literal(prefixes[i], lit, sizeof lit);
        o += (size_t)snprintf(out + o, out_sz - o,
            "%sSUBSTR(%s, 1, %zu) = %s",
            i ? " OR " : "", trimmed, strlen(prefixes[i]), lit);
        if (o >= out_sz) break;
    }
    if (o < out_sz) out[o++] = ')';
    out[o] = '\0';
}

/* PoP: hs_common_shape_preview @ hermes_state_common.py:_shape_preview */
void hs_common_shape_preview(const char *raw, char *out, size_t out_sz) {
    if (!raw) raw = "";
    /* copy + collapse newlines to spaces + trim (Python .strip()) */
    char text[4096];
    size_t o = 0;
    const char *start = raw;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    const char *end = start + strlen(start);
    while (end > start && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\n'||end[-1]=='\r')) end--;
    for (const char *p = start; p < end && o + 1 < sizeof text; p++)
        text[o++] = (*p=='\n'||*p=='\r') ? ' ' : *p;
    text[o] = '\0';

    /* Python: described = describe_skill_invocation(text); if None:
     * text.split(SKILL_EXCERPT_JOINT)[0] */
    extern char *describe_skill_invocation(const char *content);
    char *described = describe_skill_invocation(text);
    if (described) {
        snprintf(text, sizeof text, "%s", described);
        free(described);
    } else {
        char *joint = strchr(text, HSC_SKILL_EXCERPT_JOINT[0]);
        if (joint) *joint = '\0';
    }

    int maxc = hs_common_preview_max_chars();
    if ((int)strlen(text) > maxc) {
        snprintf(out, out_sz, "%.*s...", maxc, text);
    } else {
        snprintf(out, out_sz, "%s", text);
    }
}

/* PoP: hs_common_reset_end_reasons_csv @ hermes_state_common.py:_RESET_END_REASONS_SQL */
void hs_common_reset_end_reasons_csv(char *out, size_t out_sz) {
    static const char *const reasons[] = {
        "session_reset","session_switch","idle","daily","suspended",
        "resume_pending_expired", NULL
    };
    size_t o = 0;
    for (int i = 0; reasons[i]; i++) {
        o += (size_t)snprintf(out + o, out_sz - o, "%s'%s'",
                              i ? ", " : "", reasons[i]);
        if (o >= out_sz) break;
    }
}

/* PoP: hs_common_legacy_reset_child_sql @ hermes_state_common.py:_legacy_reset_child_sql */
void hs_common_legacy_reset_child_sql(const char *alias, const char *reasons_sql,
                                      char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "EXISTS (SELECT 1 FROM sessions p"
        "            WHERE p.id = %s.parent_session_id"
        "            AND p.end_reason IN (%s)"
        "            AND %s.session_key IS NOT NULL"
        "            AND %s.session_key != ''"
        "            AND %s.session_key = p.session_key)",
        alias, reasons_sql, alias, alias, alias);
}

/* PoP: hs_common_ephemeral_child_sql @ hermes_state_common.py:_ephemeral_child_sql */
void hs_common_ephemeral_child_sql(const char *alias, char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "(%s.parent_session_id IS NOT NULL"
        " AND NOT (json_extract(COALESCE(%s.model_config, '{}'), '$._branched_from') IS NOT NULL"
        " OR EXISTS (SELECT 1 FROM sessions p"
        "            WHERE p.id = %s.parent_session_id"
        "            AND p.end_reason = 'branched'"
        "            AND %s.started_at >= p.ended_at))"
        " AND NOT (EXISTS (SELECT 1 FROM sessions p"
        "        WHERE p.id = %s.parent_session_id"
        "        AND p.end_reason = 'compression'))"
        " AND NOT (json_extract(COALESCE(%s.model_config, '{}'), '$._reset_from') IS NOT NULL"
        " OR EXISTS (SELECT 1 FROM sessions p"
        "            WHERE p.id = %s.parent_session_id"
        "            AND p.end_reason IN ('session_reset','session_switch','idle','daily','suspended','resume_pending_expired')"
        "            AND %s.session_key IS NOT NULL"
        "            AND %s.session_key != ''"
        "            AND %s.session_key = p.session_key)))",
        alias, alias, alias, alias, alias, alias, alias, alias, alias);
}

/* PoP: hs_common_sql_session_last_active @ hermes_state_common.py:_sql_session_last_active */
void hs_common_sql_session_last_active(const char *alias, char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "COALESCE("
        "(SELECT MAX(_act_v.v) FROM ("
        "SELECT %s.last_activity_at AS v "
        "UNION ALL "
        "SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m "
        "WHERE _act_m.session_id = %s.id)"
        ") _act_v), "
        "%s.started_at)",
        alias, alias, alias);
}

/* PoP: hs_common_sql_session_last_active_by_id @ hermes_state_common.py:_sql_session_last_active_by_id */
void hs_common_sql_session_last_active_by_id(const char *session_id_expr,
                                             char *out, size_t out_sz) {
    snprintf(out, out_sz,
        "COALESCE("
        "(SELECT MAX(_act_v.v) FROM ("
        "SELECT (SELECT last_activity_at FROM sessions _act_s "
        "WHERE _act_s.id = %s) AS v "
        "UNION ALL "
        "SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m "
        "WHERE _act_m.session_id = %s)"
        ") _act_v), "
        "(SELECT started_at FROM sessions _act_s "
        "WHERE _act_s.id = %s))",
        session_id_expr, session_id_expr, session_id_expr);
}

/* ── Shared DDL accessors (DDL itself lives in hermes_state_open.c —
 *    extend-don't-duplicate; these accessors mirror hermes_state_common.py's
 *    re-export of the constants for the sibling modules) ── */

/* PoP: hs_common_schema_sql @ hermes_state_common.py:SCHEMA_SQL */
const char *hs_common_schema_sql(void) {
    extern const char *HERMES_STATE_SCHEMA_SQL;
    return HERMES_STATE_SCHEMA_SQL;
}
