"""twin for hermes_cli/migrate.py:_resolve_config_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/migrate.py"
    PY_FN = "_resolve_config_path"
    C_FN = "_resolve_config_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
