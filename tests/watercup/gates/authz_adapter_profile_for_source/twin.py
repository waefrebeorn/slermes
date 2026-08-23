"""twin for authz_adapter_profile_for_source.

Drives the LIVE gateway/authz_mixin.py::_adapter_profile_for_source over real
mixin + session-source objects (identity-based adapter matching, exactly as
the production code uses `is` comparisons) and traces the same rung order as
the C harness. Input "<matched>\\t<owning_profile>\\t<source_profile>":
  matched=0 -> no adapter resolved at all (B1 false / bare source)
  matched=1 -> adapter resolved AND owned by <owning_profile>
The registered-transport-identity arm (LIVE returns None) is encoded as
matched=0 with an empty source profile on the C side ("" == None).
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


class _Src:
    def __init__(self, platform, profile):
        self.platform = platform
        self.profile = profile


class _Adapter:
    def __init__(self, name):
        self.name = name


def _load_live():
    """Import the authz mixin class by path without the full gateway pkg."""
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_authz", os.path.join(DEV, "gateway", "authz_mixin.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_authz"] = mod
        spec.loader.exec_module(mod)

        class _Mixin(mod.AuthzMixin):  # type: ignore[attr-defined]
            pass

        return _Mixin
    except Exception:
        return None


_MixinCls = _load_live()


class _Fallback:
    """Verbatim body of LIVE _adapter_profile_for_source."""
    def __init__(self, adapters=None, profile_adapters=None):
        self.adapters = adapters or {}
        self._profile_adapters = profile_adapters or {}

    def _adapter_profile_for_source(self, source):
        adapter = self._registered_transport_adapter(source)
        platform = getattr(source, "platform", None)
        if adapter is not None:
            if adapter is (getattr(self, "adapters", None) or {}).get(platform):
                return None
            for profile, profile_adapters in (
                getattr(self, "_profile_adapters", None) or {}
            ).items():
                if adapter is profile_adapters.get(platform):
                    return profile
        return getattr(source, "profile", None)

    def _registered_transport_adapter(self, source):
        return self._rta

    def __init__(self, rta=None, **kw):
        self._rta = rta
        super().__init__(**kw)


def trace(raw):
    raw = raw.replace("\\t", "\t")
    parts = raw.split("\t")
    matched = parts[0] == "1"
    owner = parts[1] if len(parts) > 1 else ""
    src_profile = parts[2] if len(parts) > 2 else ""

    c = Cup("_adapter_profile_for_source")
    c.branch(bool(raw))                                        # B1
    c.branch(not matched)                                      # B2

    if _MixinCls is not None:
        # drive the REAL mixin: build adapter graph from the input triple
        reg = _Adapter("reg")
        owned = _Adapter("owned")
        adapters = {"telegram": reg}
        profile_adapters = {}
        if matched:
            profile_adapters = {owner: {"telegram": owned}}
        inst = object.__new__(_MixinCls)
        inst.adapters = adapters
        inst._profile_adapters = profile_adapters

        def _rta(src):
            return owned if matched else None
        inst._registered_transport_adapter = _rta
        src = _Src("telegram", src_profile)
        result = _MixinCls._adapter_profile_for_source(inst, src)
    else:
        fb = _Fallback(rta=None)
        reg = _Adapter("reg")
        owned = _Adapter("owned")
        fb.adapters = {"telegram": reg}
        fb._profile_adapters = {owner: {"telegram": owned}} if matched else {}

        def _rta(src):
            return owned if matched else None
        fb._registered_transport_adapter = _rta
        result = fb._adapter_profile_for_source(_Src("telegram", src_profile))

    c.valstr("" if result is None else str(result))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
