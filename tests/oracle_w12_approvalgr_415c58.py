#!/usr/bin/env python3
"""Differential oracle for tools/approval_grep_spans.py (gate w12_approvalgr_415c58).

The LIVE _quoted_grep_pattern_spans and _grep_safe_detection_variant are loaded
from the quarry and driven with scripted seams that mirror the C lexer/option
parser: _iter_top_level_shell_segments, _iter_shell_command_word_spans,
_deobfuscate_shell_word_for_detection, _shell_tokens_with_spans, _splice and the
two grep option-arg tables.  The function under test is the real quarry code;
only its collaborators are scripted.
"""
import importlib.util
import json
import os
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _lex(s, start):
    tokens, i, n = [], start, len(s)
    while i < n:
        while i < n and s[i].isspace():
            i += 1
        if i >= n:
            break
        st = i
        buf = []
        quote = None
        while i < n:
            ch = s[i]
            if quote is not None:
                if ch == quote:
                    quote = None
                else:
                    buf.append(ch)
            elif ch in ("'", '"'):
                quote = ch
            elif ch.isspace():
                break
            else:
                buf.append(ch)
            i += 1
        raw = s[st:i]
        inert = len(raw) >= 2 and raw[0] == "'" and raw[-1] == "'"
        tokens.append(("".join(buf), st, i, inert))
    return tokens


def iter_words(segment):
    return [(s, e, segment[s:e]) for _, s, e, _ in _lex(segment, 0)]


def deobf(word):
    inner = word[1:-1] if len(word) >= 2 and word[0] == word[-1] and word[0] in ("'", '"') else word
    return inner.replace("\\", "")


def tokens_fn(segment, start):
    return _lex(segment, start)


def segments(command):
    yield command


def splice(command, edits):
    parts, prev = [], 0
    for start, end, text in edits:
        parts.extend((command[prev:start], text))
        prev = end
    parts.append(command[prev:])
    return "".join(parts)


tools = types.ModuleType("tools")
tools.__path__ = [QUARRY + "/tools"]
sys.modules["tools"] = tools
ad = types.ModuleType("tools.approval_detection")
ad._GREP_OPTIONS_WITH_ARG = {
    "--after-context", "--before-context", "--binary-files", "--context", "--directories",
    "--devices", "--exclude", "--exclude-dir", "--exclude-from", "--include", "--label",
    "--max-count", "--regexp", "--file",
}
ad._GREP_SHORT_OPTIONS_WITH_ARG = {"A", "B", "C", "D", "d", "e", "f", "m"}
ad._deobfuscate_shell_word_for_detection = deobf
ad._iter_shell_command_word_spans = iter_words
ad._iter_top_level_shell_segments = segments
ad._splice = splice
sys.modules["tools.approval_detection"] = ad
st = types.ModuleType("tools.approval_shell_tokens")
st._shell_tokens_with_spans = tokens_fn
sys.modules["tools.approval_shell_tokens"] = st

SRC = QUARRY + "/tools/approval_grep_spans.py"
spec = importlib.util.spec_from_file_location("approval_grep_spans_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["approval_grep_spans_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    cmd = r.get("command", "")
    if func == "quoted_grep_pattern_spans":
        spans, malformed = live._quoted_grep_pattern_spans(cmd)
        return {"spans": [[s, e] for s, e in spans], "malformed": bool(malformed)}
    if func == "grep_safe_detection_variant":
        safe, malformed = live._grep_safe_detection_variant(cmd)
        return {"command": safe, "malformed": bool(malformed)}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_approvalgr_415c58: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
