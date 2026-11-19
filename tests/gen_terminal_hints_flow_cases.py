#!/usr/bin/env python3
"""Generate tests/fixtures/terminal_hints_flow.jsonl for the terminal_hints_flow
oracle. Each case is {"func": <name>, "in": {...}} with scripted seams in in.io
(plain value reused; {"__seq__": [...]} per-call; {"__raise__": kind, "msg": text}
raises; scripted null -> None; ABSENT key -> fail-closed RuntimeError). >=5 cases
per ported function, exercising match/no-match, format vs callable message,
error paths, fail-closed seams and BaseException passthrough. ASCII scope;
integral floats avoided; output is str-or-null (non-string scalars out of domain).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "terminal_hints_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def seq(*vals):
    return {"__seq__": list(vals)}


def M(*groups):
    return {"groups": list(groups)}


# ── regex_hint (factory) ──
add("regex_hint", pattern="x", message="m {0}", flags=0, io={"re.compile": {"pattern": "x", "flags": 32}})
add("regex_hint", pattern="^C", message="conflict", flags=8, io={"re.compile": {"pattern": "^C", "flags": 40}})
add("regex_hint", pattern="p", message={"__callable__": "missing_command_hint"}, flags=0,
    io={"re.compile": {"pattern": "p", "flags": 32}})
add("regex_hint", pattern="p", message={"__callable__": "custom"}, flags=0,
    io={"re.compile": {"pattern": "p", "flags": 32}})
add("regex_hint", pattern="p", message="m", flags=0, io={"re.compile": None})
add("regex_hint", pattern="p", message="m", flags=0, io={"re.compile": R("ValueError", "bad pattern")})
add("regex_hint", pattern="p", message="m", flags=0, io={"re.compile": R("SystemExit", "bye")})
add("regex_hint", pattern="p", message="m", flags=0, io={})

# ── hint (generic closure) ──
add("hint", message="hit {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("X")})
add("hint", message="hit {0} {1}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("A", "B")})
add("hint", message="no fields", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("X")})
add("hint", message="literal {{brace}} {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("X")})
add("hint", message="hit {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": None})
add("hint", message={"__callable__": "missing_command_hint"}, command="c", output="abc",
    rx={"pattern": "x", "flags": 0}, io={"rx.search": M("python")})
add("hint", message={"__callable__": "custom"}, command="c", output="abc",
    rx={"pattern": "x", "flags": 0}, io={"rx.search": M("g"), "message_call": "custom-result"})
add("hint", message={"__callable__": "custom"}, command="c", output="abc",
    rx={"pattern": "x", "flags": 0}, io={"rx.search": M("g"), "message_call": R("RuntimeError", "cb")})
add("hint", message="hit {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": R("ValueError", "search")})
add("hint", message="hit {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": R("SystemExit", "stop")})
add("hint", message="hit {0}", command="c", output=None, rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("X")})
add("hint", message="hit {0}", command="c", output="abc", rx={"pattern": "x", "flags": 0}, io={})
# non-ASCII group values exercise the verbatim UTF-8 byte-copy in th_format_groups
add("hint", message="field {0} — done", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("café")})
add("hint", message="{0}/{1}", command="c", output="abc", rx={"pattern": "x", "flags": 0},
    io={"rx.search": M("naïve", "☕")})

# ── production _OUTPUT_HINTS instances ──
PROD_GROUPS = {
    "hint_gh_unknown_json_field": ["title"],
    "hint_merge_conflict": [],
    "hint_command_not_found": ["python"],
    "hint_module_not_found": ["numpy"],
    "hint_already_exists": ["main"],
    "hint_gh_rate_limit": [],
    "hint_permission_denied": [],
}
for func, groups in PROD_GROUPS.items():
    add(func, command="cmd", output="some output", io={"rx.search": M(*groups)})
    add(func, command="cmd", output="some output", io={"rx.search": None})
    add(func, command="cmd", output="some output", io={"rx.search": R("RuntimeError", "boom")})
    add(func, command="cmd", output="some output", io={"rx.search": R("KeyboardInterrupt", "ki")})
    add(func, command="cmd", output="some output", io={})
# command_not_found exercises the dict + fallback via different captured tokens
add("hint_command_not_found", command="cmd", output="bash: python: command not found",
    io={"rx.search": M("pip")})
add("hint_command_not_found", command="cmd", output="bash: foobar: command not found",
    io={"rx.search": M("foobar")})
add("hint_command_not_found", command="cmd", output="x", io={"rx.search": M("")})
# a second match variant for a couple of instances
add("hint_gh_unknown_json_field", command="cmd", output="Unknown JSON field: \"number\"",
    io={"rx.search": M("number")})
add("hint_already_exists", command="cmd", output="fatal: 'dev' already exists",
    io={"rx.search": M("dev")})
add("hint_module_not_found", command="cmd", output="ImportError: No module named 'os.path'",
    io={"rx.search": M("os.path")})

# ── missing_command_hint (pure leaf) ──
add("missing_command_hint", missing="python", io={})
add("missing_command_hint", missing="pip", io={})
add("missing_command_hint", missing="foobar", io={})
add("missing_command_hint", missing="", io={})
add("missing_command_hint", missing="Python", io={})
add("missing_command_hint", missing="node.js", io={})

# ── wrapper_cd_hint (last-line fullmatch leaf) ──
add("wrapper_cd_hint", output="bash: line 1: cd: /no/dir: No such file or directory",
    io={"rx.fullmatch": M("/no/dir", "No such file or directory")})
add("wrapper_cd_hint", output=None, io={"rx.fullmatch": None})
add("wrapper_cd_hint", output="some\nlast line", io={"rx.fullmatch": M("/d", "err")})
add("wrapper_cd_hint", output="trailing   \n\n", io={"rx.fullmatch": None})
add("wrapper_cd_hint", output="C:\\x: line 2: cd: /d: Permission denied",
    io={"rx.fullmatch": M("/d", "Permission denied")})
add("wrapper_cd_hint", output="bash: line 1: cd: /d: err", io={"rx.fullmatch": R("ValueError", "fm")})
add("wrapper_cd_hint", output="bash: line 1: cd: /d: err", io={"rx.fullmatch": R("GeneratorExit", "ge")})
add("wrapper_cd_hint", output="bash: line 1: cd: /d: err", io={})

# ── failure_shapes_search (direct matcher) ──
add("failure_shapes_search", window="error[E0308]: mismatched types", io={"failure_shapes.search": M()})
add("failure_shapes_search", window="all good", io={"failure_shapes.search": None})
add("failure_shapes_search", window="Traceback (most recent call last)", io={"failure_shapes.search": M()})
add("failure_shapes_search", window="x", io={"failure_shapes.search": R("RuntimeError", "fs")})
add("failure_shapes_search", window="x", io={"failure_shapes.search": R("SystemExit", "se")})
add("failure_shapes_search", window="x", io={})

# ── masking_pipe_search (direct matcher) ──
add("masking_pipe_search", cmd="cargo build | tail -20", io={"masking_pipe.search": M()})
add("masking_pipe_search", cmd="cargo build", io={"masking_pipe.search": None})
add("masking_pipe_search", cmd="grep x | head", io={"masking_pipe.search": M()})
add("masking_pipe_search", cmd="x", io={"masking_pipe.search": R("ValueError", "mp")})
add("masking_pipe_search", cmd="x", io={"masking_pipe.search": R("KeyboardInterrupt", "ki")})
add("masking_pipe_search", cmd="x", io={})

# ── masking_fallback_search (direct matcher) ──
add("masking_fallback_search", cmd="cargo build || echo FAILED", io={"masking_fallback.search": M()})
add("masking_fallback_search", cmd="cargo build", io={"masking_fallback.search": None})
add("masking_fallback_search", cmd="cmd || true", io={"masking_fallback.search": M()})
add("masking_fallback_search", cmd="x", io={"masking_fallback.search": R("RuntimeError", "mf")})
add("masking_fallback_search", cmd="x", io={"masking_fallback.search": R("GeneratorExit", "ge")})
add("masking_fallback_search", cmd="x", io={})

# ── wrapper_cd_fullmatch (direct matcher) ──
add("wrapper_cd_fullmatch", last_line="bash: line 1: cd: /d: err", io={"rx.fullmatch": M("/d", "err")})
add("wrapper_cd_fullmatch", last_line="not a cd error", io={"rx.fullmatch": None})
add("wrapper_cd_fullmatch", last_line="Zeile 2: cd: /d: Datei nicht gefunden",
    io={"rx.fullmatch": M("/d", "Datei nicht gefunden")})
add("wrapper_cd_fullmatch", last_line="x", io={"rx.fullmatch": R("ValueError", "wf")})
add("wrapper_cd_fullmatch", last_line="x", io={"rx.fullmatch": R("SystemExit", "se")})
add("wrapper_cd_fullmatch", last_line="x", io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count audit
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        flag = "" if counts[k] >= 5 else "  <-- BELOW 5"
        print("  %-32s %d%s" % (k, counts[k], flag))
    assert all(v >= 5 for v in counts.values()), "some func has <5 cases"


if __name__ == "__main__":
    main()