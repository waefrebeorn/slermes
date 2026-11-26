"""twin for agent/auxiliary_client.py:_compression_config_claims_fast_lane (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase, Cup



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_compression_config_claims_fast_lane"
    C_FN = "_compression_config_claims_fast_lane"

    def trace(self, inp, inputs, idx=0):
        """The fn reads dict keys, so decode the JSON config first.

        call_fn handed it the raw string, `.get` raised AttributeError and every case produced the
        same ERROR cup -- a mismatch that said nothing about the port.
        """
        c = Cup("_compression_config_claims_fast_lane")
        try:
            import json as _json
            try:
                cfg = _json.loads(inp) if isinstance(inp, str) else inp
            except Exception:
                cfg = None
            if not isinstance(cfg, dict):
                cfg = {}
            self.norm(c, self._fn(cfg))
        except Exception as e:  # noqa: BLE001
            c.valstr("ERROR:%s" % type(e).__name__)
        return c.finalize()

    def norm(self, c, result):
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run
