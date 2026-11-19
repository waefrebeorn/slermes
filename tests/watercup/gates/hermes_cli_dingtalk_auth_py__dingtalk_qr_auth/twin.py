"""twin for hermes_cli/dingtalk_auth.py:dingtalk_qr_auth (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dingtalk_auth.py"
    PY_FN = "dingtalk_qr_auth"
    C_FN = "hermes_cli_dingtalk_auth_dingtalk_qr_auth"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
