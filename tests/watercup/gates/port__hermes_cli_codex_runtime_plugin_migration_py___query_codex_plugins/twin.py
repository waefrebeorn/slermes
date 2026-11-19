"""twin for hermes_cli/codex_runtime_plugin_migration.py:_query_codex_plugins (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_plugin_migration.py"
    PY_FN = "_query_codex_plugins"
    C_FN = "hermes_cli_codex_runtime_plugi_u_query_codex_plugins"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
