"""twin for context_compressor_is_summary_access_or_quota_error.

Drives the LIVE agent/context_compressor.py::_is_summary_access_or_quota_error
with synthetic exceptions and traces the same rung order as the C harness:
  B1 rate_limit / B2 auth / V1 lowered text / B3 credential marker /
  B4 401|402|403 / B5 quota marker / V2 result.

Input encoding: "<live_reason_name>|<status>|<text>" where live_reason_name is
the FailoverReason name LIVE classify_api_error actually produces for the
synthetic exception ("unknown" when classification falls through).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_ctxcomp",
    os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_ctxcomp"] = _cc
_spec.loader.exec_module(_cc)

FR = _cc.FailoverReason
REASON_CODE = {
    FR.auth: 35,
    FR.auth_permanent: 36,
    FR.rate_limit: 40,
    FR.billing: 2,
}


class _SynErr(Exception):
    pass


def trace(enc):
    reason_name, _, rest = enc.partition("|")
    status_s, _, text = rest.partition("|")
    status = int(status_s) if status_s else None

    exc = _SynErr(text)
    exc.status_code = status
    classified = _cc.classify_api_error(exc)
    got = str(classified.reason.value)
    assert got == reason_name, "classification drift: %r vs %r" % (got, reason_name)
    live_result = _cc._is_summary_access_or_quota_error(exc)

    c = Cup("is_summary_access_or_quota_error")
    code = REASON_CODE.get(classified.reason, 0)
    err_text = str(exc).lower()
    result = False
    if code == 40:                                        # B1
        c.branch(True)
    else:
        c.branch(False)
        if code in (35, 36):                              # B2
            c.branch(True)
            result = True
        else:
            c.branch(False)
            c.valstr(err_text)                            # V1
            b3 = any(m in err_text for m in _cc._SUMMARY_MISSING_CREDENTIAL_MARKERS)
            c.branch(b3)                                  # B3
            if not b3:
                st = status in (401, 402, 403)
                c.branch(st)                              # B4
                if not st:
                    b5 = any(m in err_text for m in _cc._SUMMARY_PERMANENT_QUOTA_MARKERS)
                    c.branch(b5)                          # B5
                    if b5:
                        result = True
                else:
                    result = True
            else:
                result = True
    assert result == live_result, "LIVE disagreement on %r" % enc
    c.valstr("True" if result else "False")               # V2
    return c.finalize()


def run(inputs):
    return [trace(e) for e in inputs]
