"""twin for hermes_cli/__init__.py:_ensure_utf8 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/__init__.py"
    PY_FN = "_ensure_utf8"
    C_FN = "hcli_ensure_utf8"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
