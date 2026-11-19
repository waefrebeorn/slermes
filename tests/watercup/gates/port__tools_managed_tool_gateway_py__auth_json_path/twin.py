"""twin for tools/managed_tool_gateway.py:auth_json_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/managed_tool_gateway.py"
    PY_FN = "auth_json_path"
    C_FN = "cli_tools_managed_tool_gateway_auth_json_path"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
