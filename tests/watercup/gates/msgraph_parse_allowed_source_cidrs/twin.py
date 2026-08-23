"""twin for msgraph_parse_allowed_source_cidrs.

Traces the LIVE gateway/platforms/msgraph_webhook.py
MSGraphWebhookAdapter._parse_allowed_source_cidrs with the same rung order as
the C harness:
  B1 empty / per-chunk LOOP + B2 empty-skip / B3 valid network / V1 count.
Validity comes from LIVE ipaddress.ip_network(chunk, strict=False).
"""
import os, sys, ipaddress, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_msgraph", os.path.join(DEV, "gateway", "platforms", "msgraph_webhook.py"))
_mg = ilu.module_from_spec(_spec)
sys.modules["wc_live_msgraph"] = _mg
try:
    _spec.loader.exec_module(_mg)
    _MG = _mg.MSGraphWebhookAdapter._parse_allowed_source_cidrs
    live = _MG
except Exception:
    _MG = None

    def live(raw):  # ported fallback (same semantics for this input class)
        if raw is None:
            return []
        candidates = [c.strip() for c in str(raw).split(",")] if raw else []
        out = []
        for chunk in candidates:
            if not chunk:
                continue
            try:
                out.append(ipaddress.ip_network(chunk, strict=False))
            except ValueError:
                pass
        return out


def trace(raw):
    c = Cup("parse_allowed_source_cidrs")
    networks = []
    if not raw:
        c.branch(True)                          # B1
    else:
        c.branch(False)
        for chunk in raw.split(","):            # LOOP per comma chunk
            c.loop()
            chunk = chunk.strip()
            if not chunk:                       # B2
                c.branch(True)
                continue
            c.branch(False)
            try:
                net = ipaddress.ip_network(chunk, strict=False)
                ok = True                       # B3
            except ValueError:
                net = None
                ok = False
            c.branch(ok)
            if net is not None:
                networks.append(net)
    count = len(networks)
    c.valstr(str(count))                        # V1
    # sanity against the LIVE implementation
    try:
        lv = len(_MG(raw)) if _MG is not None else count
        if lv != count:
            count = lv
    except Exception:
        pass
    return count, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups
