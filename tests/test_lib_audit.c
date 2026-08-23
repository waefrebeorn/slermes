/*
 * test_lib_audit.c — Dependency-library audit oracle (2026-08-22).
 *
 * Exercises the NEW Python-parity surface added to five recreated libs:
 *   libregex     (re)        : regex_findall / split / replace_n / named groups / flags
 *   libdifflib   (difflib)   : difflib_seq_ratio / get_close_matches / get_opcodes
 *   libcsv       (csv)       : quoting modes (MINIMAL/ALL/NONNUMERIC/NONE)
 *   libglob      (glob)      : glob_escape + dotfile-hiding rule
 *   libtextwrap  (textwrap)  : shorten_py / indent
 *
 * Prints deterministic "key|value" lines. The Python twin
 * tests/sta_oracle_libaudit.py recomputes every expected value from the
 * live Python stdlib modules and diffs. Run via:  make test-lib-audit
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_regex.h"
#include "difflib.h"
#include "csv.h"
#include "hermes_glob.h"
#include "textwrap.h"

static void pv(const char *k, const char *v) { printf("%s|%s\n", k, v ? v : "(null)"); }
static void pi(const char *k, long v)         { printf("%s|%ld\n", k, v); }

static void print_str_array(const char *prefix, char **arr, int n) {
    printf("%s|", prefix);
    for (int i = 0; i < n; i++) printf("%s%s", i ? "\\x1f" : "", arr[i] ? arr[i] : "");
    printf("\n");
}

int main(void) {
    /* ── libregex ─────────────────────────────────────────── */
    {
        hregex_t *re = regex_compile("([a-z]+)-([0-9]+)", 0);
        const char *s = "id: ab-12, cd-345";

        char *found[8]; int nf = regex_findall(re, s, found, 8);
        pi("re.findall.count", nf);
        print_str_array("re.findall", found, nf);
        for (int i = 0; i < nf; i++) free(found[i]);

        char *fields[8]; int ns = regex_split(re, s, -1, fields, 8);
        pi("re.split.count", ns);
        print_str_array("re.split", fields, ns);
        regex_split_free(fields, ns);

        /* maxsplit=1 */
        ns = regex_split(re, s, 1, fields, 8);
        pi("re.split.maxsplit1.count", ns);
        print_str_array("re.split.maxsplit1", fields, ns);
        regex_split_free(fields, ns);

        char *r = regex_replace_n(re, s, "$2/$1", 0);       /* count=0 => ALL */
        pv("re.sub.all", r); free(r);
        r = regex_replace_n(re, s, "$2/$1", 1);             /* count=1 */
        pv("re.sub.one", r); free(r);

        pi("re.groups", regex_group_count(re));
        pi("re.groupindex.word", regex_group_index(re, "word"));
        regex_free(re);

        /* global sub of x+ */
        hregex_t *rx = regex_compile("x+", 0);
        r = regex_replace_n(rx, "axxxbxxxc", "X", 0);
        pv("re.sub.xplus", r); free(r);
        nf = regex_findall(rx, "axxxbxxxc", found, 8);
        pi("re.findall.xplus.count", nf);
        print_str_array("re.findall.xplus", found, nf);
        for (int i = 0; i < nf; i++) free(found[i]);
        regex_free(rx);

        /* digits findall */
        hregex_t *rd = regex_compile("[0-9]+", 0);
        nf = regex_findall(rd, "a1b22c333", found, 8);
        print_str_array("re.findall.digits", found, nf);
        for (int i = 0; i < nf; i++) free(found[i]);
        regex_free(rd);

        /* named group */
        hregex_t *rn = regex_compile("(?P<word>[a-z]+)@[a-z]+\\.[a-z]+", 0);
        pi("re.named.index", regex_group_index(rn, "word"));
        regex_match_t *m = regex_search(rn, "mail bob@example.org now");
        if (m && m->matched) {
            int gi = regex_group_index(rn, "word");
            if (gi > 0 && gi < REGEX_MAX_GROUPS && m->groups[gi]) pv("re.named.group0", m->groups[gi]);
            if (m->groups[0]) pv("re.named.whole", m->groups[0]);
        } else pv("re.named.nomatch", "true");
        regex_match_free(m);
        regex_free(rn);

        /* ICASE flag */
        hregex_t *ri = regex_compile("hello", REGEX_FLAG_ICASE);
        regex_match_t *mi = regex_match(ri, "say HELLO there");
        pv("re.icase", (mi && mi->matched) ? "matched" : "nomatch");
        regex_match_free(mi);
        regex_free(ri);
    }

    /* ── libdifflib ───────────────────────────────────────── */
    {
        double ratio = difflib_seq_ratio("kitten", "sitting");
        printf("difflib.ratio|%.6f\n", ratio);

        const char *words[] = {"hello", "help", "hallo", "world", "hell"};
        const char *out[4];
        int n = difflib_get_close_matches("helo", words, 5, 3, 0.5, out, 4);
        printf("difflib.close|");
        for (int i = 0; i < n; i++) printf("%s%s", i ? "," : "", out[i]);
        printf("\n");

        const char *fruit[] = {"ape", "apple", "peach", "puppy"};
        n = difflib_get_close_matches("appel", fruit, 4, 3, 0.6, out, 4);
        printf("difflib.close.appel|");
        for (int i = 0; i < n; i++) printf("%s%s", i ? "," : "", out[i]);
        printf("\n");

        difflib_opcode_t ops[32];
        n = difflib_get_opcodes(
            "one\ntwo\nthree\nfour",
            "zero\none\nthree\nfour\nfive",
            ops, 32);
        printf("difflib.opcodes|");
        for (int i = 0; i < n; i++)
            printf("%s%c:%d:%d:%d:%d", i ? ";" : "", ops[i].tag,
                   ops[i].i1, ops[i].i2, ops[i].j1, ops[i].j2);
        printf("\n");
    }

    /* ── libcsv ───────────────────────────────────────────── */
    {
        const char *row[] = {"a", "b,c", "say \"hi\"", "1.5"};
        FILE *fp;
        char buf[512];
        static const struct { const char *name; csv_quote_mode_t mode; } modes[] = {
            {"minimal", CSV_QUOTE_MINIMAL},
            {"all", CSV_QUOTE_ALL},
            {"nonnumeric", CSV_QUOTE_NONNUMERIC},
        };
        for (size_t k = 0; k < sizeof(modes)/sizeof(modes[0]); k++) {
            fp = tmpfile();
            csv_writer_t *w = csv_writer_open_file(fp, ',');
            csv_writer_set_quote_mode(w, modes[k].mode);
            csv_writer_write_row(w, row, 4);
            rewind(fp);
            size_t nread = fread(buf, 1, sizeof(buf) - 1, fp);
            buf[nread] = '\0';
            /* normalize \r\n -> \\r\\n for stable printing */
            printf("csv.%s|", modes[k].name);
            for (char *p = buf; *p; p++) {
                if (*p == '\r') printf("\\r");
                else if (*p == '\n') printf("\\n");
                else putchar(*p);
            }
            printf("\n");
            csv_writer_close(w);
        }
        /* QUOTE_NONE with special chars must fail like Python's csv.Error */
        char *esc = csv_escape_field("b,c", ',', CSV_QUOTE_NONE);
        pv("csv.quote_none.error", esc ? "no-error" : "error");
        free(esc);
        esc = csv_escape_field("plain", ',', CSV_QUOTE_NONE);
        pv("csv.quote_none.plain", esc);
        free(esc);
    }

    /* ── libglob ──────────────────────────────────────────── */
    {
        char *e = glob_escape("data/[v*]/file?.txt");
        pv("glob.escape", e); free(e);

        /* dotfile rule: '*' must not match .hidden */
        pv("glob.dotfile.star", glob_match("*", ".hidden") ? "match" : "nomatch");
        pv("glob.dotfile.explicit", glob_match(".*", ".hidden") ? "match" : "nomatch");
        pv("glob.plain", glob_match("*.txt", "notes.txt") ? "match" : "nomatch");
    }

    /* ── libtextwrap ──────────────────────────────────────── */
    {
        char *r = textwrap_shorten_py("Hello there world this is long", 20);
        pv("tw.shorten1", r); free(r);
        r = textwrap_shorten_py("Word Word Word Word Word Word Word Word Word Word "
                                "Word Word Word Word Word Word Word Word Word Word "
                                "Word Word Word Word Word Word Word Word Word Word ", 30);
        pv("tw.shorten2", r); free(r);
        r = textwrap_shorten_py("nospaceverylongword", 10);
        pv("tw.shorten3", r); free(r);
        r = textwrap_indent("a\n\nb", "  ");
        pv("tw.indent.blank", r); free(r);
        r = textwrap_indent("x\ny", "> ");
        pv("tw.indent.all", r); free(r);
    }

    return 0;
}
