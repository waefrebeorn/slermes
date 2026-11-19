"""twin for gateway/hosted_room_replicas.py:_replica_transaction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_replicas.py"
    PY_FN = "_replica_transaction"
    C_FN = "_replica_transaction"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
