"""twin for context_compressor_is_blank_user_turn.

Imports LIVE agent/context_compressor.py and calls
ContextCompressor._is_blank_user_turn while tracing the SAME ladder as the C
port cc_is_blank_user_turn. Input encoding: meta\x1frole\x1fcontent-spec.
Whitespace-only text inputs are excluded: C checks strlen==0 where LIVE strips
(a real, documented divergence).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cc", os.path.join(DEV, "agent", "context_compressor.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cc"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def build_message(meta, role, spec):
    msg = {"role": role}
    if meta:
        msg["_compressed_summary"] = True  # COMPRESSED_SUMMARY_METADATA_KEY
    if spec == "N":
        return msg
    if spec.startswith("S:"):
        msg["content"] = spec[2:]
        return msg
    if spec.startswith("A"):
        body = spec[2:] if len(spec) > 1 and spec[1] == ":" else spec[1:]
        parts = []
        if body:
            for tok in body.split(","):
                if tok.startswith("S:"):
                    parts.append(tok[2:])
                elif tok.startswith("T:"):
                    parts.append({"type": "text", "text": tok[2:]})
                elif tok == "I":
                    parts.append({"type": "image"})
                elif tok.startswith("U:"):
                    parts.append({"type": tok[2:], "data": "?"})
        msg["content"] = parts
        return msg
    return msg


def run(inputs):
    cups = []
    for raw in inputs:
        meta_s, role, spec = raw.split("\x1f")
        msg = build_message(meta_s == "1", role, spec)
        c = Cup("is_blank_user_turn")
        res = False
        if role != "user":
            c.branch(True)
        else:
            c.branch(False)
            if meta_s == "1":
                c.branch(True)
            else:
                c.branch(False)
                if spec == "N":
                    c.branch(True)
                    res = True
                else:
                    c.branch(False)
                    if spec.startswith("S:"):
                        c.branch(True)
                        if len(spec[2:]) == 0:
                            res = True
                    else:
                        c.branch(False)
                        is_arr = spec.startswith("A")
                        if not is_arr:
                            c.branch(True)
                        else:
                            c.branch(False)
                            body = spec[2:]
                            if not body:
                                c.branch(True)
                                res = True
                            else:
                                c.branch(False)
                                res = True
                                for tok in body.split(","):
                                    if tok.startswith("S:"):
                                        c.branch(True)
                                        if len(tok[2:]) > 0:
                                            res = False
                                            break
                                    elif tok.startswith("T:"):
                                        c.branch(False)
                                        c.branch(len(tok) == 2)   # mirrors C strcmp(tok,"T:")
                                        if len(tok[2:]) > 0:
                                            res = False
                                            break
                                    else:
                                        c.branch(False)
                                        c.branch(False)
                                        res = False   # image/audio/unknown block
                                        break
        live = bool(mod.ContextCompressor._is_blank_user_turn(msg))
        assert res == live, f"LIVE/ladder diverge on {raw!r}: {live} vs {res}"
        c.valstr("1" if res else "0")
        cups.append(c.finalize())
    return cups
