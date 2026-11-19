"""twin for hermes_cli/model_catalog.py:_default_model_from_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_catalog.py"
    PY_FN = "_default_model_from_block"
    C_FN = "hermes_cli_model_catalog_u_default_model_from_block"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
