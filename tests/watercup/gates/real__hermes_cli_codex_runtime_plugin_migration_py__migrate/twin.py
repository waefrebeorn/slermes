"""twin for hermes_cli/codex_runtime_plugin_migration.py:migrate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_plugin_migration.py"
    PY_FN = "migrate"
    C_FN = "migrate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
