/*
 * Oracle harness for gateway/platforms/helpers.py markdown chunking.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "platforms_helpers_md.h"

/* Internal: push a strdup'd copy onto a NULL-terminated strlist. */
char **helpers_md_strlist_push(char **list, const char *s);

static void json_print_str(const char *s)
{
    printf("\"");
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') printf("\\\"");
            else if (c == '\\') printf("\\\\");
            else if (c == '\n') printf("\\n");
            else if (c == '\r') printf("\\r");
            else if (c == '\t') printf("\\t");
            else if (c < 0x20) printf("\\u%04x", c);
            else printf("%c", c);
        }
    }
    printf("\"");
}

static void print_strlist(char **list)
{
    printf("[");
    for (size_t i = 0; list && list[i]; i++) {
        if (i > 0) printf(", ");
        json_print_str(list[i]);
    }
    printf("]");
}

static char **read_strlist(json_t *cj)
{
    char **chunks = NULL;
    if (cj && cj->type == JSON_ARRAY) {
        for (size_t j = 0; j < cj->c.count; j++) {
            json_t *e = json_get(cj, j);
            const char *v = e && e->type == JSON_STRING ? e->str_val : "";
            chunks = helpers_md_strlist_push(chunks, v);
        }
    }
    return chunks;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { free(err); return 1; }
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return 1;
    }
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *oj = json_obj_get(c, "op");
        const char *op = oj && oj->type == JSON_STRING ? oj->str_val : "";
        json_t *vj = json_obj_get(c, "value");
        const char *val = vj && vj->type == JSON_STRING ? vj->str_val : "";

        if (strcmp(op, "text_has_unclosed_fence") == 0)
            printf("%s\n", helpers_md_text_has_unclosed_fence(val) ? "True" : "False");
        else if (strcmp(op, "text_ends_with_table_row") == 0)
            printf("%s\n", helpers_md_text_ends_with_table_row(val) ? "True" : "False");
        else if (strcmp(op, "is_fence_atom") == 0)
            printf("%s\n", helpers_md_is_fence_atom(val) ? "True" : "False");
        else if (strcmp(op, "is_table_atom") == 0)
            printf("%s\n", helpers_md_is_table_atom(val) ? "True" : "False");
        else if (strcmp(op, "split_at_paragraph_boundary") == 0) {
            size_t maxc = (size_t)json_get_num(c, "max_chars", 50);
            char *tail = NULL;
            char *head = helpers_md_split_at_paragraph_boundary(val, maxc, &tail);
            printf("{\"head\":");
            json_print_str(head);
            printf(",\"tail\":");
            json_print_str(tail);
            printf("}\n");
            free(head); free(tail);
        }
        else if (strcmp(op, "split_markdown_atoms") == 0) {
            char **atoms = helpers_md_split_markdown_atoms(val);
            print_strlist(atoms);
            printf("\n");
            helpers_md_free_strlist(atoms);
        }
        else if (strcmp(op, "infer_block_separator") == 0) {
            json_t *nj = json_obj_get(c, "next");
            const char *nxt = nj && nj->type == JSON_STRING ? nj->str_val : "";
            printf("'");
            for (const char *s = helpers_md_infer_block_separator(val, nxt); *s; s++) {
                unsigned char c = (unsigned char)*s;
                if (c == '\n') printf("\\n");
                else if (c == '\r') printf("\\r");
                else if (c == '\t') printf("\\t");
                else if (c == '\'') printf("\\'");
                else printf("%c", c);
            }
            printf("'\n");
        }
        else if (strcmp(op, "merge_streaming_fences") == 0) {
            json_t *cj = json_obj_get(c, "chunks");
            char **chunks = read_strlist(cj);
            char **result = helpers_md_merge_streaming_fences(chunks);
            print_strlist(result);
            printf("\n");
            helpers_md_free_strlist(chunks);
            helpers_md_free_strlist(result);
        }
        else if (strcmp(op, "balance_fences_across_chunks") == 0) {
            json_t *cj = json_obj_get(c, "chunks");
            char **chunks = read_strlist(cj);
            char **result = helpers_md_balance_fences_across_chunks(chunks);
            print_strlist(result);
            printf("\n");
            helpers_md_free_strlist(chunks);
            helpers_md_free_strlist(result);
        }
        else if (strcmp(op, "split_text_fence_aware") == 0) {
            size_t limit = (size_t)json_get_num(c, "limit", 50);
            int pref = (int)json_get_num(c, "prefer_paragraphs", 1);
            int bal = (int)json_get_num(c, "balance_fences", 0);
            char **result = helpers_md_split_text_fence_aware(val, limit, pref, bal);
            print_strlist(result);
            printf("\n");
            helpers_md_free_strlist(result);
        }
        else {
            printf("UNKNOWN_OP\n");
        }
    }
    json_free(root);
    return 0;
}
