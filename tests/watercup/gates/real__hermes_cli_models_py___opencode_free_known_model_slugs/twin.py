"""twin for hermes_cli/models.py:_opencode_free_known_model_slugs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_opencode_free_known_model_slugs"
    C_FN = "_opencode_free_known_model_slugs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
