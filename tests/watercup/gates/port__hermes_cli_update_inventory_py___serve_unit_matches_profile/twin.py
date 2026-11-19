"""twin for hermes_cli/update_inventory.py:_serve_unit_matches_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_inventory.py"
    PY_FN = "_serve_unit_matches_profile"
    C_FN = "_serve_unit_matches_profile"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
