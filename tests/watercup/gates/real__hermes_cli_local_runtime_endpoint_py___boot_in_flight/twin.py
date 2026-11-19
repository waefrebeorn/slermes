"""twin for hermes_cli/local_runtime/endpoint.py:_boot_in_flight (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/endpoint.py"
    PY_FN = "_boot_in_flight"
    C_FN = "_boot_in_flight"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
