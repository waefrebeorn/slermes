"""twin for hermes_cli/main.py:_windows_shim_in_process_chain (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_windows_shim_in_process_chain"
    C_FN = "_windows_shim_in_process_chain"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
