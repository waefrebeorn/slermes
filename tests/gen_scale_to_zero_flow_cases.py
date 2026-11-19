#!/usr/bin/env python3
"""Generate tests/fixtures/scale_to_zero_flow.jsonl for the scale_to_zero_flow
oracle (>=5 cases per ported func). Each case is {"func", "in": {..., "io": {...}}}
with io nested INSIDE in (the universal-seam script map). Script shapes: plain
value (reused), {"__seq__": [...]} (per-call, last repeats), {"__raise__": kind,
"msg": ...} (real exception). ASCII scope; integral floats == ints.

Run: python tests/gen_scale_to_zero_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "scale_to_zero_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def seq(*vals):
    return {"__seq__": list(vals)}


def raise_(kind, msg, **extra):
    d = {"__raise__": kind, "msg": msg}
    d.update(extra)
    return d


FLY = {"FLY_APP_NAME": "app", "FLY_MACHINE_ID": "m1"}
SLEEP = "GATEWAY_RELAY_SLEEP_URL"


# ── _env_str ──
case("env_str", env={"K": "  hello  "}, key="K", io={})
case("env_str", env={"K": None}, key="K", io={})
case("env_str", env={}, key="K", io={})
case("env_str", env=None, key="K", io={"os.environ": {"K": " x "}})
case("env_str", env=None, key="K", io={"os.environ": None})
case("env_str", env={"K": 123}, key="K", io={})
case("env_str", env={"K": True}, key="K", io={})
case("env_str", env={"K": "\t\n a \x1c"}, key="K", io={})
case("env_str", env=None, key="K", io={})  # fail-closed os.environ
case("env_str", env={"K": "   "}, key="K", io={})
case("env_str", env={"K": "None"}, key="K", io={})
case("env_str", env={"K": ""}, key="K", io={})

# ── self_suspend_available ──
case("self_suspend_available", environ=dict(FLY), io={"os.path.exists": True})
case("self_suspend_available", environ={"FLY_APP_NAME": "", "FLY_MACHINE_ID": "m"}, io={})
case("self_suspend_available", environ={"FLY_APP_NAME": "app", "FLY_MACHINE_ID": " "}, io={})
case("self_suspend_available", environ=dict(FLY), io={"os.path.exists": False})
case("self_suspend_available", environ=None,
     io={"os.environ": dict(FLY), "os.path.exists": True})
case("self_suspend_available", environ=dict(FLY),
     io={"os.path.exists": raise_("OSError", "no socket")})
case("self_suspend_available", environ=None, io={"os.environ": None})
case("self_suspend_available", environ=dict(FLY), io={"os.path.exists": 1})
case("self_suspend_available", environ={"FLY_APP_NAME": "app"}, io={})
case("self_suspend_available", environ=dict(FLY), io={})  # fail-closed os.path.exists

# ── brokered_sleep_url ──
case("brokered_sleep_url", environ={SLEEP: "https://x.example/sleep"},
     io={"urlsplit": {"scheme": "https", "netloc": "x.example"}})
case("brokered_sleep_url", environ={SLEEP: ""}, io={})
case("brokered_sleep_url", environ={SLEEP: "  https://x  "},
     io={"urlsplit": {"scheme": "https", "netloc": "x"}})
case("brokered_sleep_url", environ={SLEEP: "http://x"},
     io={"urlsplit": {"scheme": "http", "netloc": "x"}, "logger.warning": None})
case("brokered_sleep_url", environ={SLEEP: "https://"},
     io={"urlsplit": {"scheme": "https", "netloc": ""}, "logger.warning": None})
case("brokered_sleep_url", environ={}, io={})
case("brokered_sleep_url", environ=None,
     io={"os.environ": {SLEEP: "https://x"}, "urlsplit": {"scheme": "https", "netloc": "x"}})
case("brokered_sleep_url", environ=None, io={"os.environ": None})
case("brokered_sleep_url", environ={SLEEP: "http://x"}, state={"malformed_logged": True},
     io={"urlsplit": {"scheme": "http", "netloc": "x"}})
case("brokered_sleep_url", environ={SLEEP: "https://x"},
     io={"urlsplit": raise_("ValueError", "bad url")})
case("brokered_sleep_url", environ={SLEEP: "https://x"}, io={"urlsplit": None})
case("brokered_sleep_url", environ={SLEEP: "http://x"},
     io={"urlsplit": {"scheme": "http", "netloc": "x"}})  # fail-closed logger.warning
case("brokered_sleep_url", environ={SLEEP: None},
     io={"urlsplit": {"scheme": "https", "netloc": "x"}})
case("brokered_sleep_url", environ={SLEEP: "ftp://x"},
     io={"urlsplit": {"scheme": "ftp", "netloc": "x"}, "logger.warning": None})

# ── suspend_available ──
case("suspend_available", environ=dict(FLY), io={"os.path.exists": True})
case("suspend_available", environ={"FLY_APP_NAME": "", "FLY_MACHINE_ID": "m", SLEEP: "https://x"},
     io={"urlsplit": {"scheme": "https", "netloc": "x"}})
case("suspend_available", environ={"FLY_APP_NAME": "app", "FLY_MACHINE_ID": "m", SLEEP: ""},
     io={"os.path.exists": False})
case("suspend_available", environ=None,
     io={"os.environ": dict(FLY), "os.path.exists": True})
case("suspend_available", environ=dict(FLY),
     io={"os.path.exists": raise_("OSError", "boom")})
case("suspend_available", environ={"FLY_APP_NAME": "", SLEEP: "http://x"},
     io={"urlsplit": {"scheme": "http", "netloc": "x"}, "logger.warning": None})
case("suspend_available", environ=None, io={"os.environ": None})
case("suspend_available", environ={"FLY_APP_NAME": "app", "FLY_MACHINE_ID": "m", SLEEP: "https://x"},
     io={"os.path.exists": False, "urlsplit": {"scheme": "https", "netloc": "x"}})

# ── request_brokered_suspend ──
REQ = {"urllib.request.Request": {"req": 1}}


def req_io(**extra):
    d = dict(REQ)
    d.update(extra)
    return d


case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200}, **{"response.__enter__": None, "response.__exit__": None, "logger.info": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 404}, **{"response.__enter__": None, "response.__exit__": None, "logger.warning": None}))
case("request_brokered_suspend", url="https://x", opener={"fn": 1},
     io=req_io(opener={"status": 200}, **{"response.__enter__": None, "response.__exit__": None, "logger.info": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": "200"}, **{"response.__enter__": None, "response.__exit__": None, "logger.info": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": "bad"}, **{"response.__enter__": None, "response.__exit__": None, "logger.warning": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": None}, **{"response.__enter__": None, "response.__exit__": None, "logger.warning": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={}, **{"response.__enter__": None, "response.__exit__": None, "logger.warning": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen=raise_("HTTPError", "HTTP Error 500: boom", code=500, reason="boom"),
               **{"logger.warning": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200}, **{"response.__enter__": None,
               "response.__exit__": raise_("OSError", "close fail"), "logger.warning": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": "bad"}, **{"response.__enter__": None, "response.__exit__": True}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200}, **{"response.__enter__": raise_("OSError", "enter fail"), "logger.warning": None}))
case("request_brokered_suspend", url="https://x", io=req_io(urlopen=None))
case("request_brokered_suspend", url="https://x",
     io={"urllib.request.Request": raise_("ValueError", "bad url")})
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200}, **{"response.__enter__": None, "response.__exit__": None,
               "logger.info": raise_("RuntimeError", "log fail")}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200}, **{"response.__enter__": None,
               "response.__exit__": raise_("SystemExit", "bye")}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": 200.5}, **{"response.__enter__": None, "response.__exit__": None, "logger.info": None}))
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen={"status": {}}, **{"response.__enter__": None, "response.__exit__": None}))
case("request_brokered_suspend", url="https://x", io=REQ)  # fail-closed urlopen
case("request_brokered_suspend", url="https://x",
     io=req_io(urlopen=raise_("URLError", "<urlopen error refused>"), **{"logger.warning": None}))
case("request_brokered_suspend", url="https://x", timeout=5,
     io=req_io(urlopen={"status": 204}, **{"response.__enter__": None, "response.__exit__": None, "logger.info": None}))

# ── suspend_self ──
SOCK_OK = {
    "socket.socket": {}, "sock.__enter__": None, "sock.settimeout": None,
    "sock.connect": None, "sock.sendall": None, "sock.__exit__": None,
}


def sock_io(recv, **extra):
    d = dict(SOCK_OK)
    d["sock.recv"] = recv
    d.update(extra)
    return d


case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"logger.info": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 500 Internal Server Error\r\n\r\nbody here", ""), **{"logger.warning": None}))
case("suspend_self", environ={"FLY_APP_NAME": "", "FLY_MACHINE_ID": "m"},
     io={"logger.warning": None})
case("suspend_self", environ={"FLY_APP_NAME": "app"}, io={"logger.warning": None})
case("suspend_self", environ=None,
     io=dict(sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"logger.info": None}),
             **{"os.environ": dict(FLY)}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"sock.connect": raise_("OSError", "conn refused"), "logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=dict(SOCK_OK, **{"socket.socket": raise_("OSError", "no socket"), "logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", {"x": 1})))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"sock.__exit__": raise_("OSError", "close fail"), "logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("", ""), **{"sock.settimeout": raise_("OSError", "timeout fail"), "sock.__exit__": True}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK", ""), **{"logger.info": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("garbage", ""), **{"logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 2xx OK\r\n\r\n", ""), **{"logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=dict(SOCK_OK, **{"sock.__enter__": raise_("OSError", "enter fail"), "logger.warning": None}))
case("suspend_self", environ=dict(FLY), io=dict(SOCK_OK, **{"socket.socket": None}))
case("suspend_self", environ=dict(FLY), io=SOCK_OK)  # fail-closed sock.recv
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 500 X\r\n\r\n" + "A" * 600, ""), **{"logger.warning": None}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"sock.connect": raise_("SystemExit", "bye")}))
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"sock.recv": raise_("OSError", "recv fail"), "sock.__exit__": True, "logger.warning": None}))
case("suspend_self", environ=dict(FLY), socket_path="/custom.sock", timeout=3,
     io=sock_io(seq("HTTP/1.1 200 OK\r\n\r\n", ""), **{"logger.info": None}))
case("suspend_self", environ={"FLY_APP_NAME": "app", "FLY_MACHINE_ID": "m"},
     io={"logger.warning": raise_("RuntimeError", "log fail")})
case("suspend_self", environ=dict(FLY),
     io=sock_io(seq("HTTP/1.1 500 X\r\n\r\na\x7f\x01b\"c\\d\te", ""), **{"logger.warning": None}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"
    assert len(counts) == 6, "expected 6 ported funcs"


if __name__ == "__main__":
    main()
