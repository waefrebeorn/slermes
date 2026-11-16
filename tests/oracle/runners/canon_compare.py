#!/usr/bin/env python3
"""Canonicalise and compare oracle output -- in ONE interpreter.

The runner used to spawn Python three times per case: twice to normalise the
two outputs and once to compare them. At ~170 ms per interpreter start that is
~93 s for a 182-case port, which dwarfed the C side it was measuring. The
oracle already runs in one process, so normalise and compare there.

normalise() reproduces the runner's old three-stage pipeline exactly:
canon (sort keys, optional integral-float coercion) -> strip_patterns (regex
removal) -> strip_fields (drop top-level JSON keys).
"""
import json
import re


def canon(o, float_int_equal):
    if float_int_equal and isinstance(o, float) and o == int(o):
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v, float_int_equal) for k, v in o.items()}
    if isinstance(o, list):
        return [canon(v, float_int_equal) for v in o]
    return o


def normalise(text, cfg):
    float_int_equal = bool(cfg.get("float_int_equal"))
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.dumps(canon(json.loads(line), float_int_equal),
                                  sort_keys=True))
        except Exception:
            out.append(line)
    text = "\n".join(out) + "\n"

    for p in cfg.get("strip_patterns") or []:
        p = p.strip()
        if p:
            text = re.sub(p, "", text)

    fields = cfg.get("strip_fields") or []
    if fields:
        drop = set(fields)
        out = []
        for line in text.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
                if isinstance(obj, dict):
                    obj = {k: v for k, v in obj.items() if k not in drop}
                    line = json.dumps(obj)
            except Exception:
                pass
            out.append(line)
        text = "\n".join(out) + "\n"
    return text


SELF_SUMMARY = re.compile(
    r"RESULT: .*0 mismatch|oracle: 0 mismatch|0 mismatches|0 failed")


def verdict(c_text, py_text, cfg, rc=0):
    """Return (verdict, c_norm, py_norm).

    verdict is MATCH or MISMATCH. Some oracles are self-summarising: they print
    'MISMATCH ...' on failure and a '0 mismatch' summary on success, so their own
    verdict is honoured when the raw JSON diff cannot be parsed -- but only when
    the oracle actually exited 0. A crashed oracle that happens to print a
    success summary must not pass.
    """
    c_norm = normalise(c_text, cfg)
    py_norm = normalise(py_text, cfg)
    if c_norm == py_norm:
        return "MATCH", c_norm, py_norm
    if "MISMATCH" in py_text:
        return "MISMATCH", c_norm, py_norm
    if rc == 0 and SELF_SUMMARY.search(py_text):
        return "MATCH", c_norm, py_norm
    return "MISMATCH", c_norm, py_norm
