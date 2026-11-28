"""twin for hermes_cli/model_catalog.py:_default_model_from_block (None -> "null")."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_catalog.py"
    PY_FN = "_default_model_from_block"
    C_FN = "hermes_cli_model_catalog_u_default_model_from_block"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("null" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
