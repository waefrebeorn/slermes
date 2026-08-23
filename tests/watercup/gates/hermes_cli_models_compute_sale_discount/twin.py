"""twin for hermes_cli_models_compute_sale_discount.

Traces the LIVE hermes_cli/models.py::compute_sale_discount with the SAME
rung order as the C harness:
  B1 free/$0 / B2 dict / B3 was-empty / B4,B5 prompt sale /
  B6,B7 completion sale / VSTR(outcome).
Input token: "cp|cc|wp|wc" ("-" on both was-fields => original is not a dict).
The outcome value comes from calling the LIVE function; branch rungs are
evaluated in the mirrored order from the same parsed fields.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_models", os.path.join(DEV, "hermes_cli", "models.py"))
_md = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_models"] = _md
try:
    _spec.loader.exec_module(_md)
    _live_fn = _md.compute_sale_discount
except Exception:
    _live_fn = None


def _parse(tok):
    parts = tok.split("|")
    while len(parts) < 4:
        parts.append("")
    cp, cc, wp, wc = parts[:4]
    have_dict = not (wp == "-" and wc == "-")
    if have_dict:
        original = {"prompt": wp if wp != "" else None,
                    "completion": wc if wc != "" else None}
        # strip trailing empties to mimic dicts without those keys
        original = {k: v for k, v in original.items() if v is not None}
    else:
        original = None
    return cp, cc, original


def _nonneg(raw):
    try:
        n = float(raw)
    except (TypeError, ValueError):
        return None
    return n if n >= 0 and n == n else None


def _finite(raw):
    try:
        n = float(raw)
    except (TypeError, ValueError):
        return None
    return n if n > 0 and n == n else None


def trace(tok):
    c = Cup("models_compute_sale_discount")
    cp, cc, original = _parse(tok)
    wp = original.get("prompt") if isinstance(original, dict) else None
    wc = original.get("completion") if isinstance(original, dict) else None

    cur_p_any = _nonneg(cp) if cp not in (None, "") else None
    cur_c_any = _nonneg(cc) if cc not in (None, "") else None
    free_hit = (cur_p_any == 0 and cur_c_any in (0, None))
    c.branch(free_hit)                                    # B1
    if free_hit:
        res = _live_fn(cp, cc, original)
        label = "free|{}|{}|{}".format(res[0],
                                       "" if wp in (None, "") else str(wp),
                                       "" if wc in (None, "") else str(wc))
        c.valstr(label)
        return c.finalize()

    c.branch(isinstance(original, dict))                  # B2
    if not isinstance(original, dict):
        c.valstr("none")
        return c.finalize()

    was_empty = (wp in (None, "")) and (wc in (None, ""))
    c.branch(was_empty)                                   # B3
    if was_empty:
        c.valstr("none")
        return c.finalize()

    cur_p = _finite(cp) if cp not in (None, "") else None
    orig_p = _finite(wp) if wp not in (None, "") else None
    cheaper = (cur_p is not None and orig_p is not None and cur_p < orig_p)
    c.branch(cheaper)                                     # B4
    if cheaper:
        pct = int(round((1.0 - (cur_p / orig_p)) * 100))
        ok = pct >= 1
        c.branch(ok)                                      # B5
        res = _live_fn(cp, cc, original)
        if ok:
            c.valstr("sale|%d|%s|%s" % (res[0], str(wp),
                                        "" if wc in (None, "") else str(wc)))
        else:
            c.valstr("none")
        return c.finalize()

    cur_c = _finite(cc) if cc not in (None, "") else None
    orig_c = _finite(wc) if wc not in (None, "") else None
    cheaper = (cur_c is not None and orig_c is not None and cur_c < orig_c)
    c.branch(cheaper)                                     # B6
    if cheaper:
        pct = int(round((1.0 - (cur_c / orig_c)) * 100))
        ok = pct >= 1
        c.branch(ok)                                      # B7
        res = _live_fn(cp, cc, original)
        if ok:
            c.valstr("sale|%d|%s|%s" % (res[0], str(wp), str(wc)))
        else:
            c.valstr("none")
        return c.finalize()

    c.valstr("none")
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
