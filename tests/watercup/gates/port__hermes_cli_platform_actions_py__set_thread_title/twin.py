"""twin for hermes_cli/platform_actions.py:set_thread_title (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/platform_actions.py"
    PY_FN = "set_thread_title"
    C_FN = "platform_actions_set_thread_title"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
