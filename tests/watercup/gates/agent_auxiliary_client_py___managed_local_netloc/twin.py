"""twin for agent/auxiliary_client.py:_managed_local_netloc (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_managed_local.py"
    PY_FN = "_managed_local_netloc"
    C_FN = "_managed_local_netloc"

    def norm(self, c, result):
        # Python returns "host:port" ("" when there is no managed server). The C port is a
        # predicate over the same state, so the shared projection is the boolean, not the string:
        # emitting the raw string made every case mismatch on the value's type, not its meaning.
        if isinstance(result, str):
            c.valstr("1" if result.strip() else "0")
            return
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run
