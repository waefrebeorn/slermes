"""twin for hermes_cli/windows_ssh_runtime.py:_security_attributes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/windows_ssh_runtime.py"
    PY_FN = "_security_attributes"
    C_FN = "wssr_u_security_attributes"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
