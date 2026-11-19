"""twin for hermes_cli/urllib_security.py:_secure_opener_from_installed_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/urllib_security.py"
    PY_FN = "_secure_opener_from_installed_policy"
    C_FN = "hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
