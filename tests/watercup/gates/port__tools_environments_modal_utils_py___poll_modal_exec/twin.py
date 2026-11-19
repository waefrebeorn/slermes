"""twin for tools/environments/modal_utils.py:_poll_modal_exec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/modal_utils.py"
    PY_FN = "_poll_modal_exec"
    C_FN = "cli_tools_environments_modal_utils__poll_modal_exec"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
