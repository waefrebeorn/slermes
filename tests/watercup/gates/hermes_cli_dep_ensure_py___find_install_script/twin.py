"""twin for hermes_cli/dep_ensure.py:_find_install_script (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dep_ensure.py"
    PY_FN = "_find_install_script"
    C_FN = "hermes_cli_dep_ensure_u_find_install_script"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
