"""twin for base_safe_url_for_log.

Imports LIVE gateway/platforms/base.py::safe_url_for_log and traces the SAME
branch ladder as the C harness over "<max_len>|<url>" inputs.
"""
import os, sys
from urllib.parse import urlsplit
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_gw_base", os.path.join(DEV, "gateway", "platforms", "base.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_gw_base"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for inp in inputs:
        max_s, _, url = inp.partition("|")
        try:
            max_len = int(max_s)
        except ValueError:
            max_len = 80
        c = Cup("safe_url_for_log")
        raw = str(url)
        if max_len <= 0:
            c.branch(True); safe = ""
        else:
            c.branch(False)
            if not raw:
                c.branch(True); safe = ""
            else:
                c.branch(False)
                parsed = urlsplit(raw)
                if parsed.scheme and parsed.netloc:
                    c.branch(True)
                    netloc = parsed.netloc.rsplit("@", 1)[-1]
                    base = f"{parsed.scheme}://{netloc}"
                    path = parsed.path or ""
                    if path and path != "/":
                        c.branch(True)
                        basename = path.rsplit("/", 1)[-1]
                        safe = f"{base}/.../{basename}" if basename else f"{base}/..."
                    else:
                        c.branch(False)
                        safe = base
                else:
                    c.branch(False)
                    safe = raw
                if len(safe) <= max_len:
                    c.branch(True)
                else:
                    c.branch(False)
                    if max_len <= 3:
                        c.branch(True); safe = "." * max_len
                    else:
                        c.branch(False); safe = f"{safe[:max_len - 3]}..."
        live = mod.safe_url_for_log(url, max_len)
        c.valstr(live if isinstance(live, str) else str(live))
        cups.append(c.finalize())
    return cups
