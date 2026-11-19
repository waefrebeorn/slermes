"""twin for hermes_cli/plugins.py:_tool_override_allowed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_tool_override_allowed"
    C_FN = "plug_tool_override_allowed"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
