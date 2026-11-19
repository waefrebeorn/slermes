"""twin for hermes_cli/dingtalk_auth.py:wait_for_registration_success (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dingtalk_auth.py"
    PY_FN = "wait_for_registration_success"
    C_FN = "hermes_cli_dingtalk_auth_wait_for_registration_success"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
