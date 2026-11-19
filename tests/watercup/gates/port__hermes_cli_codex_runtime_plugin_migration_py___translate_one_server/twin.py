"""twin for hermes_cli/codex_runtime_plugin_migration.py:_translate_one_server (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_plugin_migration.py"
    PY_FN = "_translate_one_server"
    C_FN = "hermes_cli_codex_runtime_plugi_u_translate_one_server"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
