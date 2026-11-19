"""twin for hermes_cli/dingtalk_auth.py:_ensure_qrcode_installed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dingtalk_auth.py"
    PY_FN = "_ensure_qrcode_installed"
    C_FN = "hermes_cli_dingtalk_auth_u_ensure_qrcode_installed"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
