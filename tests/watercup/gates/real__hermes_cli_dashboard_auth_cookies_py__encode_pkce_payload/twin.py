"""twin for hermes_cli/dashboard_auth/cookies.py:encode_pkce_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/cookies.py"
    PY_FN = "encode_pkce_payload"
    C_FN = "encode_pkce_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
