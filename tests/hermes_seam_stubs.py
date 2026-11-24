"""Stand-ins for host-blocked heavy imports, shared by differential oracles.

This host's application-control policy refuses pydantic's compiled extension:

    ImportError: DLL load failed while importing _pydantic_core: Access is denied.

Any oracle whose live module sits behind a FastAPI/pydantic import therefore
dies before it can compare a single case -- and the failure looks like a broken
port.  Those packages are seams: no model validation or HTTP routing is under
test, only the pure value functions around them.

Install the stubs BEFORE importing the module under test:

    from hermes_seam_stubs import install
    HTTPException = install()

The modules are permissive on purpose: every attribute asked of them resolves to
something usable (a plain class for UpperCase names, a pass-through decorator
otherwise), so a router module importing a long tail of pydantic/fastapi names
still loads.  Nothing here validates anything -- the fixtures are the input.
"""
import sys
import types


class BaseModel:
    """Minimal pydantic.BaseModel stand-in (attribute bag + dict round-trip)."""

    def __init__(self, **kw):
        self.__dict__.update(kw)

    @classmethod
    def model_validate(cls, obj):
        return cls(**(obj or {}))

    @classmethod
    def parse_obj(cls, obj):
        return cls(**(obj or {}))

    def model_dump(self, *a, **k):
        return dict(self.__dict__)

    def dict(self, *a, **k):
        return dict(self.__dict__)


class ValidationError(ValueError):
    pass


class HTTPException(Exception):
    def __init__(self, status_code=500, detail=None, headers=None):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail
        self.headers = headers


class APIRouter:
    """Decorator sink: route registration is irrelevant to a pure-logic gate."""

    def __init__(self, *a, **k):
        self.routes = []

    def _route(self, *a, **k):
        def deco(fn):
            return fn

        if a and callable(a[0]):
            return deco(a[0])
        return deco

    get = post = put = patch = delete = head = options = _route
    add_api_route = _route
    add_websocket_route = _route
    websocket = _route
    api_route = _route

    def __getattr__(self, attr):
        """Any further route decorator the router module asks for."""
        if attr.startswith("__"):
            raise AttributeError(attr)
        return self._route


class FastAPI:
    """Stand-in for the app object: registration calls must not raise."""

    def __init__(self, *a, **k):
        self.router = APIRouter()
        self.state = types.SimpleNamespace()
        self.user_middleware = []

    def add_middleware(self, *a, **k):
        self.user_middleware.append((a, k))

    def middleware(self, *a, **k):
        def deco(fn):
            return fn

        if a and callable(a[0]):
            return a[0]
        return deco

    def mount(self, *a, **k):
        return None

    def include_router(self, *a, **k):
        return None

    def add_api_route(self, *a, **k):
        return None

    def exception_handler(self, *a, **k):
        def deco(fn):
            return fn

        return deco

    def on_event(self, *a, **k):
        def deco(fn):
            return fn

        return deco

    get = post = put = patch = delete = APIRouter._route
    websocket = APIRouter._route


class Response:
    def __init__(self, content=None, status_code=200, **kw):
        self.content = content
        self.status_code = status_code


def _field(default=None, *a, **k):
    return default


def _decorator(*a, **k):
    if a and callable(a[0]):
        return a[0]

    def wrap(fn):
        return fn

    return wrap


def _create_model(name, **fields):
    return type(name, (BaseModel,), {})


def _permissive(mod):
    def _ga(attr):
        if attr.startswith("__"):
            raise AttributeError(attr)
        if attr in ("Field", "Query", "Body", "Header", "Path", "Cookie", "Form", "File"):
            return _field
        if "valid" in attr or attr in ("Depends", "depends"):
            return _decorator
        if attr[:1].isupper():
            return type(attr, (BaseModel,), {})
        return _decorator

    mod.__getattr__ = _ga
    return mod


def _module(name, **attrs):
    mod = types.ModuleType(name)
    # __path__ makes the stand-in a package, so `fastapi.middleware.cors` and
    # friends resolve through the finder instead of raising ModuleNotFoundError
    # ('fastapi' is not a package) -- which a router import turns into a
    # "Web UI requires fastapi and uvicorn" SystemExit.
    mod.__path__ = []
    for key, value in attrs.items():
        setattr(mod, key, value)
    sys.modules[name] = mod
    return mod


