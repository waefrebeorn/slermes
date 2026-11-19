"""twin for hermes_cli/runtime_provider.py:_try_resolve_from_custom_pool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_try_resolve_from_custom_pool"
    C_FN = "_try_resolve_from_custom_pool"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
