import glob, os, subprocess, tempfile
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe6.exe")
PROBES = [
    ("str_mt",        "print(type(getmetatable('')))", "table"),
    ("str_mt_index",  "print(type(getmetatable('').__index))", "table"),
    ("str_mt_lookup", "print(getmetatable('').__index.upper('abc'))", "ABC"),
    ("str_dot_upper", "local s = 'abc' print(s.upper(s))", "ABC"),
    ("str_colon_up",  "local s = 'abc' print(s:upper())", "ABC"),
    ("str_paren_colon","print(('abc'):upper())", "ABC"),
    ("str_paren_dot", "print(('abc').upper('abc'))", "ABC"),
    ("str_num_colon", "local n = 10 print(n:byte and 1 or 2)", "2"),
    ("tbl_colon_ok",  "local t = {v = 5} function t:get() return self.v end print(t:get())", "5"),
    ("tbl_dot_ok",    "local t = {v = 5} function t.get(self) return self.v end print(t.get(t))", "5"),
    ("num_mt",        "print(type(getmetatable(1)))", "nil"),
    ("str_len_op",    "local s = 'abc' print(#s)", "3"),
    ("str_sub_direct","print(string.sub('hello', 2, 3))", "el"),
    ("str_idx_direct","print(string.upper('abc'))", "ABC"),
    ("str_idx_via_s", "local s = 'abc' local u = string.upper print(u(s))", "ABC"),
]

def build():
    src = os.path.join(REPO, "tests", "slv_probe_main.c")
    lv = os.path.join(REPO, "src", "lv"); lua = os.path.join(REPO, "src", "lua")
    objs = sorted(glob.glob(os.path.join(lv, "slv_*.c")))
    cmd = (["gcc","-O2","-g","-Wall","-Werror=implicit-function-declaration","-I",lua,"-I",lv]
           + objs + [os.path.join(lua,"slv_ui.c"), src, "-o", HARNESS, "-lm"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: print(r.stderr[:1500]); raise SystemExit(2)

def run(code):
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code+"\n"); path=f.name
    try:
        r = subprocess.run([HARNESS, path], capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    finally:
        os.unlink(path)
    out = r.stdout.strip().replace("\r","")
    if r.returncode:
        lines=(r.stderr or out).strip().splitlines()
        return False, lines[0] if lines else f"rc={r.returncode}"
    return True, out

build()
for name, code, want in PROBES:
    good, detail = run(code)
    mark = "ok  " if (good and detail == want) else ("WRONG" if good else "GAP ")
    print(f"{mark} {name:16s} want={want!r:10s} got={detail!r}")
