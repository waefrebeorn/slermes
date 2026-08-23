"""twin for security_audit_osv_severity_from_record.

Traces the LIVE hermes_cli/security_audit.py::_osv_severity_from_record with
the SAME coarse rung ladder as the C harness, then calls the LIVE function for
the result value.
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_security_audit",
    os.path.join(DEV, "hermes_cli", "security_audit.py"))
_sa = ilu.module_from_spec(_spec)
sys.modules["wc_live_security_audit"] = _sa
if _spec.loader is not None:
    _spec.loader.exec_module(_sa)

ORDER = _sa.SEVERITY_ORDER


def trace(record):
    c = Cup("osv_severity_from_record")
    c.branch(isinstance(record, dict))                          # B1
    if not isinstance(record, dict):
        pass
    else:
        db = record.get("database_specific")
        db_ok = isinstance(db, dict)
        c.branch(db_ok)                                         # B2
        done = False
        if db_ok:
            raw = db.get("severity")
            ok = isinstance(raw, str) and bool(raw.strip())
            c.branch(ok)                                        # B3
            if ok:
                up = raw.strip().upper()
                c.valstr(up)
                hit = up in ORDER
                c.branch(hit)                                   # B4
                if hit:
                    done = True
        if not done:
            # C checks the raw key (NULL -> skip); LIVE folds missing to []
            # via `or []` — same observable behavior, traced as a raw check.
            affected = record.get("affected")
            c.branch(isinstance(affected, list))                # B5
            if isinstance(affected, list):
                for entry in affected:
                    c.loop()
                    eo = isinstance(entry, dict)
                    c.branch(eo)
                    if not eo:
                        continue
                    eco = entry.get("ecosystem_specific") or {}
                    eo2 = isinstance(eco, dict)
                    c.branch(eo2)
                    if not eo2:
                        continue
                    sev = eco.get("severity")
                    so = isinstance(sev, str) and bool(sev.strip())
                    c.branch(so)
                    if not so:
                        continue
                    up = sev.strip().upper()
                    c.valstr(up)
                    hit = up in ORDER
                    c.branch(hit)
                    if hit:
                        break
    result = _sa._osv_severity_from_record(record)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(_json.loads(raw)) for raw in inputs]
