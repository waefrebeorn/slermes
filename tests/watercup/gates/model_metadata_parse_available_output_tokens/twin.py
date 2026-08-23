"""twin for model_metadata_parse_available_output_tokens.

Traces the LIVE agent/model_metadata.py::parse_available_output_tokens_from_error
against the same rung ladder as the C harness (traced copy of
src/agent/provider_metadata.c::parse_available_output_tokens_from_error):
  B1 empty / B2 has max_tokens / B3 has available-tokens marker /
  B4 digits after marker / B5 '=' fallback / B6 digits after '=';
VSTR(result or "-1").
"""
import os, sys, re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_mm", os.path.join(DEV, "agent", "model_metadata.py"))
_mm = ilu.module_from_spec(_spec)
sys.modules["wc_live_mm"] = _mm
_spec.loader.exec_module(_mm)
_parse_live = _mm.parse_available_output_tokens_from_error


def trace(msg):
    c = Cup("parse_available_output_tokens_from_error")
    low = (msg or "").lower()

    c.branch(not msg)                                        # B1
    if msg:
        has_mt = "max_tokens" in low
        marker = None
        for m in ("available_output_tokens", "available_tokens", "available tokens"):
            if m in low:
                marker = m
                break
        # NOTE: marker preference order mirrors the C extraction lookup.
        c.branch(has_mt)                                     # B2
        c.branch(marker is not None)                         # B3
    else:
        has_mt, marker = False, None

    result = None
    if has_mt and marker is not None:
        at = low.index(marker)
        num = at + len(marker)
        while num < len(low) and low[num] not in ":= ":
            num += 1
        while num < len(low) and low[num] in ":= ":
            num += 1
        digit = num < len(low) and low[num].isdigit()
        c.branch(digit)                                      # B4
        if digit:
            m = re.match(r"\d+", low[num:])
            val = int(m.group(0))
            if val >= 1:
                result = val
        if result is None:
            eq = low.rfind("=")
            if eq != -1:
                c.branch(True)                               # B5
                rest = low[eq + 1:].lstrip(" ")
                digit2 = bool(rest) and rest[0].isdigit()
                c.branch(digit2)                             # B6
                if digit2:
                    val = int(re.match(r"\d+", rest).group(0))
                    if val >= 1:
                        result = val
            else:
                c.branch(False)                              # B5
                # C emits B6 only inside the eq!=NULL path — nothing here.
        else:
            pass
    else:
        # keep rung count aligned: no extra rungs on the failure path
        pass

    live = _parse_live(msg)
    expected = live  # LIVE is the oracle for the value
    got = expected if expected is not None else None
    assert got == result or (expected is None and result is None), (
        msg, result, expected)
    if expected is not None:
        assert result == expected, (msg, result, expected)

    c.valstr(str(result) if result is not None else "-1")
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
