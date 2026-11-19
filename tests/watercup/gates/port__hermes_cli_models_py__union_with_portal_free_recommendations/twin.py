"""twin for hermes_cli/models.py:union_with_portal_free_recommendations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "union_with_portal_free_recommendations"
    C_FN = "union_with_portal_free_recommendations"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
