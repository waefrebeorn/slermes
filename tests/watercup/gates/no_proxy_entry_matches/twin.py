"""twin for no_proxy_entry_matches.

Imports LIVE gateway/platforms/base.py::_no_proxy_entry_matches and traces the
SAME rung ladder as the C harness. Inputs are "<entry>|<host>|<port>" triples
(port -1 means None). IP/CIDR inputs are out of scope: the C port omits the
ipaddress branches (documented), so only domain/wildcard cases are exercised.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_gp_base",
                                   os.path.join(DEV, "gateway", "platforms", "base.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_gp_base"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    fn = mod._no_proxy_entry_matches
    for raw in inputs:
        entry, host, port_s = raw.split("|")
        port = int(port_s)
        c = Cup("no_proxy_entry_matches")

        # mirror C rungs
        if not entry or not host:
            c.branch(False)   # C null-guard rung (unreachable via argv)
            c.branch(True)
            result = 0
        else:
            c.branch(False)
            token = entry.strip(" \t").lower()
            if not token:
                c.branch(True)
                result = 0
            else:
                c.branch(False)
                if token == "*":
                    c.branch(True)
                    result = 1
                else:
                    c.branch(False)
                    token_host, token_port = mod._split_host_port(token)
                    hp = port if port != -1 else None
                    tp = token_port if token_port is not None else -1
                    if tp != -1 and hp is not None and tp != hp:
                        c.branch(True)
                        result = 0
                    else:
                        c.branch(False)
                        if tp != -1 and hp is None:
                            c.branch(True)
                            result = 0
                        else:
                            c.branch(False)
                            # LIVE ipaddress blocks raise ValueError on domain
                            # tokens (C omits them entirely — untraced on both).
                            if token_host.startswith("*."):
                                c.branch(True)
                                suffix = token_host[1:]
                                result = 1 if host.lower().endswith(suffix) else 0
                            else:
                                c.branch(False)
                                if token_host.startswith("."):
                                    c.branch(True)
                                    ok = host == token_host[1:] or host.endswith(token_host)
                                    result = 1 if ok else 0
                                else:
                                    c.branch(False)
                                    ok = host == token_host or host.endswith("." + token_host)
                                    result = 1 if ok else 0

        assert fn(entry, host, None if port == -1 else port) == bool(result), \
            (raw, fn(entry, host, None if port == -1 else port), result)
        c.valstr(str(result))
        cups.append(c.finalize())
    return cups