def install(force=False):
    """Install pydantic/fastapi/starlette stand-ins and return HTTPException.

    Existing real modules are left alone unless ``force`` is set: an oracle that
    can import the real stack should keep using it.
    """
    def blocked(name):
        mod = sys.modules.get(name)
        return force or mod is None

    if blocked("pydantic"):
        _module(
            "pydantic",
            BaseModel=BaseModel,
            Field=_field,
            field_validator=_decorator,
            validator=_decorator,
            model_validator=_decorator,
            root_validator=_decorator,
            ConfigDict=dict,
            ValidationError=ValidationError,
            SecretStr=str,
            StrictBool=bool,
            StrictInt=int,
            StrictStr=str,
            StrictFloat=float,
            create_model=_create_model,
        )
        _module("pydantic.fields", Field=_field)
        _module("pydantic.main", BaseModel=BaseModel)
        _module("pydantic_core", PydanticUndefined=None)
    if blocked("pydantic_settings"):
        _module("pydantic_settings", BaseSettings=BaseModel, SettingsConfigDict=dict)
    if blocked("fastapi"):
        _module(
            "fastapi",
            APIRouter=APIRouter,
            FastAPI=FastAPI,
            HTTPException=HTTPException,
            Request=object,
            Response=Response,
            JSONResponse=Response,
            PlainTextResponse=Response,
            StreamingResponse=Response,
            Depends=lambda *a, **k: None,
            Query=_field,
            Body=_field,
            Header=_field,
            Path=_field,
            BackgroundTasks=object,
            WebSocket=object,
            status=types.SimpleNamespace(HTTP_200_OK=200, HTTP_400_BAD_REQUEST=400,
                                         HTTP_401_UNAUTHORIZED=401,
                                         HTTP_403_FORBIDDEN=403,
                                         HTTP_404_NOT_FOUND=404),
        )
        _module("fastapi.responses", JSONResponse=Response, PlainTextResponse=Response,
                Response=Response, StreamingResponse=Response)
        _module("fastapi.security", HTTPBearer=object, HTTPAuthorizationCredentials=object)
    if blocked("starlette"):
        _module("starlette").__path__ = []
    for name in ("starlette.requests", "starlette.responses", "starlette.routing",
                 "starlette.websockets", "starlette.datastructures",
                 "starlette.background", "starlette.status"):
        if blocked(name):
            _module(name, Request=object, Response=Response, JSONResponse=Response,
                    WebSocket=object, Route=object, Headers=dict, QueryParams=dict,
                    BackgroundTasks=object)

    for name in ("pydantic", "pydantic.fields", "pydantic.main", "pydantic_settings",
                 "fastapi", "fastapi.responses"):
        mod = sys.modules.get(name)
        if mod is not None and not hasattr(mod, "__getattr__"):
            _permissive(mod)
    if blocked("psutil"):
        _module(
            "psutil",
            Error=Exception,
            NoSuchProcess=type("NoSuchProcess", (Exception,), {}),
            AccessDenied=type("AccessDenied", (Exception,), {}),
            ZombieProcess=type("ZombieProcess", (Exception,), {}),
            TimeoutExpired=type("TimeoutExpired", (Exception,), {}),
            Process=object,
            Popen=object,
            cpu_count=lambda *a, **k: 1,
            cpu_percent=lambda *a, **k: 0.0,
            virtual_memory=lambda *a, **k: types.SimpleNamespace(
                total=0, available=0, percent=0.0, used=0, free=0),
            disk_usage=lambda *a, **k: types.SimpleNamespace(
                total=0, used=0, free=0, percent=0.0),
            net_connections=lambda *a, **k: [],
            pid_exists=lambda *a, **k: False,
            wait_procs=lambda *a, **k: ([], []),
        )
    if blocked("uvicorn"):
        _module(
            "uvicorn",
            run=lambda *a, **k: None,
            Config=object,
            Server=object,
        )
    _install_finder()
    return HTTPException


_STUB_ROOTS = ("pydantic", "pydantic_settings", "fastapi", "starlette",
               "psutil", "uvicorn", "httpx", "websockets", "multipart")


class _StubFinder:
    """Fabrics any remaining submodule of a stubbed package.

    Enumerating starlette.formparsers, starlette.templating, ... one by one is a
    losing game: a router module imported by file path drags whichever tail it
    happens to touch. Anything under a stubbed root resolves to a permissive
    module, so the import graph loads and the pure functions can be compared.
    """

    def find_spec(self, fullname, path=None, target=None):
        root = fullname.split(".")[0]
        if root not in _STUB_ROOTS or fullname in sys.modules:
            return None
        import importlib.machinery

        return importlib.machinery.ModuleSpec(fullname, self, is_package=True)

    def create_module(self, spec):
        mod = types.ModuleType(spec.name)
        mod.__path__ = []
        sys.modules[spec.name] = mod
        return mod

    def exec_module(self, module):
        _permissive(module)


def _install_finder():
    if not any(isinstance(f, _StubFinder) for f in sys.meta_path):
        sys.meta_path.insert(0, _StubFinder())
