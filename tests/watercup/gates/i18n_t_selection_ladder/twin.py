"""twin for i18n_t_selection_ladder.

Traces LIVE agent/i18n.py::t with the same rung order as the C harness:
  B1 explicit lang / V1 normalized target / B2 miss->fallback / B3 (folded
  into B2: fallback only when target != en) / B4 bare-key last resort /
  V2 final message. Catalog values come from the LIVE YAML loader.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_i18n", os.path.join(DEV, "agent", "i18n.py"))
_i18n = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_i18n"] = _i18n
_spec.loader.exec_module(_i18n)

os.environ.pop("HERMES_LANGUAGE", None)


def trace(key, lang, kwargs):
    c = Cup("i18n_t")
    # B1: explicit lang
    if lang:
        c.branch(True)
        target = _i18n._normalize_lang(lang)
    else:
        c.branch(False)
        target = _i18n.get_language()
    c.valstr(target)

    value = _i18n._load_catalog(target).get(key)

    # B2: miss -> English fallback (only when target != en, as in LIVE t())
    if value is None and target != _i18n.DEFAULT_LANGUAGE:
        c.branch(True)
        value = _i18n._load_catalog(_i18n.DEFAULT_LANGUAGE).get(key)
    else:
        c.branch(False)

    if value is None:                       # B4
        c.branch(True)
        result = key
    else:
        c.branch(False)
        if kwargs:
            try:
                result = value.format(**kwargs)
            except (KeyError, IndexError, ValueError):
                result = value
        else:
            result = value
    c.valstr(result)
    c.finalize()
    return result, c


def run(inputs):
    outs = []
    for probe in inputs:
        parts = probe.split("|")
        parts += ["", ""]
        kwargs = {}
        if parts[2]:
            for pair in parts[2].split(","):
                if "=" in pair:
                    k, _, v = pair.partition("=")
                    kwargs[k] = v
        outs.append(trace(parts[0], parts[1], kwargs)[1].finalize())
    return outs
