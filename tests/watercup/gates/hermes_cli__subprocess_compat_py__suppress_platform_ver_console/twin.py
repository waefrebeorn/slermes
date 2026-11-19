"""twin for hermes_cli/_subprocess_compat.py:suppress_platform_ver_console (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_subprocess_compat.py"
    PY_FN = "suppress_platform_ver_console"
    C_FN = "hermes_cli__subprocess_compat_suppress_platform_ver_console"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
