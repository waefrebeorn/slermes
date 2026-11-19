"""twin for hermes_cli/update_receipt.py:collect_fleet_versions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_receipt.py"
    PY_FN = "collect_fleet_versions"
    C_FN = "collect_fleet_versions"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
