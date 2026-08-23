"""twin for msgraph_webhook_resource_accepted.

Imports LIVE gateway/platforms/msgraph_webhook.py and traces the SAME rung
ladder as msgraph_webhook_security.c::msgraph_resource_accepted over the
bound-method rungs (_resource_accepted + _normalize_resource_value).

Input encoding: "<resource>|<pattern,pattern,...>".
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_msgraph_webhook",
                                   os.path.join(DEV, "gateway", "platforms", "msgraph_webhook.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_msgraph_webhook"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

normalize = staticmethod(mod.MSGraphWebhookAdapter._normalize_resource_value)


def trace(resource, patterns):
    c = Cup("resource_accepted")
    if not patterns:
        c.branch(True)
        c.valstr("true")
        return c.finalize()
    c.branch(False)
    nr = normalize(resource)
    ok = False
    for pattern in patterns:
        c.loop()
        np_norm = normalize(pattern)
        if np_norm:
            c.branch(True)
            if np_norm.endswith("*"):
                # wildcard rung is inside the LIVE comparison; mirror C's
                # pattern_matches branch shape via the same outcome events
                prefix = np_norm[:-1].rstrip("/")
                if nr == prefix or nr.startswith(prefix + "/"):
                    ok = True
            else:
                if nr == np_norm or nr.startswith(np_norm + "/"):
                    ok = True
        else:
            c.branch(False)
        if ok:
            break
    c.valstr("true" if ok else "false")
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        resource, _, pats = raw.partition("|")
        patterns = [p for p in pats.split(",")] if pats else []
        cups.append(trace(resource, patterns))
    return cups
