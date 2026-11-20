#!/usr/bin/env python3
"""Generate tests/fixtures/fuzzy_match_flow2.jsonl for the fuzzy_match_flow2
oracle. Each case is {"func": <name>, "in": {...}} with scripted seams in in.io
(plain value reused; {"__seq__":[...]} per-call, last repeats;
{"__raise__":kind,"msg":text} raises; scripted null -> None; ABSENT ->
fail-closed RuntimeError). >=5 cases per ported function, covering decision
branches, short-circuits, fail-closed seams, BaseException passthrough and
correction_notes mutation order. All offset-based cases are ASCII (byte
offset == code point); no exotic Unicode whitespace in strip positions.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "fuzzy_match_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def seq(*vals):
    return {"__seq__": list(vals)}


IMPORT = "import_note_edit_match"
NOTE = "note_edit_match"


# ── window_spans ──
add("window_spans", content="abc\ndef\nghi", content_lines=["abc", "def", "ghi"], n=1,
    io={"accept": seq(True, False, True)})
add("window_spans", content="abc\ndef\nghi", content_lines=["abc", "def", "ghi"], n=2,
    io={"accept": seq(True, False)})
add("window_spans", content="abc\ndef\nghi", content_lines=["abc", "def", "ghi"], n=4, io={})
add("window_spans", content="", content_lines=[""], n=1, io={"accept": seq(False)})
add("window_spans", content="a\nb", content_lines=["a", "b"], n=1,
    io={"accept": seq(1, 0)})
add("window_spans", content="a\nb\nc", content_lines=["a", "b", "c"], n=1,
    io={"accept": R("ValueError", "bad accept")})
add("window_spans", content="a\nb\nc", content_lines=["a", "b", "c"], n=1,
    io={"accept": seq(True, True, True)})
add("window_spans", content="a\nb", content_lines=["a", "b"], n=1, io={})  # fail-closed

# ── match_transformed_lines ──
add("match_transformed_lines", content="A\nB\nC", pattern="B",
    io={"transform": seq(["B"], ["A"], ["B"], ["C"])})
add("match_transformed_lines", content="A\nB\nC", pattern="A\nB",
    io={"transform": seq(["A", "B"], ["A", "B"], ["B", "C"])})
add("match_transformed_lines", content="A\nB\nC", pattern="Z",
    io={"transform": seq(["Z"], ["X"], ["Y"], ["W"])})
add("match_transformed_lines", content="", pattern="",
    io={"transform": seq([""], [""])})
add("match_transformed_lines", content="A\nB", pattern="B",
    io={"transform": R("RuntimeError", "tf boom")})
add("match_transformed_lines", content="a\nb", pattern="b", io={})  # fail-closed
add("match_transformed_lines", content="x\ny\nz", pattern="x",
    io={"transform": seq(["x"], ["x"], ["y"], ["z"])})

# ── strip_boundary ──
add("strip_boundary", lines=["  a  ", " b ", "c  "])
add("strip_boundary", lines=["  a  "])
add("strip_boundary", lines=[])
add("strip_boundary", lines=["\tX\t", "Y", "  Z  "])
add("strip_boundary", lines=["   "])
add("strip_boundary", lines=["a", "b"])
add("strip_boundary", lines=[" a ", " b ", " c "])

# ── invert_norm_map ──
add("invert_norm_map", orig_to_norm=[0, 1, 2, 3])
add("invert_norm_map", orig_to_norm=[0, 0, 1, 1])
add("invert_norm_map", orig_to_norm=[5, 5, 9])
add("invert_norm_map", orig_to_norm=[0])
add("invert_norm_map", orig_to_norm=[])
add("invert_norm_map", orig_to_norm=[2, 2, 2, 2, 2])
add("invert_norm_map", orig_to_norm=[0, 2, 4, 6])

# ── norm_end_to_orig ──
add("norm_end_to_orig", orig_to_norm=[0, 1, 2, 3], orig_start=0, norm_end=2)
add("norm_end_to_orig", orig_to_norm=[0, 0, 1, 2], orig_start=0, norm_end=1)
add("norm_end_to_orig", orig_to_norm=[0, 1, 2, 3], orig_start=2, norm_end=9)
add("norm_end_to_orig", orig_to_norm=[0, 1, 2, 3], orig_start=0, norm_end=0)
add("norm_end_to_orig", orig_to_norm=[0, 0, 0], orig_start=1, norm_end=1)
add("norm_end_to_orig", orig_to_norm=[], orig_start=0, norm_end=5)

# ── boundary_note ──
add("boundary_note", strategies=["block_anchor"])
add("boundary_note", strategies=["exact", "context_aware"])
add("boundary_note", strategies=["block_anchor", "block_anchor", "context_aware"])
add("boundary_note", strategies=["exact"])
add("boundary_note", strategies=[])
add("boundary_note", strategies=["block_anchor"], target="X", where=" HERE")
add("boundary_note", strategies=[None, "context_aware", "line_trimmed"])
add("boundary_note", strategies=["context_aware", "block_anchor"])

# ── matched_regions ──
add("matched_regions", content="abcdef", matches=[[0, 2], [4, 6]])
add("matched_regions", content="abcdef", matches=[])
add("matched_regions", content="abcdef", matches=[[1, 5]])
add("matched_regions", content="abcdef", matches=[[4, 6], [0, 2]])
add("matched_regions", content="abc", matches=[[0, 3], [1, 2]])
add("matched_regions", content="", matches=[])

# ── run_strategy_chain ──
add("run_strategy_chain", content="old text", old_string="old", new_string="XYZ",
    replace_all=False, strategies=["exact"],
    io={"exact": [[0, 3]], "maybe_unescape_new_string": "XYZ",
        "apply_replacements": "XYZ text", IMPORT: None, NOTE: None})
add("run_strategy_chain", content="old text", old_string="old", new_string="X",
    replace_all=False, strategies=["exact"], io={"exact": []})
add("run_strategy_chain", content="ab xx", old_string="x", new_string="y",
    replace_all=False, strategies=["exact"],
    io={"exact": [[0, 1], [3, 4]], "format_match_locations": "  L1: ab",
        IMPORT: None, NOTE: None})
add("run_strategy_chain", content="ab xx", old_string="x", new_string="y",
    replace_all=True, strategies=["block_anchor"],
    io={"block_anchor": [[0, 1], [3, 4]], IMPORT: None, NOTE: None})
add("run_strategy_chain", content="  old", old_string="old", new_string="NEW",
    replace_all=False, strategies=["line_trimmed"],
    io={"line_trimmed": [[2, 5]], "detect_escape_drift": "DRIFT-ERR"})
add("run_strategy_chain", content="  old", old_string="old", new_string="NEW",
    replace_all=False, strategies=["line_trimmed"],
    io={"line_trimmed": [[2, 5]], "detect_escape_drift": None,
        "maybe_unescape_new_string": "Q", "apply_replacements": "  Q",
        IMPORT: None, NOTE: None})
add("run_strategy_chain", content="a—b", old_string="a—b", new_string="a-b",
    replace_all=False, strategies=["unicode_normalized"],
    io={"unicode_normalized": [[0, 3]], "detect_escape_drift": None,
        "maybe_unescape_new_string": "a-b",
        "opcodes": [["equal", 0, 3, 0, 3]],
        "apply_replacements": "a-b", IMPORT: None, NOTE: None})
add("run_strategy_chain", content="abc", old_string="a", new_string="b",
    replace_all=False, strategies=["exact"], io={"exact": R("ValueError", "boom")})
add("run_strategy_chain", content="abc", old_string="a", new_string="b",
    replace_all=False, strategies=["exact"],
    io={"exact": [[0, 1]], "maybe_unescape_new_string": "b",
        "apply_replacements": "bbc", IMPORT: R("ValueError", "no metrics")})
add("run_strategy_chain", content="abc", old_string="a", new_string="b",
    replace_all=False, strategies=["exact"],
    io={"exact": [[0, 1]], "maybe_unescape_new_string": "b",
        "apply_replacements": "bbc", IMPORT: R("SystemExit", "bye")})
add("run_strategy_chain", content="abc", old_string="a", new_string="b",
    replace_all=False, strategies=["exact"],
    io={"exact": [[0, 1]], "maybe_unescape_new_string": "b",
        "apply_replacements": R("TypeError", "apply boom")})
add("run_strategy_chain", content="aaaa", old_string="a", new_string="b",
    replace_all=True, strategies=["exact"],
    io={"exact": [[0, 1], [1, 2], [2, 3], [3, 4]],
        "maybe_unescape_new_string": "b", "apply_replacements": "bbbb",
        IMPORT: None, NOTE: None})

# ── strip_line_number_gutter ──
add("strip_line_number_gutter", text="1|hello\n2|world")
add("strip_line_number_gutter", text="1|hello")
add("strip_line_number_gutter", text="1|hello", min_numbered=1)
add("strip_line_number_gutter", text="1|a\n3|b")
add("strip_line_number_gutter", text="hello")
add("strip_line_number_gutter", text="1|a\n2|\n3|b")
add("strip_line_number_gutter", text="10|a\n11|b")
add("strip_line_number_gutter", text="1|a\n2|b", min_numbered=3)
add("strip_line_number_gutter", text="")

# ── gutter_stripped_old ──
add("gutter_stripped_old", content="hello world", old_string="1|hello\n2|world")
add("gutter_stripped_old", content="hello", old_string="1|hello")
add("gutter_stripped_old", content="hello", old_string="hello")
add("gutter_stripped_old", content="1|hello", old_string="1|hello\n2|world")
add("gutter_stripped_old", content="hello world", old_string="1| \n2| ")
add("gutter_stripped_old", content="hello world", old_string="4|hello\n5|world")

# ── gutter_recovery ──
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="bye world", replace_all=False,
    correction_notes=[],
    io={"exact": [[0, 5]], "maybe_unescape_new_string": "bye world",
        "apply_replacements": "bye world", IMPORT: None, NOTE: None})
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="1|bye\n2|world", replace_all=False,
    correction_notes=[],
    io={"exact": [[0, 5]], "maybe_unescape_new_string": "bye\nworld",
        "apply_replacements": "bye\nworld", IMPORT: None, NOTE: None})
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="1|bye\n3|world", replace_all=False,
    correction_notes=[], io={IMPORT: None, NOTE: None})
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="hello world", replace_all=False,
    correction_notes=[])
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="bye world", replace_all=False,
    correction_notes=None,
    io={"exact": [[0, 5]], "maybe_unescape_new_string": "bye world",
        "apply_replacements": "bye world", IMPORT: None, NOTE: None})
add("gutter_recovery", content="hello world", stripped_old="zzz",
    strategies=["exact"], new_string="bye", replace_all=False,
    correction_notes=[], io={"exact": []})
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="bye", replace_all=False,
    correction_notes=[], io={"exact": R("ValueError", "chain boom")})
add("gutter_recovery", content="hello world", stripped_old="hello world",
    strategies=["exact"], new_string="1|bye\n3|world", replace_all=False,
    correction_notes=[], io={IMPORT: R("ValueError", "swallowed")})

# ── note_edit_match ──
add("note_edit_match", strategy="exact", miss=None, io={IMPORT: None, NOTE: None})
add("note_edit_match", strategy=None, miss="ambiguous", io={IMPORT: None, NOTE: None})
add("note_edit_match", strategy="line_trimmed", miss=None,
    io={IMPORT: R("ValueError", "no metrics")})
add("note_edit_match", strategy="exact", miss=None,
    io={IMPORT: R("RuntimeError", "broken")})
add("note_edit_match", strategy="exact", miss=None,
    io={IMPORT: R("SystemExit", "bye")})
add("note_edit_match", strategy="exact", miss=None,
    io={IMPORT: R("KeyboardInterrupt", "ctrl-c")})
add("note_edit_match", strategy="exact", miss=None,
    io={IMPORT: None, NOTE: R("TypeError", "note boom")})
add("note_edit_match", strategy="block_anchor", miss="no_match",
    io={IMPORT: None, NOTE: None})
add("note_edit_match", strategy="exact", miss=None, io={})  # fail-closed import

# ── detect_newline_literal_drift ──
add("detect_newline_literal_drift", content="a\nb", matches=[[0, 3]],
    old_string="a\\nb", new_string="a\\nc")
add("detect_newline_literal_drift", content="a\nb", matches=[[0, 3]],
    old_string="a\\nb", new_string="x")
add("detect_newline_literal_drift", content="abc", matches=[[0, 3]],
    old_string="a\\nb", new_string="a\\nc")
add("detect_newline_literal_drift", content="a\nb\nc\nd", matches=[[0, 3], [4, 7]],
    old_string="a\\nb\\nc\\nd", new_string="x\\ny")
add("detect_newline_literal_drift", content="ab", matches=[[0, 2]],
    old_string="x", new_string="p\\nq")
add("detect_newline_literal_drift", content="a\nb", matches=[[0, 3]],
    old_string="a\\nb\\nb", new_string="a\\nc")

# ── backslash_runs ──
add("backslash_runs", s="a\\\\b\\\\\\c")
add("backslash_runs", s="no backslash")
add("backslash_runs", s="\\")
add("backslash_runs", s="\\\\")
add("backslash_runs", s="a\\b\\c")
add("backslash_runs", s="")

# ── detect_backslash_doubling ──
add("detect_backslash_doubling", matched_regions="\\\\", old_string="\\\\\\\\",
    new_string="x")
add("detect_backslash_doubling", matched_regions="\\\\", old_string="\\\\\\\\",
    new_string="\\\\")
add("detect_backslash_doubling", matched_regions="\\", old_string="\\\\",
    new_string="x")
add("detect_backslash_doubling", matched_regions="\\a\\", old_string="\\\\a\\\\",
    new_string="x")
add("detect_backslash_doubling", matched_regions="\\\\", old_string="\\\\\\",
    new_string="x")
add("detect_backslash_doubling", matched_regions="plain", old_string="plain",
    new_string="x")

# ── match_starts_mid_line ──
add("match_starts_mid_line", content="abc def\nghi", start=4, end=7)
add("match_starts_mid_line", content="  def\nghi", start=2, end=5)
add("match_starts_mid_line", content="\n\ndef\nx", start=2, end=5)
add("match_starts_mid_line", content="\ndef\nx", start=0, end=4)
add("match_starts_mid_line", content="aaaa", start=1, end=3)
add("match_starts_mid_line", content="x y", start=2, end=3)

# ── unicode_edit_plan ──
add("unicode_edit_plan", old_string="hi", new_string="bye",
    io={"opcodes": [["equal", 0, 2, 0, 2], ["insert", 2, 2, 2, 3]]})
add("unicode_edit_plan", old_string="\u201cx\u201d", new_string="\"x\"",
    io={"opcodes": [["replace", 0, 3, 0, 3]]})
add("unicode_edit_plan", old_string="abc", new_string="abc",
    io={"opcodes": [["equal", 0, 3, 0, 3]]})
add("unicode_edit_plan", old_string="a", new_string="b",
    io={"opcodes": R("ValueError", "opcode boom")})
add("unicode_edit_plan", old_string="a", new_string="b", io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"
    assert len(counts) == 17, "need all 17 functions"


if __name__ == "__main__":
    main()
