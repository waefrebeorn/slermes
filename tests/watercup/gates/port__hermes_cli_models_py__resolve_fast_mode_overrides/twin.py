"""twin for hermes_cli/models.py:resolve_fast_mode_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "resolve_fast_mode_overrides"
    C_FN = "models_resolve_fast_mode_overrides"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
