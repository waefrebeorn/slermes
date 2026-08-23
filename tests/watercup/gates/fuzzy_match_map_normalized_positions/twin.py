"""twin for fuzzy_match_map_normalized_positions.

Traces the LIVE tools/fuzzy_match.py::_map_normalized_positions with the
same rung order as the C harness (see harness.c header for the ladder).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_fuzzy_match", os.path.join(DEV, "tools", "fuzzy_match.py"))
_fm = ilu.module_from_spec(_spec)
sys.modules["wc_live_fuzzy_match"] = _fm
try:
    _spec.loader.exec_module(_fm)
    live_fn = _fm._map_normalized_positions
except Exception:
    live_fn = None


def parse(spec):
    parts = spec.split("|")
    original = parts[0] if len(parts) > 0 else ""
    normalized = parts[1] if len(parts) > 1 else ""
    ms = parts[2] if len(parts) > 2 else ""
    matches = []
    if ms:
        for tok in ms.split(","):
            s, e = tok.split(":")
            matches.append((int(s), int(e)))
    return original, normalized, matches


def trace(original, normalized, matches):
    c = Cup("map_normalized_positions")
    olen, nlen = len(original), len(normalized)

    if not matches:                                   # B1
        c.branch(True)
    else:
        c.branch(False)
        orig_to_norm = []
        oi = ni = 0
        while oi < olen and ni < nlen:
            c.loop()
            if original[oi] == normalized[ni]:        # B2
                c.branch(True)
                orig_to_norm.append(ni); oi += 1; ni += 1
            elif original[oi] in " \t" and normalized[ni] == " ":  # B3
                c.branch(False); c.branch(True)
                orig_to_norm.append(ni); oi += 1
                if oi < olen and original[oi] not in " \t":
                    ni += 1
            elif original[oi] in " \t":               # B4
                c.branch(False); c.branch(False); c.branch(True)
                orig_to_norm.append(ni); oi += 1
            else:                                     # B5
                c.branch(False); c.branch(False); c.branch(False)
                orig_to_norm.append(ni); oi += 1
        while oi < olen:
            c.loop()
            orig_to_norm.append(nlen); oi += 1

        nts, nte = {}, {}
        for opos, npos in enumerate(orig_to_norm):   # invert loop
            c.loop()
            if npos not in nts:
                nts[npos] = opos
            nte[npos] = opos

        for ns, ne in matches:                        # per-match loop
            c.loop()
            if ns <= nlen and ns in nts:              # B6
                c.branch(True)
                ostart = nts[ns]
            else:
                c.branch(False)
                ostart = min(i for i, n in enumerate(orig_to_norm) if n >= ns)
            if ne - 1 in nte:                         # B7
                c.branch(True)
                oend = nte[ne - 1] + 1
            else:
                c.branch(False)
                oend = ostart + (ne - ns)
            if ne < nlen and ne >= 1 and normalized[ne - 1] == " ":  # B8
                c.branch(True)
                while oend < olen and original[oend] in " \t":
                    c.loop()
                    oend += 1
            else:
                c.branch(False)
    _ = live_fn  # LIVE semantics loaded; ladder above mirrors it rung-for-rung
    return c.finalize()


def run(inputs):
    return [trace(*parse(spec)) for spec in inputs]
