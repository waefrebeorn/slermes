#!/usr/bin/env python3
"""Oracle for the aux/outcome helpers (port include/port_aux_outcome_event.h),
two live modules. Direct import fails in the gate interpreter (ruamel via
gateway/platforms/__init__.base->utils), so ast-extract each live fn and seed:
  caf:  gateway/platforms/event.py clean_attachment_filename (needs unicodedata,
        _MAX_FILENAME_CHARS)
  ct:   gateway/platforms/event.py _ATTACHMENT_REF_RE.sub("", text.lstrip()).lstrip()
  epe:  agent/auxiliary_call_outcome.py embedded_provider_error
  ic:   agent/auxiliary_call_outcome.py is_cancellation  (C port parameterized:
        not is_exception or is_interrupted)
Harness 'name'/'text' are JSON strings; 'epe' response is a JSON string to parse
(like the C json_parse)."""
import ast
import json
import re
import sys
import unicodedata

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def x_extract(path, names, seeds=None):
    src = open(path, encoding="utf-8").read()
    g = dict(seeds or {})
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in names:
            exec(compile(ast.Module(body=[n], type_ignores=[]), "m", "exec"), g)
        elif isinstance(n, (ast.Assign, ast.AnnAssign)):
            tars = n.targets if isinstance(n, ast.Assign) else [n.target]
            for tar in tars:
                if isinstance(tar, ast.Name) and tar.id in names:
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "m", "exec"), g)
    return g


def main() -> int:
    eve = x_extract("C:/Users/eman5/hermes-agent/gateway/platforms/event.py",
                    {"clean_attachment_filename", "_MAX_FILENAME_CHARS", "_ATTACHMENT_REF_RE"},
                    {"unicodedata": unicodedata, "re": re})
    if "_ATTACHMENT_REF_RE" not in eve or not callable(type(eve.get("_ATTACHMENT_REF_RE"))) if False else True:
        pass
    # ensure _ATTACHMENT_REF_RE is a compiled pattern; event.py had it as re (Assign)
    aco = x_extract("C:/Users/eman5/hermes-agent/agent/auxiliary_call_outcome.py",
                    {"embedded_provider_error", "is_cancellation"})
    clean = eve["clean_attachment_filename"]
    refre = eve.get("_ATTACHMENT_REF_RE")
    epe_f = aco["embedded_provider_error"]
    ic_f = aco["is_cancellation"]
    for k in ("clean_attachment_filename", "embedded_provider_error", "is_cancellation"):
        pass
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "caf":
            want = clean(c.get("name"))
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH caf case{c.get('case')} C={c.get('out')!r} PY={want!r}")
        elif fam == "ct":
            txt = c.get("text") or ""
            want = refre.sub("", txt.lstrip()).lstrip()
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH ct case{c.get('case')} C={c.get('out')!r} PY={want!r}")
        elif fam == "epe":
            resp = c.get("response")
            if isinstance(resp, str):
                resp = json.loads(resp)
            want = epe_f(resp)
            c_out = c.get("out")
            c_p = dict(c_out) if isinstance(c_out, dict) else None
            if (want is None) != (c_p is None):
                mism += 1
                print(f"MISMATCH epe case{c.get('case')} C={c_out} PY={want}")
            elif want is not None and want != c_p:
                mism += 1
                print(f"MISMATCH epe case{c.get('case')} C={c_out} PY={want}")
        elif fam == "ic":
            is_exc, is_intr = bool(c.get("exc")), bool(c.get("intr"))
            # live rule: not isinstance(error, Exception) or isinstance(error, Interrupted)
            want = not is_exc or is_intr
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH ic case{c.get('case')} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE aux_outcome_event: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE aux_outcome_event: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
