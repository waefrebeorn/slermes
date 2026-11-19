"""twin for hermes_cli/macos_tcc_anchor.py:_store_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "_store_root"
    C_FN = "_store_root"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
