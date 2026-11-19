"""twin for gateway/session_state.py:legacy_lease_token_property (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_state.py"
    PY_FN = "legacy_lease_token_property"
    C_FN = "relay_turn_lease_token_view_new"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
