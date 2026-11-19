"""twin for hermes_cli/plugins.py:_classify_entrypoint_value_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_classify_entrypoint_value_kind"
    C_FN = "_classify_entrypoint_value_kind"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
