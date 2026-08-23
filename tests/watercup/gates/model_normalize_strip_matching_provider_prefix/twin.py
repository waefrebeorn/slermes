"""twin for model_normalize_strip_matching_provider_prefix.

Imports LIVE hermes_cli/model_normalize.py::_strip_matching_provider_prefix and
traces the SAME branch ladder as the C harness: slash-present, non-empty prefix,
non-empty remainder, then provider-alias equality. Alias resolution goes through
LIVE _normalize_provider_alias -> hermes_cli.models.normalize_provider.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_model_normalize2", os.path.join(DEV, "hermes_cli", "model_normalize.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_model_normalize2"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        model_name, _, target = raw.partition("\t")
        c = Cup("strip_matching_provider_prefix")
        c.branch("/" in model_name)                      # b0
        done = False
        result = model_name
        if "/" not in model_name:
            done = True
        prefix, _, remainder = model_name.partition("/")
        c.branch(bool(prefix.strip()))                   # b1: prefix non-empty
        if not prefix.strip():
            done = True
        c.branch(bool(remainder.strip()))                # b2: remainder non-empty
        if not remainder.strip():
            done = True
        if not done:
            np = mod._normalize_provider_alias(prefix)
            nt = mod._normalize_provider_alias(target)
            match = bool(np) and bool(nt) and np == nt
            c.branch(match)                              # b3
            result = remainder.strip() if match else model_name
        # cross-check against the LIVE fn so a silent divergence fails loudly
        live = mod._strip_matching_provider_prefix(model_name, target)
        assert live == result, "LIVE diverged on %r: %r vs %r" % (raw, live, result)
        c.valstr(result)
        cups.append(c.finalize())
    return cups
