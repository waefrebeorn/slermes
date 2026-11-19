"""twin for hermes_cli/model_catalog.py:_read_disk_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_catalog.py"
    PY_FN = "_read_disk_cache"
    C_FN = "hermes_cli_model_catalog_u_read_disk_cache"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
