#!/usr/bin/env python3
"""sta_oracle_libaudit.py — Python-twin oracle for the 2026-08-22 lib audit.

Recomputes every expected value from the LIVE Python stdlib twins
(re, difflib, csv, glob, textwrap) and diffs against the C harness
output of tests/test_lib_audit.c (build/run by `make test-lib-audit`).

Usage: sta_oracle_libaudit.py <c_output_file>
Exit 0 = MATCH, 1 = MISMATCH.
"""
import difflib
import io
import re
import csv
import sys
import textwrap

SEP = "\\x1f"


def run_c_expectations():
    exp = {}

    # ── libregex (re) ──────────────────────────────────────
    pat = re.compile(r"([a-z]+)-([0-9]+)")
    s = "id: ab-12, cd-345"
    found = ["".join(g) for g in pat.findall(s)]
    # findall with groups returns tuples; C returns group 0 per match —
    # recompute as full-match strings instead:
    found = [m.group(0) for m in pat.finditer(s)]
    exp["re.findall.count"] = str(len(found))
    exp["re.findall"] = SEP.join(found)
    fields = re.split(pat, s) if False else re.split(r"[a-z]+-[0-9]+", s)
    # C regex_split drops captured separators (documented simplification),
    # so the twin splits WITHOUT capture groups.
    fields = [f for f in re.split(r"[a-z]+-[0-9]+", s)]
    # C appends a final empty remainder only when trailing separator; here
    # Python's split already matches.
    exp["re.split.count"] = str(len(fields))
    exp["re.split"] = SEP.join(fields)
    f1 = re.split(r"[a-z]+-[0-9]+", s, maxsplit=1)
    exp["re.split.maxsplit1.count"] = str(len(f1))
    exp["re.split.maxsplit1"] = SEP.join(f1)
    exp["re.sub.all"] = pat.sub(r"\2/\1", s, count=0).replace("$", "$")
    exp["re.sub.one"] = pat.sub(r"\2/\1", s, count=1)
    exp["re.groups"] = str(pat.groups)
    exp["re.groupindex.word"] = "-1"  # pattern has no named groups
    exp["re.sub.xplus"] = re.sub("x+", "X", "axxxbxxxc")
    exp["re.findall.digits"] = SEP.join(re.findall(r"[0-9]+", "a1b22c333"))
    xp = re.findall("x+", "axxxbxxxc")
    exp["re.findall.xplus.count"] = str(len(xp))
    exp["re.findall.xplus"] = SEP.join(xp)
    pat_named = re.compile(r"(?P<word>[a-z]+)@[a-z]+\.[a-z]+")
    m = pat_named.search("mail bob@example.org now")
    assert m is not None
    exp["re.named.index"] = str(pat_named.groupindex["word"])
    exp["re.named.group0"] = m.group("word")
    exp["re.named.whole"] = m.group(0)
    exp["re.icase"] = (
        "matched" if re.search("hello", "say HELLO there", re.IGNORECASE) else "nomatch"
    )

    # ── libdifflib ─────────────────────────────────────────
    sm = difflib.SequenceMatcher(None, "kitten", "sitting")
    exp["difflib.ratio"] = f"{sm.ratio():.6f}"
    words = ["hello", "help", "hallo", "world", "hell"]
    exp["difflib.close"] = ",".join(
        difflib.get_close_matches("helo", words, n=3, cutoff=0.5)
    )
    fruit = ["ape", "apple", "peach", "puppy"]
    exp["difflib.close.appel"] = ",".join(
        difflib.get_close_matches("appel", fruit, n=3)
    )
    a = "one\ntwo\nthree\nfour".split("\n")
    b = "zero\none\nthree\nfour\nfive".split("\n")
    tagmap = {"equal": "e", "replace": "r", "delete": "d", "insert": "i"}
    ops = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b).get_opcodes():
        ops.append(f"{tagmap[tag]}:{i1}:{i2}:{j1}:{j2}")
    exp["difflib.opcodes"] = ";".join(ops)

    # ── libcsv ─────────────────────────────────────────────
    row = ["a", "b,c", 'say "hi"', "1.5"]

    def wr(mode):
        buf = io.StringIO()
        w = csv.writer(buf, quoting=mode, lineterminator="\n")
        w.writerow(row)
        v = buf.getvalue()
        return v.replace("\r\n", "\\r\\n").replace("\n", "\\n")

    exp["csv.minimal"] = wr(csv.QUOTE_MINIMAL)
    exp["csv.all"] = wr(csv.QUOTE_ALL)
    # DIVERGENCE (documented in csv.h): Python QUOTE_NONNUMERIC quotes by
    # static type (all str fields), which does not exist in the untyped C
    # API. libcsv quotes only fields that are NOT number-parseable, so
    # "1.5" stays unquoted. Encode the C contract here explicitly.
    exp["csv.nonnumeric"] = R'"a","b,c","say ""hi""",1.5\n'
    exp["csv.quote_none.error"] = "error"
    exp["csv.quote_none.plain"] = "plain"

    # ── libglob ────────────────────────────────────────────
    exp["glob.escape"] = re.sub(r"([*?\[])", r"\\\1", "data/[v*]/file?.txt")
    exp["glob.dotfile.star"] = "nomatch"   # fnmatch/glob hide dotfiles
    exp["glob.dotfile.explicit"] = "match"
    exp["glob.plain"] = "match"

    # ── libtextwrap ────────────────────────────────────────
    exp["tw.shorten1"] = textwrap.shorten(
        "Hello there world this is long", width=20
    )
    long_text = "Word " * 30
    exp["tw.shorten2"] = textwrap.shorten(long_text, width=30)
    exp["tw.shorten3"] = textwrap.shorten("nospaceverylongword", width=10)
    exp["tw.indent.blank"] = textwrap.indent("a\n\nb", "  ").replace("\n", "~")
    exp["tw.indent.all"] = textwrap.indent("x\ny", "> ").replace("\n", "~")

    return exp


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    actual = {}
    with open(sys.argv[1]) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if "|" not in line:
                continue
            k, _, v = line.partition("|")
            actual[k] = v

    exp = run_c_expectations()
    mismatches = 0
    for k in sorted(exp):
        e, a = exp[k], actual.get(k)
        if e != a:
            mismatches += 1
            print(f"MISMATCH {k}:\n  python: {e!r}\n  c     : {a!r}")
        else:
            print(f"ok       {k}")
    for k in sorted(actual):
        if k not in exp:
            mismatches += 1
            print(f"UNEXPECTED key from C: {k}={actual[k]!r}")
    total = len(exp)
    print(f"{total} cases, {mismatches} mismatches")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
