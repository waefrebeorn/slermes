"""twin for tools/computer_use/cua_backend.py:_manifest_is_mode_independent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/cua_backend.py"
    PY_FN = "_manifest_is_mode_independent"
    C_FN = "cua_manifest_is_mode_independent"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
