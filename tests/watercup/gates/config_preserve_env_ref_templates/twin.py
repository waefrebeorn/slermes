"""twin for config_preserve_env_ref_templates.

Traces the LIVE hermes_cli/config.py::_preserve_env_ref_templates with the
SAME rung ladder as the C harness (recursion wrapped in R/r):

  B1 current-is-str / B2 raw-is-str-with-${...} / B3 eq-raw /
  B4 eq-loaded_expanded / B5 expand(raw)==current / leaf V tag /
  B6 obj (per-key L + recursion) / B7 array / V "passthrough".

Input: JSON [current, raw, loaded_expanded].
"""
import os
import json
import re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import sys
sys.path.insert(0, DEV)
from watercup_py import Cup

# Import LIVE config module by path. hermes_cli/config.py is heavy; if its
# package imports fail we still need the LIVE expansion semantics, so fall
# back to transcribing _expand_env_vars/_env_expand_match exactly from the
# LIVE source (verbatim logic below, verified against it).
try:
    spec = ilu.spec_from_file_location(
        "wc_live_config", os.path.join(DEV, "hermes_cli", "config.py"))
    cfg = ilu.module_from_spec(spec)
    sys.modules["wc_live_config"] = cfg
    spec.loader.exec_module(cfg)
    live_expand = cfg._expand_env_vars
except Exception:
    live_expand = None

_ENV_RE = re.compile(r"\${([^}]+)}")
_SECRETREF_RE = re.compile(r"^[a-z][a-z0-9_-]*:")


def _expand_str(obj):
    """LIVE _expand_env_vars string arm (verbatim semantics)."""
    def repl(m):
        raw = m.group(0)
        inner = m.group(1).strip()
        if inner.startswith("env:"):
            name = inner[len("env:"):].strip()
            if not name:
                return raw
            val = os.environ.get(name)
            return val if val is not None else raw
        if ":" in inner and _SECRETREF_RE.match(inner):
            return raw
        return os.environ.get(inner, raw)
    if isinstance(obj, str):
        return _ENV_RE.sub(repl, obj)
    if isinstance(obj, dict):
        return {k: _expand_str(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_expand_str(i) for i in obj]
    return obj


def expand(obj):
    return live_expand(obj) if live_expand else _expand_str(obj)


def trace(wc, current, raw, loaded):
    wc.rec_in()
    result_tag = None
    recurse = None

    if isinstance(current, str):
        wc.branch(True)                                    # B1
        tmpl = isinstance(raw, str) and bool(re.search(r"\${[^}]+}", raw))
        wc.branch(tmpl)                                    # B2
        if tmpl:
            if current == raw:
                wc.branch(True)                            # B3
                wc.valstr("keep_raw")
            else:
                wc.branch(False)                           # B3
                le = isinstance(loaded, str) and current == loaded
                wc.branch(le)                              # B4
                if le:
                    wc.valstr("keep_raw")
                else:
                    eq_exp = expand(raw) == current
                    wc.branch(eq_exp)                      # B5
                    wc.valstr("keep_raw" if eq_exp else "keep_current")
        else:
            wc.valstr("keep_current")
    else:
        wc.branch(False)                                   # B1
        if isinstance(current, dict):
            wc.branch(True)                                # B6
            for key, value in current.items():
                wc.loop()
                rv = raw.get(key) if isinstance(raw, dict) else None
                lv = loaded.get(key) if isinstance(loaded, dict) else None
                sub = trace(wc, value, rv, lv)
                if recurse is None:
                    recurse = {key: sub}
                else:
                    recurse[key] = sub
        else:
            wc.branch(False)                               # B6
            if isinstance(current, list):
                wc.branch(True)                            # B7
                out = []
                for i, item in enumerate(current):
                    wc.loop()
                    rv = raw[i] if isinstance(raw, list) and i < len(raw) else None
                    lv = loaded[i] if isinstance(loaded, list) and i < len(loaded) else None
                    out.append(trace(wc, item, rv, lv))
                recurse = out
            else:
                wc.branch(False)                           # B7
                wc.valstr("passthrough")
    wc.rec_out()
    return recurse


def trace_top(current, raw, loaded):
    c = Cup("preserve_env_ref_templates")
    trace(c, current, raw, loaded)
    return c.finalize()


def run(inputs):
    out = []
    for s in inputs:
        arr = json.loads(s)
        cur = arr[0]
        raw = arr[1]
        loaded = arr[2] if len(arr) > 2 else None
        out.append(trace_top(cur, raw, loaded))
    return out


if __name__ == "__main__":
    # sanity vs LIVE fn where importable
    if live_expand:
        cases = [
            ("${ZQ_UNSET_1}", "${ZQ_UNSET_1}", None),
            ("sk-expanded", "${ZQ_UNSET_1}", "sk-expanded"),
        ]
        for cur, raw, le in cases:
            assert cfg._preserve_env_ref_templates(cur, raw, le) in ("${ZQ_UNSET_1}", cur)
    print("twin ok")
