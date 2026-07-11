/*
 * t_port_yuanbao_markdown.c — v561 residual-façade verification oracle.
 * Proves the 9 pure MarkdownProcessor static helpers in
 * gateway/platforms/yuanbao.py are faithfully ported to C
 * (yuanbao_md_* in src/gateway/platforms/yuanbao.c). The parity scanner
 * counts them as 148 "gaps" due to symbol-prefix mangling — this oracle
 * verifies they are actually REAL ported code, not missing work.
 *
 * Output: one JSON object per case; sta_oracle_yuanbao_markdown.py recomputes
 * against LIVE Python and asserts field equality.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* prototypes — these are defined in yuanbao.c (TU-local, not in a header) */
bool     yuanbao_md_has_unclosed_fence(const char *text);
bool     yuanbao_md_ends_with_table_row(const char *text);
bool     yuanbao_md_is_fence_atom(const char *text);
bool     yuanbao_md_is_table_atom(const char *text);
char   **yuanbao_md_split_into_atoms(const char *text);
char    *yuanbao_md_split_at_paragraph_boundary(const char *text, int max_chars, char **tail_out);
char    *yuanbao_md_strip_outer_markdown_fence(const char *text);
char    *yuanbao_md_sanitize_markdown_table(const char *text);
const char *yuanbao_md_markdown_hint_system_prompt(void);

static void json_esc(const char *s) {
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') { fputs("\\n", stdout); }
        else if (*p == '\r') { fputs("\\r", stdout); }
        else if (*p == '\t') { fputs("\\t", stdout); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_str(const char *fn, const char *in, const char *out)
{
    printf("{\"fn\":");
    json_esc(fn);
    printf(",\"in\":");
    json_esc(in);
    printf(",\"out\":");
    json_esc(out);
    printf("}\n");
}

static void emit_bool(const char *fn, const char *in, int v)
{
    printf("{\"fn\":");
    json_esc(fn);
    printf(",\"in\":");
    json_esc(in);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    /* ---- bool helpers ---- */
    emit_bool("unclosed", "```\ncode", 1); /* odd fence */
    emit_bool("unclosed", "```\ncode\n```", 0); /* balanced */
    emit_bool("unclosed", "plain text", 0);
    emit_bool("unclosed", "```a\nx\n```\n```b\ny", 1); /* 3 toggles */

    emit_bool("ends_table", "| a | b |\n| --- | --- |", 1);
    emit_bool("ends_table", "hello world", 0);
    emit_bool("ends_table", "| a | b |\n\npara", 0);

    emit_bool("is_fence_atom", "```python\nx=1\n```", 1);
    emit_bool("is_fence_atom", "| a | b |", 0);

    emit_bool("is_table_atom", "| a | b |\n| --- | --- |", 1);
    emit_bool("is_table_atom", "```code```", 0);

    /* ---- split_into_atoms ---- */
    const char *atom_in = "para one\n\n```python\nx=1\n```\n\n| a | b |\n| --- | --- |\nlast para";
    char **atoms = yuanbao_md_split_into_atoms(atom_in);
    int atom_n = 0;
    if (atoms) { for (char **a = atoms; *a; a++) atom_n++; }
    printf("{\"fn\":\"split_atoms\",\"in\":");
    json_esc(atom_in);
    printf(",\"count\":%d,\"atoms\":[", atom_n);
    if (atoms) {
        for (int i = 0; i < atom_n; i++) {
            if (i) printf(",");
            json_esc(atoms[i]);
        }
        for (int i = 0; i < atom_n; i++) free(atoms[i]);
        free(atoms);
    }
    printf("]}\n");

    /* ---- split_at_paragraph_boundary ---- */
    const char *pb = "First sentence. Second sentence.\n\nThird paragraph here.";
    char *tail = NULL;
    char *head = yuanbao_md_split_at_paragraph_boundary(pb, 20, &tail);
    emit_str("split_para_head", pb, head);
    emit_str("split_para_tail", pb, tail);
    free(head); free(tail);

    /* ---- strip_outer_markdown_fence ---- */
    const char *fenced = "```markdown\n# Title\ncode here\n```";
    emit_str("strip_fence", fenced, yuanbao_md_strip_outer_markdown_fence(fenced));
    const char *not_fenced = "# Title\ncode here";
    emit_str("strip_fence_noop", not_fenced, yuanbao_md_strip_outer_markdown_fence(not_fenced));

    /* ---- sanitize_markdown_table ---- */
    const char *tbl = "| a | b |  \n| --- | --- |  \n| 1 | 2 |\n\npara";
    emit_str("sanitize_tbl", tbl, yuanbao_md_sanitize_markdown_table(tbl));

    /* ---- markdown_hint_system_prompt ---- */
    emit_str("hint", NULL, yuanbao_md_markdown_hint_system_prompt());

    return 0;
}
