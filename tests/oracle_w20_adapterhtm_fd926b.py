#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter_html.py (w20_adapterhtm_fd926b).

The live module is imported from the quarry and each harness line is replayed
through _latex_to_tokens, _tokens_to_mx_maths, _tokens_to_tex_source,
_sanitize_matrix_html, _pre_sanitize_matrix_markdown and _safe_url. The TeX
token store crosses the wire as [{tag, tex}] objects and is rebuilt as the
module's own (tag, tex) tuples.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_adapterhtm_fd926b_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "platforms", "matrix", "adapter_html.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def store_of(r):
    raw = r.get("store")
    if not isinstance(raw, list):
        return []
    return [(str(e.get("tag", "span")), str(e.get("tex", "")))
            for e in raw if isinstance(e, dict)]


def expected(r):
    f = r.get("func")
    if f == "latex":
        text, store = live._latex_to_tokens(r.get("text") or "")
        return {"text": text, "store": [{"tag": tag, "tex": tex} for tag, tex in store]}
    if f == "mx_maths":
        return live._tokens_to_mx_maths(r.get("html") or "", store_of(r))
    if f == "tex_source":
        return live._tokens_to_tex_source(r.get("text") or "", store_of(r))
    if f == "sanitize":
        store = store_of(r)
        return live._sanitize_matrix_html(r.get("html") or "", store if store else None)
    if f == "pre_sanitize":
        return live._pre_sanitize_matrix_markdown(r.get("text") or "")
    if f == "safe_url":
        return live._MatrixHtmlSanitizer._safe_url(r.get("value") or "")
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_adapterhtm_fd926b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
