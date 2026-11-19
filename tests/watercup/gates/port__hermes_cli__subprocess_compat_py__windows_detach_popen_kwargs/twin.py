"""twin for hermes_cli/_subprocess_compat.py:windows_detach_popen_kwargs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_subprocess_compat.py"
    PY_FN = "windows_detach_popen_kwargs"
    C_FN = "hermes_cli__subprocess_compat_windows_detach_popen_kwargs"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
