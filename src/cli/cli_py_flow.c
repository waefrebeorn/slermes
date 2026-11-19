/*
 * cli_py_flow.c -- FLOW cores + leaves of the quarry ROOT cli.py (LIVE authority)
 * that NO existing PoP claims: the output-history configure/suspend/replay
 * cluster, the slash-text leaves (_looks_like_slash_command, _slash_args), the
 * skill-command cache leaves (_ensure_skill_commands, _session_skill_commands),
 * the -q seed gate (_should_seed_interactive), the two input-sentinel __str__
 * leaves (_VoiceInputMessage, _SeededQueryMessage), and the HermesCLI slash /
 * skill / quick-command dispatch cluster (_install_tool_callbacks,
 * _show_security_advisories, finalize_preloaded_skills, _slash_handler,
 * _process_unregistered_slash, _run_quick_command, _run_plugin_slash_command,
 * _queue_skill_message, _run_skill_bundle_command, _queue_loaded_skills,
 * _run_skill_slash_command, _expand_slash_prefix).
 *
 * NOTE on the module path: the task brief names "hermes_cli/cli.py", but the
 * quarry has NO such file -- the CLI facade lives at the quarry ROOT (cli.py),
 * which is what every existing PoP claim ("@ cli.py:<fn>") and every watercup
 * gate (py_module "cli.py") credits. PoP lines here therefore name the REAL
 * defining module, cli.py, per the scanner's defining-module rule.
 *
 * Self-contained C11, libjson-only. Claimed siblings (process_command,
 * _console_print, get_skill_bundles, _get_plugin_cmd_handler_names) and every
 * foreign collaborator (cli_render helpers, prompt_toolkit print/paint, the
 * agent.skill_commands / skill_bundles functions, gateway_plugin_commands,
 * security_advisories, the vault/tools callback setters, subprocess, redact,
 * plugin dispatch/choices, i18n t, ChatConsole, cprint, COMMANDS,
 * unknown_command_lines, the module logger) enter through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; cpy_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (cpy_ensure_skill_commands inside
 * cpy_session_skill_commands, cpy_queue_skill_message inside
 * cpy_queue_loaded_skills inside the bundle/skill runners, the quick/plugin/
 * bundle/skill/expand runners inside cpy_process_unregistered_slash); the
 * oracle keeps the extracted defs real on the same paths, so their internal
 * seam events appear in BOTH traces.
 *
 * Exception propagation: every cpy flow returns json_t* -- a normal result
 * (NULL == None) or a raise spec. `except Exception` sites (_replay_output_history,
 * _should_seed_interactive, _show_security_advisories, _run_quick_command,
 * _run_plugin_slash_command) swallow every non-BaseException kind;
 * `except subprocess.TimeoutExpired` (_run_quick_command) swallows that kind
 * only; `except gateway_plugin_commands.CatalogUnavailable`
 * (_process_unregistered_slash) swallows that kind only. The BaseException
 * family (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
 * ALWAYS passes through. Fail-closed: a missing/empty seam script yields a
 * RuntimeError raise spec (recorded, then propagated/swallowed exactly as
 * live); no seam installed at all yields the same spec.
 *
 * Modeling decisions (fixture domain, ASCII):
 *  - Module globals (_OUTPUT_HISTORY_ENABLED / _MAX_LINES / _HISTORY /
 *    _REPLAYING / _SUPPRESSED, _skill_commands) travel in the mutated `st`
 *    object and are echoed in out.state; the oracle seeds the extracted
 *    namespace globals from the same object per case.
 *  - _suspend_output_history is a @contextmanager with no body seams: the
 *    harness drives enter -> a synthetic "suspend_body" seam (user code under
 *    the CM, scripted on BOTH sides) -> exit (always, finally semantics);
 *    out.state echoes suppressed_during / suppressed_after.
 *  - _PT_ANSI / Style([]) are pure str-preserving wrappers transcribed INLINE
 *    (no seam): their observable effect is the text reaching pt_print /
 *    paint_formatted_text. _ACCENT is the skin fallback ANSI string
 *    "\x1b[1;38;2;255;215;0m" (skin resolution out of scope), _DIM
 *    "\x1b[2;3m", _RST "\x1b[0m" -- the oracle seeds the same literals.
 *  - _PaintedLine history entries travel as {"__painted__":true,"text":str,
 *    "width":int|null}; the replay width repaint mutates the HISTORY entry
 *    (live _output_history_lines returns the recorded str/_PaintedLine objects
 *    themselves), so the oracle interns painted doubles by text between the
 *    history deque and the output_history_lines / output_tail_fitting seam
 *    results, and C mirrors the width write back onto the matching history
 *    entry. Painted texts are UNIQUE per case (identity-by-text domain).
 *  - The CLI receiver projects to the {"__cli__":true} marker in seam args
 *    (identity is not observable through those seams); the GONE sentinel
 *    projects to {"__gone__":true} (live object() identity becomes marker
 *    equality; the oracle's run/run_skill recorders return the real GONE
 *    object for that script so `is GONE` agrees).
 *  - bind_asker is a CM seam pair bind_asker.enter {asker} / bind_asker.exit
 *    {exc}; the factory call itself is silent (as _setup_navigation_scope in
 *    setup_flow2). An enter raise skips body AND exit; an exit raise replaces
 *    the body exception; exit never suppresses.
 *  - Callback attributes (_sudo_password_callback, ...) and session_id /
 *    config / _pending_input / system_prompt are direct self reads: an ABSENT
 *    key raises AttributeError "'types.SimpleNamespace' object has no
 *    attribute '<key>'" (the oracle self double is a SimpleNamespace); a
 *    PRESENT-null _pending_input raises the METHOD-named NoneType error on
 *    .put. getattr-with-default sites (_preload_skills_finalized,
 *    _preload_skills_thread, _preload_skills_error, _auto_load_skills_result,
 *    _preload_skills_result, session_id in _run_plugin_slash_command) read
 *    absent as the default.
 *  - str()-computed positions carry integral ints only (libjson has no
 *    int/float identity); integral floats normalize to ints on both sides.
 *    str.split()/strip() use the ASCII White_Space set plus \x1c-\x1f;
 *    non-ASCII whitespace is outside the fixture domain.
 *  - subprocess.run results are {"stdout":str,"stderr":str} attribute doubles
 *    (a missing key raises the SimpleNamespace AttributeError, caught by the
 *    live except Exception); TimeoutExpired is the synthetic Exception
 *    subclass named on the subprocess module double.
 *  - quick_commands / skill_commands / skill_bundles / dispatch are objects;
 *    bundle_info['name'] / skill_info['name'] missing keys raise KeyError
 *    (str(KeyError(k)) == repr(k)). auto_result / preload result / stacked
 *    split / bundle results are fixed-arity arrays (unpack ValueErrors carry
 *    the py3.14 "expected N, got M" wording).
 *
 * Seam name table (name -> live call):
 *   coerce_output_history_limit   cli_render._coerce_output_history_limit(value)
 *   output_history_lines          cli_render._output_history_lines()
 *   output_tail_fitting           cli_render._output_tail_fitting(lines,rows,columns,painted)
 *   pt_print                      prompt_toolkit.print_formatted_text(ANSI(text))
 *   paint_formatted_text          prompt_toolkit.renderer.print_formatted_text(output,ANSI,Style([]))
 *   output.get_size               the output double's .get_size()
 *   line_rows                     cli_render._line_rows(line, columns)
 *   set_chrome_floor              cli_render._set_chrome_floor(rows)
 *   add_suspect_rows              cli_render._add_suspect_rows(rows)
 *   painted_columns               cli_render._painted_columns()
 *   suspend_body                  synthetic user code under _suspend_output_history
 *   get_interactive_skill_commands  agent.skill_commands.get_interactive_skill_commands()
 *   gateway_plugin_commands.skills  hermes_cli.gateway_plugin_commands.skills(cli)
 *   stdin.isatty / stdout.isatty  sys.stdin.isatty() / sys.stdout.isatty()
 *   set_sudo_password_callback    tools.terminal_tool setter (cli lazy shim)
 *   set_approval_callback         tools.terminal_tool setter (cli lazy shim)
 *   set_secret_capture_callback   tools.skills_tool setter (cli lazy shim)
 *   set_unlock_prompt_callback    agent.vault_backends.unlock setter
 *   set_save_login_prompt_callback  agent.vault_backends.unlock setter
 *   set_code_prompt_callback      agent.vault_backends.unlock setter
 *   detect_compromised / startup_banner  hermes_cli.security_advisories
 *   print                         builtin print(*args, file=, flush=)
 *   preload_thread.join           self._preload_skills_thread.join(timeout=120)
 *   logger.warning                module logger.warning(fmt, *args)
 *   format_missing_skills         agent.skill_commands.format_missing_skills(missing)
 *   pending_input.put             self._pending_input.put(msg)
 *   t                             agent.i18n.t(key, **kwargs)
 *   ChatConsole.print             cli_render.ChatConsole().print(text)
 *   build_bundle_invocation_message  agent.skill_bundles (cli lazy shim)
 *   build_skill_invocation_message   agent.skill_commands (cli lazy shim)
 *   gateway_plugin_commands.run_skill / lookup / run / report_gone
 *   split_stacked_skill_commands / build_stacked_skill_invocation_message
 *                                 agent.skill_commands
 *   process_command               HermesCLI.process_command [CLAIMED]
 *   console_print                 HermesCLI._console_print [CLAIMED]
 *   get_skill_bundles             cli.get_skill_bundles [CLAIMED]
 *   get_plugin_cmd_handler_names  cli._get_plugin_cmd_handler_names [CLAIMED]
 *   build_subprocess_env          tools.environments.local.build_subprocess_env()
 *   windows_hide_flags            hermes_cli._subprocess_compat.windows_hide_flags()
 *   subprocess.run                subprocess.run(cmd, shell=True, ...)
 *   redact_sensitive_text         agent.redact.redact_sensitive_text(text)
 *   rich_text_from_ansi           cli_render._rich_text_from_ansi(text)
 *   get_plugin_command_handler / resolve_plugin_command_result  hermes_cli.plugins
 *   invoke_plugin_command         hermes_cli.plugin_command_dispatch
 *   clarify_asker                 hermes_cli.plugin_choices.clarify_asker(callback)
 *   bind_asker.enter / bind_asker.exit  hermes_cli.plugin_choices.bind_asker CM
 *   cprint                        cli_render._cprint(text)
 *   COMMANDS                      hermes_cli.commands.COMMANDS (const fetch)
 *   unknown_command_lines         hermes_cli.cli_unknown_command.unknown_command_lines
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include "hermes_json.h"

/* -- universal seam plumbing -- */

typedef json_t *(*cpy_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct cpy_io {
    cpy_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient flags (security_import_ok) */
} cpy_io_t;

static char *cpy_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static json_t *cpy_mk_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool cpy_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cpy_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *cpy_raise_msg(const json_t *r) {
    const json_t *m = r ? json_obj_get(r, "msg") : NULL;
    return (m && m->type == JSON_STRING) ? m->str_val : "";
}

/* One seam call: args ownership transfers to cpy_call (freed here); the
 * result is owned by the caller. No seam installed -> fail-closed spec. */
static json_t *cpy_call(cpy_io_t *io, const char *name, json_t *args) {
    json_t *r = NULL;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        char *m = cpy_fmt("no seam installed for %s", name);
        r = cpy_mk_raise("RuntimeError", m);
        free(m);
    }
    json_free(args);
    return r;
}

/* -- exception hierarchy (FULL builtin table; unknown kinds are synthetic
 * Exception subclasses on the oracle side, so they count as Exception) -- */

static bool cpy_is_exception(const char *kind);

/* BaseException passthrough: the trio (+ asyncio CancelledError) are the ONLY
 * kinds the full hierarchy table walks to BaseException rather than Exception;
 * every other kind (incl. synthetic unknowns) is an Exception descendant. */
static bool cpy_is_base_exc(const char *kind) {
    return !cpy_is_exception(kind);
}

static const char *cpy_exc_parent(const char *kind) {
    /* BaseException-only trio (plus asyncio CancelledError). */
    if (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt") ||
        !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"))
        return "BaseException";
    if (!strcmp(kind, "Exception")) return "BaseException";
    /* Exception direct children. */
    if (!strcmp(kind, "RuntimeError") || !strcmp(kind, "ValueError") ||
        !strcmp(kind, "TypeError") || !strcmp(kind, "AttributeError") ||
        !strcmp(kind, "NameError") || !strcmp(kind, "LookupError") ||
        !strcmp(kind, "ArithmeticError") || !strcmp(kind, "OSError") ||
        !strcmp(kind, "StopIteration") || !strcmp(kind, "StopAsyncIteration") ||
        !strcmp(kind, "AssertionError") || !strcmp(kind, "SyntaxError") ||
        !strcmp(kind, "SystemError") || !strcmp(kind, "ImportError") ||
        !strcmp(kind, "Warning") || !strcmp(kind, "BufferError") ||
        !strcmp(kind, "EOFError") || !strcmp(kind, "ConnectionError") ||
        !strcmp(kind, "UnicodeError") || !strcmp(kind, "ReferenceError"))
        return "Exception";
    /* RuntimeError family. */
    if (!strcmp(kind, "NotImplementedError") || !strcmp(kind, "RecursionError"))
        return "RuntimeError";
    /* NameError family. */
    if (!strcmp(kind, "UnboundLocalError")) return "NameError";
    /* LookupError family. */
    if (!strcmp(kind, "IndexError") || !strcmp(kind, "KeyError"))
        return "LookupError";
    /* ArithmeticError family. */
    if (!strcmp(kind, "ZeroDivisionError") || !strcmp(kind, "OverflowError") ||
        !strcmp(kind, "FloatingPointError"))
        return "ArithmeticError";
    /* OSError family. */
    if (!strcmp(kind, "BlockingIOError") || !strcmp(kind, "ChildProcessError") ||
        !strcmp(kind, "FileExistsError") || !strcmp(kind, "FileNotFoundError") ||
        !strcmp(kind, "InterruptedError") || !strcmp(kind, "IsADirectoryError") ||
        !strcmp(kind, "NotADirectoryError") || !strcmp(kind, "PermissionError") ||
        !strcmp(kind, "ProcessLookupError") || !strcmp(kind, "TimeoutError") ||
        !strcmp(kind, "BrokenPipeError") || !strcmp(kind, "ConnectionAbortedError") ||
        !strcmp(kind, "ConnectionRefusedError") || !strcmp(kind, "ConnectionResetError"))
        return "OSError";
    /* ImportError family. */
    if (!strcmp(kind, "ModuleNotFoundError")) return "ImportError";
    /* SyntaxError family. */
    if (!strcmp(kind, "IndentationError")) return "SyntaxError";
    if (!strcmp(kind, "TabError")) return "IndentationError";
    /* UnicodeError family. */
    if (!strcmp(kind, "UnicodeDecodeError") || !strcmp(kind, "UnicodeEncodeError") ||
        !strcmp(kind, "UnicodeTranslateError"))
        return "UnicodeError";
    /* Warning family. */
    if (!strcmp(kind, "UserWarning") || !strcmp(kind, "DeprecationWarning") ||
        !strcmp(kind, "SyntaxWarning") || !strcmp(kind, "RuntimeWarning") ||
        !strcmp(kind, "FutureWarning") || !strcmp(kind, "ImportWarning") ||
        !strcmp(kind, "UnicodeWarning") || !strcmp(kind, "BytesWarning") ||
        !strcmp(kind, "ResourceWarning") || !strcmp(kind, "EncodingWarning"))
        return "Warning";
    /* Synthetic / unknown kinds: the oracle builds type(kind,(Exception,),{}). */
    return "Exception";
}

static bool cpy_is_exception(const char *kind) {
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int depth = 0; depth < 16; depth++) {
        if (!strcmp(cur, "Exception")) return true;
        if (!strcmp(cur, "BaseException")) return false;
        const char *p = cpy_exc_parent(cur);
        if (!p || !strcmp(p, cur)) return true;
        snprintf(cur, sizeof(cur), "%s", p);
    }
    return true;
}

/* str(exc) for a catch site: KeyError stringifies as repr(key). */
static char *cpy_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { k += (size_t)sprintf(out + k, "\\x%02x", c); }
        else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

static char *cpy_exc_str(const char *kind, const char *msg) {
    if (!strcmp(kind, "KeyError")) return cpy_repr_str(msg);
    char *out = malloc(strlen(msg) + 1);
    if (out) strcpy(out, msg);
    return out;
}

/* -- JSON / Python value helpers -- */

static bool cpy_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static const char *cpy_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) &&
                                  fabs(v->num_val) < 1e16) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static json_t *cpy_attr_raise(const char *carrier, const char *attr) {
    char *m = cpy_fmt("'%s' object has no attribute '%s'", carrier, attr);
    json_t *r = cpy_mk_raise("AttributeError", m);
    free(m);
    return r;
}

static const char *cpy_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool cpy_str_eq(const json_t *v, const char *s) {
    const char *t = cpy_sval(v);
    return t && !strcmp(t, s);
}

/* malloc'd Python str() projection (NULL/absent -> "None"); containers are
 * outside the str()-position domain. */
static char *cpy_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) { char *o = malloc(5); if (o) strcpy(o, "None"); return o; }
    if (v->type == JSON_STRING) { char *o = malloc(strlen(v->str_val) + 1); if (o) strcpy(o, v->str_val); return o; }
    if (v->type == JSON_BOOL) { const char *s = v->bool_val ? "True" : "False"; char *o = malloc(strlen(s) + 1); if (o) strcpy(o, s); return o; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16)
            return cpy_fmt("%.0f", v->num_val);
        return cpy_fmt("%.17g", v->num_val);
    }
    char *o = malloc(8); if (o) strcpy(o, "<obj>"); return o;
}

static bool cpy_has(const json_t *o, const char *k) {
    return o && o->type == JSON_OBJECT && json_obj_get(o, k) != NULL;
}

static json_t *cpy_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static json_t *cpy_copy_or_null(const json_t *v) {
    return (v && v->type != JSON_NULL) ? json_copy(v) : NULL;
}

/* Python str whitespace: ASCII WS plus \x1c-\x1f (non-ASCII WS out of domain). */
static bool cpy_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || c == 0x20 || (c >= 0x1c && c <= 0x1f);
}

static char *cpy_strip(const char *s) {
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && cpy_is_ws((unsigned char)s[a])) a++;
    while (b > a && cpy_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

static char *cpy_lstrip_slash(const char *s) {
    while (*s == '/') s++;
    char *o = malloc(strlen(s) + 1);
    if (o) strcpy(o, s);
    return o;
}

static bool cpy_startswith(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

/* join a string array; non-str element -> the live TypeError. */
static char *cpy_join(const json_t *arr, const char *sep, json_t **err) {
    *err = NULL;
    if (!arr || arr->type != JSON_ARRAY) {
        char *m = cpy_fmt("can only join an iterable (got %s)", cpy_type_name(arr));
        *err = cpy_mk_raise("TypeError", m);
        free(m);
        return NULL;
    }
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t sl = strlen(sep);
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        const char *s = cpy_sval(v);
        if (!s) {
            char *m = cpy_fmt("sequence item %zu: expected str instance, %s found",
                              i, cpy_type_name(v));
            *err = cpy_mk_raise("TypeError", m);
            free(m);
            free(out);
            return NULL;
        }
        size_t vl = strlen(s);
        size_t need = len + vl + (i ? sl : 0) + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(out); return NULL; }
            out = nb;
        }
        if (i) { memcpy(out + len, sep, sl); len += sl; }
        memcpy(out + len, s, vl);
        len += vl;
        out[len] = '\0';
    }
    return out;
}

/* len() with the live TypeError for non-sized values. */
static size_t cpy_len(const json_t *v, json_t **err) {
    *err = NULL;
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT)) return v->c.count;
    if (v && v->type == JSON_STRING) return strlen(v->str_val);
    char *m = cpy_fmt("object of type '%s' has no len()", cpy_type_name(v));
    *err = cpy_mk_raise("TypeError", m);
    free(m);
    return 0;
}

/* fixed-arity unpack of a JSON array (py3.14 ValueError wording; a
 * non-iterable is the live TypeError). */
static json_t *cpy_unpack(const json_t *arr, size_t n, json_t **out) {
    if (!arr || arr->type != JSON_ARRAY) {
        char *m = cpy_fmt("cannot unpack non-iterable %s object", cpy_type_name(arr));
        json_t *r = cpy_mk_raise("TypeError", m);
        free(m);
        return r;
    }
    size_t got = arr->c.count;
    if (got != n) {
        char *m = cpy_fmt("%s values to unpack (expected %zu, got %zu)",
                          got > n ? "too many" : "not enough", n, got);
        json_t *r = cpy_mk_raise("ValueError", m);
        free(m);
        return r;
    }
    for (size_t i = 0; i < n; i++) out[i] = arr->c.items[i];
    return NULL;
}

static int cpy_cmp_strp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* new sorted array of the string elements (str domain). */
static json_t *cpy_sorted_strings(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = arr->c.count;
    if (!n) return out;
    const char **v = calloc(n, sizeof(char *));
    if (!v) return out;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = cpy_sval(arr->c.items[i]);
        if (s) v[k++] = s;
    }
    qsort(v, k, sizeof(char *), cpy_cmp_strp);
    for (size_t i = 0; i < k; i++) json_append(out, json_string(v[i]));
    free(v);
    return out;
}

static bool cpy_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cpy_str_eq(arr->c.items[i], s)) return true;
    return false;
}

/* deep JSON equality (GONE marker comparison). */
static bool cpy_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!cpy_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !cpy_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}

static bool cpy_is_gone(const json_t *v) {
    if (!v || v->type != JSON_OBJECT || v->c.count != 1) return false;
    const json_t *g = json_obj_get(v, "__gone__");
    return g && g->type == JSON_BOOL && g->bool_val;
}

static json_t *cpy_cli_marker(void) {
    json_t *m = json_object();
    json_set(m, "__cli__", json_bool(true));
    return m;
}

/* self attribute reads: direct (raise on absent) vs getattr-with-default. */
static const json_t *cpy_self_read(const json_t *st, const char *key, json_t **err) {
    *err = NULL;
    const json_t *v = cpy_oget(st, key);
    if (!v) *err = cpy_attr_raise("types.SimpleNamespace", key);
    return v;
}

static const json_t *cpy_self_getdef(const json_t *st, const char *key, const json_t *def) {
    const json_t *v = cpy_oget(st, key);
    return v ? v : def;
}

/* first whitespace-delimited word; returns malloc'd word or NULL when the
 * string has no words (live IndexError site). *rest receives the remainder
 * AFTER the word (raw, may be empty/whitespace). */
static char *cpy_first_word(const char *s, const char **rest) {
    size_t i = 0;
    while (s[i] && cpy_is_ws((unsigned char)s[i])) i++;
    if (!s[i]) return NULL;
    size_t a = i;
    while (s[i] && !cpy_is_ws((unsigned char)s[i])) i++;
    char *w = malloc(i - a + 1);
    if (!w) return NULL;
    memcpy(w, s + a, i - a);
    w[i - a] = '\0';
    if (rest) *rest = s + i;
    return w;
}

/* painted-line projection: {"__painted__":true,"text":..,"width":..} -> text */
static const char *cpy_line_text(const json_t *line) {
    if (!line) return NULL;
    if (line->type == JSON_STRING) return line->str_val;
    if (line->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(line, "text");
        return cpy_sval(t);
    }
    return NULL;
}

static bool cpy_is_painted(const json_t *line) {
    if (!line || line->type != JSON_OBJECT) return false;
    const json_t *p = json_obj_get(line, "__painted__");
    return p && p->type == JSON_BOOL && p->bool_val;
}

/* obj['name'] on a non-object: the live py3.14 TypeError wording. */
static json_t *cpy_subscript_str_raise(const json_t *v) {
    const char *tn = cpy_type_name(v);
    char *m = NULL;
    if (v && v->type == JSON_STRING)
        m = cpy_fmt("string indices must be integers, not 'str'");
    else if (v && v->type == JSON_ARRAY)
        m = cpy_fmt("list indices must be integers or slices, not str");
    else
        m = cpy_fmt("'%s' object is not subscriptable", tn);
    json_t *r = cpy_mk_raise("TypeError", m);
    free(m);
    return r;
}
/* -- output-history cluster (module globals travel in st) -- */

/* PoP: cpy_configure_output_history @ cli.py:_configure_output_history */
json_t *cpy_configure_output_history(cpy_io_t *io, json_t *st,
                                     const json_t *enabled, const json_t *max_lines) {
    /* _OUTPUT_HISTORY_ENABLED = bool(enabled) -- written BEFORE the coerce
     * call, so a coerce raise leaves it updated (live statement order). */
    json_set(st, "enabled", json_bool(cpy_truthy(enabled)));
    json_t *a = json_object();
    json_set(a, "value", max_lines ? json_copy(max_lines) : json_number(200));
    json_t *r = cpy_call(io, "coerce_output_history_limit", a);
    if (cpy_is_raise(r)) return r;
    json_set(st, "max_lines", r ? r : json_number(200));
    json_set(st, "history", json_array());   /* fresh deque(maxlen=...) */
    return NULL;
}

/* PoP: cpy_suspend_output_history_enter @ cli.py:_suspend_output_history */
json_t *cpy_suspend_output_history_enter(json_t *st) {
    const json_t *old = cpy_oget(st, "suppressed");
    bool oldb = cpy_truthy(old);
    json_set(st, "suppressed", json_bool(true));
    return json_bool(oldb);
}

/* contextmanager exit half; single PoP claim lives on the enter half */
json_t *cpy_suspend_output_history_exit(json_t *st, bool old_value) {
    json_set(st, "suppressed", json_bool(old_value));   /* finally: restore */
    return NULL;
}

/* PoP: cpy_replay_output_history @ cli.py:_replay_output_history */
json_t *cpy_replay_output_history(cpy_io_t *io, json_t *st,
                                  const json_t *fit, const json_t *output) {
    if (!cpy_truthy(cpy_oget(st, "enabled")) || !cpy_truthy(cpy_oget(st, "history")))
        return NULL;
    json_set(st, "replaying", json_bool(true));
    json_t *pend = NULL;          /* BaseException in flight */
    json_t *rendered = NULL;      /* owned working list */
    json_t *columns = NULL;       /* borrowed from fit */
    json_t *top = NULL;           /* borrowed from fit; NULL == None */
    bool have_fit = fit && fit->type != JSON_NULL;
    json_t *r = NULL;

    r = cpy_call(io, "output_history_lines", json_object());
    if (cpy_is_raise(r)) {
        if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
        goto fin;
    }
    rendered = r;   /* array domain */

    if (have_fit) {
        json_t *parts[4];
        json_t *ue = cpy_unpack(fit, 4, parts);
        if (ue) {
            if (cpy_is_base_exc(cpy_raise_kind(ue))) pend = ue; else json_free(ue);
            goto fin;
        }
        json_t *rows = parts[0];
        columns = parts[1];
        json_t *painted = parts[2];
        top = parts[3];
        if (top && top->type == JSON_NULL) top = NULL;
        json_t *a = json_object();
        /* None lines drop from the args on BOTH sides (args_drop_null). */
        if (rendered) json_set(a, "lines", json_copy(rendered));
        json_set(a, "rows", json_copy(rows));
        json_set(a, "columns", json_copy(columns));
        json_set(a, "painted", json_copy(painted));
        r = cpy_call(io, "output_tail_fitting", a);
        if (cpy_is_raise(r)) {
            if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
            goto fin;
        }
        json_free(rendered);
        rendered = r;
    }

    if (cpy_truthy(rendered)) {
        /* "\n".join(rendered_lines) -- painted lines join as their text. */
        json_t *texts = json_array();
        for (size_t i = 0; rendered && i < rendered->c.count; i++) {
            const char *t = cpy_line_text(rendered->c.items[i]);
            json_append(texts, json_string(t ? t : ""));
        }
        json_t *je = NULL;
        char *joined = cpy_join(texts, "\n", &je);
        json_free(texts);
        if (je) {
            if (cpy_is_base_exc(cpy_raise_kind(je))) pend = je; else json_free(je);
            goto fin;
        }
        if (!output || output->type == JSON_NULL) {
            json_t *a = json_object();
            json_set(a, "text", json_string(joined));
            r = cpy_call(io, "pt_print", a);
            free(joined);
            if (cpy_is_raise(r)) {
                if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
                goto fin;
            }
            json_free(r);
        } else {
            json_t *a = json_object();
            json_set(a, "output", json_copy(output));
            char *jt = cpy_fmt("%s\n", joined);
            json_set(a, "text", json_string(jt ? jt : "\n"));
            free(jt);
            json_set(a, "style", json_array());
            r = cpy_call(io, "paint_formatted_text", a);
            free(joined);
            if (cpy_is_raise(r)) {
                if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
                goto fin;
            }
            json_free(r);
            r = cpy_call(io, "output.get_size", json_object());
            if (cpy_is_raise(r)) {
                if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
                goto fin;
            }
            json_t *size = r;
            if (top) {
                /* top += sum(_line_rows(line, columns) for line in rendered) */
                double acc = 0.0;
                const char *acc_tn = "int";
                json_t *sum_err = NULL;
                for (size_t i = 0; rendered && i < rendered->c.count; i++) {
                    json_t *la = json_object();
                    const char *t = cpy_line_text(rendered->c.items[i]);
                    json_set(la, "line", json_string(t ? t : ""));
                    json_set(la, "columns", json_copy(columns));
                    json_t *lr = cpy_call(io, "line_rows", la);
                    if (cpy_is_raise(lr)) { sum_err = lr; break; }
                    if (!lr || lr->type != JSON_NUMBER) {
                        char *m = cpy_fmt("unsupported operand type(s) for +: '%s' and '%s'",
                                          acc_tn, cpy_type_name(lr));
                        sum_err = cpy_mk_raise("TypeError", m);
                        free(m);
                        json_free(lr);
                        break;
                    }
                    acc += lr->num_val;
                    acc_tn = "int";   /* int results keep the accumulator int */
                    json_free(lr);
                }
                if (sum_err) {
                    json_free(size);
                    if (cpy_is_base_exc(cpy_raise_kind(sum_err))) pend = sum_err;
                    else json_free(sum_err);
                    goto fin;
                }
                const json_t *srows = cpy_oget(size, "rows");
                if (!srows) {
                    json_free(size);
                    pend = NULL;
                    json_t *ae = cpy_attr_raise("types.SimpleNamespace", "rows");
                    json_free(ae);   /* except Exception: pass */
                    goto fin;
                }
                double tval = top->type == JSON_NUMBER ? top->num_val : 0.0;
                tval += acc;
                double sr = srows->type == JSON_NUMBER ? srows->num_val : 0.0;
                double floor_v = sr - tval;
                if (floor_v < 0.0) floor_v = 0.0;
                json_t *fa = json_object();
                json_set(fa, "rows", json_number(floor_v));
                r = cpy_call(io, "set_chrome_floor", fa);
                if (cpy_is_raise(r)) {
                    json_free(size);
                    if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
                    goto fin;
                }
                json_free(r);
                const json_t *scols = cpy_oget(size, "columns");
                double sc = scols && scols->type == JSON_NUMBER ? scols->num_val : 0.0;
                double cc = columns && columns->type == JSON_NUMBER ? columns->num_val : 0.0;
                if (!scols || sc != cc) {
                    if (!scols) {
                        json_free(size);
                        json_t *ae = cpy_attr_raise("types.SimpleNamespace", "columns");
                        json_free(ae);
                        goto fin;
                    }
                    json_t *sa = json_object();
                    json_set(sa, "rows", json_number(tval + 1.0 - sr));
                    r = cpy_call(io, "add_suspect_rows", sa);
                    if (cpy_is_raise(r)) {
                        json_free(size);
                        if (cpy_is_base_exc(cpy_raise_kind(r))) pend = r; else json_free(r);
                        goto fin;
                    }
                    json_free(r);
                }
            }
            json_free(size);
        }
        /* width = _painted_columns() if fit is None else columns */
        json_t *width = NULL;
        if (!have_fit) {
            width = cpy_call(io, "painted_columns", json_object());
            if (cpy_is_raise(width)) {
                if (cpy_is_base_exc(cpy_raise_kind(width))) pend = width; else json_free(width);
                width = NULL;
                goto fin;
            }
        } else {
            width = columns ? json_copy(columns) : json_null();
        }
        /* for line in rendered: if isinstance(line, _PaintedLine): line.width = width.
         * Live repaints the RECORDED objects (history identity), so mirror the
         * write onto the matching history entry (unique painted texts). */
        json_t *hist = cpy_oget(st, "history");
        for (size_t i = 0; rendered && i < rendered->c.count; i++) {
            json_t *line = rendered->c.items[i];
            if (!cpy_is_painted(line)) continue;
            json_set(line, "width", width ? json_copy(width) : json_null());
            const char *t = cpy_line_text(line);
            if (hist && hist->type == JSON_ARRAY && t) {
                for (size_t j = 0; j < hist->c.count; j++) {
                    json_t *hl = hist->c.items[j];
                    if (cpy_is_painted(hl)) {
                        const char *ht = cpy_line_text(hl);
                        if (ht && !strcmp(ht, t)) {
                            json_set(hl, "width", width ? json_copy(width) : json_null());
                            break;
                        }
                    }
                }
            }
        }
        json_free(width);
    }

fin:
    json_free(rendered);
    json_set(st, "replaying", json_bool(false));   /* finally */
    return pend;
}

/* -- slash-text leaves -- */

/* PoP: cpy_looks_like_slash_command @ cli.py:_looks_like_slash_command */
json_t *cpy_looks_like_slash_command(const json_t *text) {
    if (!cpy_truthy(text)) return json_bool(false);
    const char *s = cpy_sval(text);
    if (!s) return cpy_attr_raise(cpy_type_name(text), "startswith");
    if (s[0] != '/') return json_bool(false);
    const char *rest = NULL;
    char *w = cpy_first_word(s, &rest);
    if (!w) return cpy_mk_raise("IndexError", "list index out of range");
    bool ok = strchr(w + 1, '/') == NULL;
    free(w);
    return json_bool(ok);
}

/* PoP: cpy_slash_args @ cli.py:_slash_args */
json_t *cpy_slash_args(const json_t *cmd) {
    const char *s = cpy_sval(cmd);
    if (!s) return cpy_attr_raise(cpy_type_name(cmd), "split");
    const char *rest = NULL;
    char *w = cpy_first_word(s, &rest);
    if (!w) return json_string("");
    free(w);
    /* parts[1] exists only when non-whitespace follows the first word. */
    while (*rest && cpy_is_ws((unsigned char)*rest)) rest++;
    if (!*rest) return json_string("");
    char *t = cpy_strip(rest);
    json_t *out = json_string(t ? t : "");
    free(t);
    return out;
}

/* -- skill-command cache leaves -- */

/* PoP: cpy_ensure_skill_commands @ cli.py:_ensure_skill_commands */
json_t *cpy_ensure_skill_commands(cpy_io_t *io, json_t *st) {
    const json_t *sc = cpy_oget(st, "skill_commands");
    if (sc && sc->type != JSON_NULL) return json_copy(sc);
    /* from agent.skill_commands import get_interactive_skill_commands */
    return cpy_call(io, "get_interactive_skill_commands", json_object());
}

/* PoP: cpy_session_skill_commands @ cli.py:_session_skill_commands */
json_t *cpy_session_skill_commands(cpy_io_t *io, json_t *st) {
    json_t *a = json_object();
    json_set(a, "cli", cpy_cli_marker());
    json_t *found = cpy_call(io, "gateway_plugin_commands.skills", a);
    if (cpy_is_raise(found)) return found;
    if (!found) return cpy_ensure_skill_commands(io, st);   /* found is None */
    return found;
}

/* -- -q seed gate -- */

/* PoP: cpy_should_seed_interactive @ cli.py:_should_seed_interactive */
json_t *cpy_should_seed_interactive(cpy_io_t *io, const json_t *query,
                                    const json_t *image, bool quiet, bool oneshot) {
    if ((!cpy_truthy(query) && !cpy_truthy(image)) || oneshot || quiet)
        return json_bool(false);
    json_t *r = cpy_call(io, "stdin.isatty", json_object());
    if (cpy_is_raise(r)) {
        if (cpy_is_base_exc(cpy_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);   /* except Exception */
    }
    if (!cpy_truthy(r)) { json_free(r); return json_bool(false); }  /* short-circuit */
    json_free(r);
    r = cpy_call(io, "stdout.isatty", json_object());
    if (cpy_is_raise(r)) {
        if (cpy_is_base_exc(cpy_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    bool b = cpy_truthy(r);
    json_free(r);
    return json_bool(b);
}

/* -- input-sentinel __str__ leaves (construction + str modeled together) -- */

/* PoP: cpy_voice_input_message_str @ cli.py:_VoiceInputMessage */
json_t *cpy_voice_input_message_str(const json_t *st) {
    const json_t *text = cpy_oget(st, "text");
    if (!text)
        return cpy_mk_raise("TypeError",
                            "_VoiceInputMessage.__init__() missing 1 required positional argument: 'text'");
    if (text->type != JSON_STRING) {
        char *m = cpy_fmt("__str__ returned non-string (type %s)", cpy_type_name(text));
        json_t *r = cpy_mk_raise("TypeError", m);
        free(m);
        return r;
    }
    return json_copy(text);
}

/* PoP: cpy_seeded_query_message_str @ cli.py:_SeededQueryMessage */
json_t *cpy_seeded_query_message_str(const json_t *st) {
    const json_t *text = cpy_oget(st, "text");
    if (!text)
        return cpy_mk_raise("TypeError",
                            "_SeededQueryMessage.__init__() missing 1 required positional argument: 'text'");
    /* self.text = text or "" -- falsy collapses to the empty string. */
    if (!cpy_truthy(text)) return json_string("");
    if (text->type != JSON_STRING) {
        char *m = cpy_fmt("__str__ returned non-string (type %s)", cpy_type_name(text));
        json_t *r = cpy_mk_raise("TypeError", m);
        free(m);
        return r;
    }
    return json_copy(text);
}
/* -- HermesCLI startup / skill methods (self state travels in st) -- */

/* PoP: cpy_install_tool_callbacks @ cli.py:_install_tool_callbacks */
json_t *cpy_install_tool_callbacks(cpy_io_t *io, json_t *st) {
    const json_t *flag = cpy_oget(st, "_tool_callbacks_installed");
    if (!flag) return cpy_attr_raise("types.SimpleNamespace", "_tool_callbacks_installed");
    if (cpy_truthy(flag)) return NULL;
    static const char *setters[6] = {
        "set_sudo_password_callback", "set_approval_callback",
        "set_secret_capture_callback", "set_unlock_prompt_callback",
        "set_save_login_prompt_callback", "set_code_prompt_callback",
    };
    static const char *attrs[6] = {
        "_sudo_password_callback", "_approval_callback", "_secret_capture_callback",
        "_vault_unlock_callback", "_vault_save_login_callback", "_vault_code_callback",
    };
    for (int i = 0; i < 6; i++) {
        /* the callback attribute is read BEFORE the setter call fires */
        json_t *ae = NULL;
        const json_t *cb = cpy_self_read(st, attrs[i], &ae);
        if (ae) return ae;
        json_t *a = json_object();
        json_set(a, "callback", json_copy(cb));
        json_t *r = cpy_call(io, setters[i], a);
        if (cpy_is_raise(r)) return r;   /* flag stays False */
        json_free(r);
    }
    json_set(st, "_tool_callbacks_installed", json_bool(true));
    return NULL;
}

/* PoP: cpy_show_security_advisories @ cli.py:_show_security_advisories */
json_t *cpy_show_security_advisories(cpy_io_t *io, json_t *st) {
    (void)st;   /* the live body never touches self */
    bool import_ok = true;
    if (io && io->env) {
        const json_t *f = cpy_oget(io->env, "security_import_ok");
        if (f && f->type == JSON_BOOL) import_ok = f->bool_val;
    }
    if (!import_ok) return NULL;   /* ImportError inside the try: swallowed */
    json_t *d = cpy_call(io, "detect_compromised", json_object());
    if (cpy_is_raise(d)) {
        if (cpy_is_base_exc(cpy_raise_kind(d))) return d;
        json_free(d);
        return NULL;
    }
    json_t *a = json_object();
    json_set(a, "advisories", d ? json_copy(d) : json_null());
    json_t *b = cpy_call(io, "startup_banner", a);
    json_free(d);
    if (cpy_is_raise(b)) {
        if (cpy_is_base_exc(cpy_raise_kind(b))) return b;
        json_free(b);
        return NULL;
    }
    if (cpy_truthy(b)) {
        json_t *pa = json_object();
        json_t *args = json_array();
        json_append(args, json_copy(b));
        json_set(pa, "args", args);
        json_set(pa, "file", json_string("stderr"));
        json_set(pa, "flush", json_bool(true));
        json_t *r = cpy_call(io, "print", pa);
        if (cpy_is_raise(r)) {
            json_free(b);
            if (cpy_is_base_exc(cpy_raise_kind(r))) return r;
            json_free(r);
            return NULL;
        }
        json_free(r);
    }
    json_free(b);
    return NULL;
}

/* PoP: cpy_finalize_preloaded_skills @ cli.py:finalize_preloaded_skills */
json_t *cpy_finalize_preloaded_skills(cpy_io_t *io, json_t *st) {
    const json_t *fin = cpy_self_getdef(st, "_preload_skills_finalized", NULL);
    if (cpy_truthy(fin)) return NULL;
    const json_t *thread = cpy_self_getdef(st, "_preload_skills_thread", NULL);
    if (!thread || thread->type == JSON_NULL) {
        json_set(st, "_preload_skills_finalized", json_bool(true));
        return NULL;
    }
    json_t *ja = json_object();
    json_set(ja, "timeout", json_number(120));
    json_t *r = cpy_call(io, "preload_thread.join", ja);
    if (cpy_is_raise(r)) return r;   /* finalized stays False (live order) */
    json_free(r);
    json_set(st, "_preload_skills_finalized", json_bool(true));
    const json_t *err = cpy_self_getdef(st, "_preload_skills_error", NULL);
    if (err && err->type != JSON_NULL) return json_copy(err);   /* raise err */
    const json_t *auto_result = cpy_self_getdef(st, "_auto_load_skills_result", NULL);
    if (cpy_truthy(auto_result)) {
        if (auto_result->type != JSON_ARRAY || auto_result->c.count < 3) {
            return cpy_mk_raise("IndexError", "list index out of range");
        }
        const json_t *amissing = auto_result->c.items[2];
        if (cpy_truthy(amissing)) {
            json_t *je = NULL;
            char *joined = cpy_join(amissing, ", ", &je);
            if (je) return je;
            json_t *la = json_object();
            json_set(la, "fmt", json_string(
                "skills.auto_load: skill(s) not found or disabled, skipped: %s"));
            json_t *largs = json_array();
            json_append(largs, json_string(joined ? joined : ""));
            json_set(la, "args", largs);
            free(joined);
            r = cpy_call(io, "logger.warning", la);
            if (cpy_is_raise(r)) return r;
            json_free(r);
        }
        /* self.preloaded_skills = list(auto_result[1]) */
        const json_t *anames = auto_result->c.items[1];
        json_set(st, "preloaded_skills",
                 (anames && anames->type == JSON_ARRAY) ? json_copy(anames) : json_array());
    } else {
        json_set(st, "preloaded_skills", json_array());
    }
    const json_t *result = cpy_self_getdef(st, "_preload_skills_result", NULL);
    if (!cpy_truthy(result)) return NULL;
    json_t *parts[3];
    json_t *ue = cpy_unpack(result, 3, parts);
    if (ue) return ue;
    const json_t *skills_prompt = parts[0];
    const json_t *loaded = parts[1];
    const json_t *missing = parts[2];
    if (cpy_truthy(missing)) {
        json_t *fa = json_object();
        json_set(fa, "missing", json_copy(missing));
        json_t *fm = cpy_call(io, "format_missing_skills", fa);
        if (cpy_is_raise(fm)) return fm;
        char *fms = cpy_str(fm);
        if (!cpy_truthy(loaded)) {
            json_t *vr = cpy_mk_raise("ValueError", fms ? fms : "");
            free(fms);
            json_free(fm);
            return vr;
        }
        json_t *je = NULL;
        char *lj = cpy_join(loaded, ", ", &je);
        if (je) { free(fms); json_free(fm); return je; }
        json_t *la = json_object();
        json_set(la, "fmt", json_string(
            "Skipping %s. Continuing with: %s. List available skills with `hermes skills list`."));
        json_t *largs = json_array();
        json_append(largs, json_string(fms ? fms : ""));
        json_append(largs, json_string(lj ? lj : ""));
        json_set(la, "args", largs);
        free(fms);
        free(lj);
        json_free(fm);
        r = cpy_call(io, "logger.warning", la);
        if (cpy_is_raise(r)) return r;
        json_free(r);
    }
    if (cpy_truthy(skills_prompt)) {
        json_t *ae = NULL;
        const json_t *sp = cpy_self_read(st, "system_prompt", &ae);
        if (ae) return ae;
        /* "\n\n".join(p for p in (self.system_prompt, skills_prompt) if p).strip() */
        json_t *ps = json_array();
        if (cpy_truthy(sp)) {
            const char *s = cpy_sval(sp);
            json_append(ps, json_string(s ? s : ""));
        }
        if (cpy_truthy(skills_prompt)) {
            const char *s = cpy_sval(skills_prompt);
            json_append(ps, json_string(s ? s : ""));
        }
        json_t *je = NULL;
        char *joined = cpy_join(ps, "\n\n", &je);
        json_free(ps);
        if (je) return je;
        char *trimmed = cpy_strip(joined ? joined : "");
        free(joined);
        json_set(st, "system_prompt", json_string(trimmed ? trimmed : ""));
        free(trimmed);
    }
    /* self.preloaded_skills += [n for n in loaded if n not in preloaded] */
    json_t *pre = cpy_oget(st, "preloaded_skills");
    if (!pre || pre->type != JSON_ARRAY) { pre = json_array(); json_set(st, "preloaded_skills", pre); }
    if (loaded && loaded->type == JSON_ARRAY) {
        for (size_t i = 0; i < loaded->c.count; i++) {
            const json_t *n = loaded->c.items[i];
            bool found = false;
            for (size_t j = 0; j < pre->c.count; j++)
                if (cpy_json_eq(pre->c.items[j], n)) { found = true; break; }
            if (!found) json_append(pre, json_copy(n));
        }
    }
    return NULL;
}

/* PoP: cpy_slash_handler @ cli.py:_slash_handler */
json_t *cpy_slash_handler(const json_t *dispatch, const json_t *handlers,
                          const json_t *canonical) {
    const char *canon = cpy_sval(canonical);
    if (!canon) return cpy_attr_raise(cpy_type_name(canonical), "replace");
    const json_t *entry = cpy_oget(dispatch, canon);
    if (entry) return json_copy(entry);
    /* name = f"_handle_{canonical.replace('-', '_')}_command" */
    size_t n = strlen(canon);
    char *name = malloc(n + 32);
    if (!name) return NULL;
    size_t k = 0;
    memcpy(name, "_handle_", 8); k = 8;
    for (size_t i = 0; i < n; i++) name[k++] = canon[i] == '-' ? '_' : canon[i];
    memcpy(name + k, "_command", 9);
    json_t *out = NULL;
    if (cpy_arr_has_str(handlers, name)) {
        out = json_array();
        json_append(out, json_string(name));
        json_append(out, json_bool(true));
    }
    free(name);
    return out;
}

/* PoP: cpy_queue_skill_message @ cli.py:_queue_skill_message */
json_t *cpy_queue_skill_message(cpy_io_t *io, json_t *st, const json_t *msg) {
    const json_t *pi = cpy_oget(st, "_pending_input");
    if (!pi) return NULL;   /* hasattr False */
    if (pi->type == JSON_NULL)
        return cpy_attr_raise("NoneType", "put");
    json_t *a = json_object();
    json_set(a, "msg", msg ? json_copy(msg) : json_null());
    json_t *r = cpy_call(io, "pending_input.put", a);
    if (cpy_is_raise(r)) return r;
    json_free(r);   /* statement-position call: the method returns None */
    return NULL;
}

/* PoP: cpy_queue_loaded_skills @ cli.py:_queue_loaded_skills */
json_t *cpy_queue_loaded_skills(cpy_io_t *io, json_t *st, const json_t *msg,
                                const json_t *label, const json_t *missing) {
    char *ls = cpy_str(label);
    char *line = cpy_fmt("\n\xe2\x9a\xa1 %s", ls ? ls : "None");
    free(ls);
    json_t *pa = json_object();
    json_t *pargs = json_array();
    json_append(pargs, json_string(line ? line : ""));
    json_set(pa, "args", pargs);
    free(line);
    json_t *r = cpy_call(io, "print", pa);
    if (cpy_is_raise(r)) return r;
    json_free(r);
    if (cpy_truthy(missing)) {
        json_t *je = NULL;
        char *names = cpy_join(missing, ", ", &je);
        if (je) return je;
        json_t *ta = json_object();
        json_set(ta, "key", json_string("cli.skills.skipped_missing"));
        json_t *kw = json_object();
        json_set(kw, "names", json_string(names ? names : ""));
        json_set(ta, "kwargs", kw);
        free(names);
        json_t *t = cpy_call(io, "t", ta);
        if (cpy_is_raise(t)) return t;
        char *ts = cpy_str(t);
        char *text = cpy_fmt("[yellow]%s[/]", ts ? ts : "None");
        free(ts);
        json_free(t);
        json_t *ca = json_object();
        json_set(ca, "text", json_string(text ? text : ""));
        free(text);
        r = cpy_call(io, "ChatConsole.print", ca);
        if (cpy_is_raise(r)) return r;
        json_free(r);
    }
    return cpy_queue_skill_message(io, st, msg);
}

/* PoP: cpy_run_skill_bundle_command @ cli.py:_run_skill_bundle_command */
json_t *cpy_run_skill_bundle_command(cpy_io_t *io, json_t *st, const json_t *base_cmd,
                                     const json_t *bundle_info, const json_t *user_instruction) {
    json_t *ae = NULL;
    const json_t *sid = cpy_self_read(st, "session_id", &ae);
    if (ae) return ae;
    json_t *a = json_object();
    json_set(a, "command", base_cmd ? json_copy(base_cmd) : json_null());
    json_set(a, "instruction", user_instruction ? json_copy(user_instruction) : json_null());
    json_set(a, "task_id", json_copy(sid));
    json_t *br = cpy_call(io, "build_bundle_invocation_message", a);
    if (cpy_is_raise(br)) return br;
    if (!cpy_truthy(br)) {
        json_free(br);
        json_t *ta = json_object();
        json_set(ta, "key", json_string("cli.skills.bundle_load_failed"));
        json_t *kw = json_object();
        json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
        json_set(ta, "kwargs", kw);
        json_t *t = cpy_call(io, "t", ta);
        if (cpy_is_raise(t)) return t;
        char *ts = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts ? ts : "None");
        free(ts);
        json_free(t);
        json_t *ca = json_object();
        json_set(ca, "text", json_string(text ? text : ""));
        free(text);
        json_t *r = cpy_call(io, "ChatConsole.print", ca);
        if (cpy_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    json_t *parts[3];
    json_t *ue = cpy_unpack(br, 3, parts);
    if (ue) { json_free(br); return ue; }
    const json_t *msg = parts[0];
    const json_t *loaded_names = parts[1];
    const json_t *missing = parts[2];
    /* bundle_info['name'] -- KeyError when absent (evaluated before _t) */
    const json_t *name = cpy_oget(bundle_info, "name");
    if (!name) {
        json_free(br);
        if (!bundle_info || bundle_info->type != JSON_OBJECT)
            return cpy_subscript_str_raise(bundle_info);
        return cpy_mk_raise("KeyError", "'name'");   /* str(KeyError(k)) == repr(k) */
    }
    json_t *le = NULL;
    size_t cnt = cpy_len(loaded_names, &le);
    if (le) { json_free(br); return le; }
    json_t *ta = json_object();
    json_set(ta, "key", json_string("cli.skills.loading_bundle"));
    json_t *kw = json_object();
    json_set(kw, "name", json_copy(name));
    char *cs = cpy_fmt("%zu", cnt);
    json_set(kw, "count", json_string(cs ? cs : "0"));
    free(cs);
    json_set(ta, "kwargs", kw);
    json_t *t = cpy_call(io, "t", ta);
    if (cpy_is_raise(t)) { json_free(br); return t; }
    json_t *r = cpy_queue_loaded_skills(io, st, msg, t, missing);
    json_free(t);
    json_free(br);
    return r;
}

/* PoP: cpy_run_skill_slash_command @ cli.py:_run_skill_slash_command */
json_t *cpy_run_skill_slash_command(cpy_io_t *io, json_t *st, const json_t *base_cmd,
                                    const json_t *skill_info, const json_t *rest) {
    json_t *a = json_object();
    json_set(a, "cli", cpy_cli_marker());
    json_set(a, "command", base_cmd ? json_copy(base_cmd) : json_null());
    json_set(a, "rest", rest ? json_copy(rest) : json_null());
    json_t *rs = cpy_call(io, "gateway_plugin_commands.run_skill", a);
    if (cpy_is_raise(rs)) return rs;
    if (cpy_truthy(rs)) { json_free(rs); return NULL; }
    json_free(rs);
    json_t *sa = json_object();
    json_set(sa, "rest", rest ? json_copy(rest) : json_null());
    json_set(sa, "interactive", json_bool(true));
    json_t *sp = cpy_call(io, "split_stacked_skill_commands", sa);
    if (cpy_is_raise(sp)) return sp;
    json_t *parts2[2];
    json_t *ue = cpy_unpack(sp, 2, parts2);
    if (ue) { json_free(sp); return ue; }
    const json_t *extra_keys = parts2[0];
    const json_t *user_instruction = parts2[1];
    if (cpy_truthy(extra_keys)) {
        json_t *cmds = json_array();
        json_append(cmds, base_cmd ? json_copy(base_cmd) : json_null());
        if (extra_keys->type == JSON_ARRAY)
            for (size_t i = 0; i < extra_keys->c.count; i++)
                json_append(cmds, json_copy(extra_keys->c.items[i]));
        json_t *ae = NULL;
        const json_t *sid = cpy_self_read(st, "session_id", &ae);
        if (ae) { json_free(cmds); json_free(sp); return ae; }
        json_t *ba = json_object();
        json_set(ba, "commands", cmds);
        json_set(ba, "instruction", user_instruction ? json_copy(user_instruction) : json_null());
        json_set(ba, "task_id", json_copy(sid));
        json_t *sr = cpy_call(io, "build_stacked_skill_invocation_message", ba);
        if (cpy_is_raise(sr)) { json_free(sp); return sr; }
        if (!cpy_truthy(sr)) {
            json_free(sr);
            json_free(sp);
            json_t *ta = json_object();
            json_set(ta, "key", json_string("cli.skills.stacked_load_failed"));
            json_t *kw = json_object();
            json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
            json_set(ta, "kwargs", kw);
            json_t *t = cpy_call(io, "t", ta);
            if (cpy_is_raise(t)) return t;
            char *ts = cpy_str(t);
            char *text = cpy_fmt("[bold red]%s[/]", ts ? ts : "None");
            free(ts);
            json_free(t);
            json_t *ca = json_object();
            json_set(ca, "text", json_string(text ? text : ""));
            free(text);
            json_t *r = cpy_call(io, "ChatConsole.print", ca);
            if (cpy_is_raise(r)) return r;
            json_free(r);
            return NULL;
        }
        json_t *parts3[3];
        ue = cpy_unpack(sr, 3, parts3);
        if (ue) { json_free(sr); json_free(sp); return ue; }
        const json_t *msg = parts3[0];
        const json_t *loaded_names = parts3[1];
        const json_t *missing = parts3[2];
        json_t *le = NULL;
        size_t cnt = cpy_len(loaded_names, &le);
        if (le) { json_free(sr); json_free(sp); return le; }
        json_t *je = NULL;
        char *names = cpy_join(loaded_names, ", ", &je);
        if (je) { json_free(sr); json_free(sp); return je; }
        json_t *ta = json_object();
        json_set(ta, "key", json_string("cli.skills.loading_stacked"));
        json_t *kw = json_object();
        char *cs = cpy_fmt("%zu", cnt);
        json_set(kw, "count", json_string(cs ? cs : "0"));
        free(cs);
        json_set(kw, "names", json_string(names ? names : ""));
        json_set(ta, "kwargs", kw);
        free(names);
        json_t *t = cpy_call(io, "t", ta);
        if (cpy_is_raise(t)) { json_free(sr); json_free(sp); return t; }
        json_t *r = cpy_queue_loaded_skills(io, st, msg, t, missing);
        json_free(t);
        json_free(sr);
        json_free(sp);
        return r;
    }
    json_t *ae = NULL;
    const json_t *sid = cpy_self_read(st, "session_id", &ae);
    if (ae) { json_free(sp); return ae; }
    json_t *ma = json_object();
    json_set(ma, "command", base_cmd ? json_copy(base_cmd) : json_null());
    json_set(ma, "rest", rest ? json_copy(rest) : json_null());
    json_set(ma, "task_id", json_copy(sid));
    json_t *msg = cpy_call(io, "build_skill_invocation_message", ma);
    json_free(sp);
    if (cpy_is_raise(msg)) return msg;
    if (cpy_truthy(msg)) {
        const json_t *name = cpy_oget(skill_info, "name");
        if (!name) {
            json_free(msg);
            if (!skill_info || skill_info->type != JSON_OBJECT)
                return cpy_subscript_str_raise(skill_info);
            return cpy_mk_raise("KeyError", "'name'");   /* str(KeyError(k)) == repr(k) */
        }
        json_t *ta = json_object();
        json_set(ta, "key", json_string("cli.skills.loading_skill"));
        json_t *kw = json_object();
        json_set(kw, "name", json_copy(name));
        json_set(ta, "kwargs", kw);
        json_t *t = cpy_call(io, "t", ta);
        if (cpy_is_raise(t)) { json_free(msg); return t; }
        json_t *r = cpy_queue_loaded_skills(io, st, msg, t, NULL);
        json_free(t);
        json_free(msg);
        return r;
    }
    json_free(msg);
    json_t *ta = json_object();
    json_set(ta, "key", json_string("cli.skills.skill_load_failed"));
    json_t *kw = json_object();
    json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
    json_set(ta, "kwargs", kw);
    json_t *t = cpy_call(io, "t", ta);
    if (cpy_is_raise(t)) return t;
    char *ts = cpy_str(t);
    char *text = cpy_fmt("[bold red]%s[/]", ts ? ts : "None");
    free(ts);
    json_free(t);
    json_t *ca = json_object();
    json_set(ca, "text", json_string(text ? text : ""));
    free(text);
    json_t *r = cpy_call(io, "ChatConsole.print", ca);
    if (cpy_is_raise(r)) return r;
    json_free(r);
    return NULL;
}
/* -- quick / plugin / unregistered / prefix dispatch cluster -- */

#define CPY_ACCENT "\x1b[1;38;2;255;215;0m"
#define CPY_DIM    "\x1b[2;3m"
#define CPY_RST    "\x1b[0m"

json_t *cpy_expand_slash_prefix(cpy_io_t *io, json_t *st, const json_t *cmd_original,
                                const json_t *cmd_lower, const json_t *skill_commands,
                                const json_t *skill_bundles);

/* _t(key, **kwargs) seam helper: returns the rendered string node (or raise). */
static json_t *cpy_t(cpy_io_t *io, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_set(a, "key", json_string(key));
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return cpy_call(io, "t", a);
}

/* _console_print(text) / _cprint(text) / ChatConsole().print(text) helpers. */
static json_t *cpy_print_text(cpy_io_t *io, const char *seam, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    return cpy_call(io, seam, a);
}

/* PoP: cpy_run_quick_command @ cli.py:_run_quick_command */
json_t *cpy_run_quick_command(cpy_io_t *io, json_t *st, const json_t *base_cmd,
                              const json_t *qcmd, const json_t *user_args) {
    (void)st;
    if (!qcmd || qcmd->type != JSON_OBJECT)
        return cpy_attr_raise(cpy_type_name(qcmd), "get");
    const json_t *qtype = cpy_oget(qcmd, "type");
    if (cpy_str_eq(qtype, "alias")) {
        const json_t *tv = cpy_oget(qcmd, "target");
        const char *ts = cpy_sval(tv);
        if (!ts) {
            if (tv) return cpy_attr_raise(cpy_type_name(tv), "strip");  /* null.strip */
            ts = "";   /* get default */
        }
        char *target = cpy_strip(ts);
        if (target && target[0]) {
            char *full = cpy_startswith(target, "/") ? cpy_fmt("%s", target)
                                                     : cpy_fmt("/%s", target);
            free(target);
            char *ua = cpy_str(user_args);
            char *cmd0 = cpy_fmt("%s %s", full ? full : "", ua ? ua : "None");
            free(full);
            free(ua);
            char *cmd = cpy_strip(cmd0 ? cmd0 : "");
            free(cmd0);
            json_t *a = json_object();
            json_set(a, "command", json_string(cmd ? cmd : ""));
            json_set(a, "redispatch", json_bool(true));
            free(cmd);
            return cpy_call(io, "process_command", a);
        }
        free(target);
        json_t *kw = json_object();
        json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
        json_t *t = cpy_t(io, "cli.quick.no_target", kw);
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "console_print", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    if (!cpy_str_eq(qtype, "exec")) {
        json_t *kw = json_object();
        json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
        json_t *t = cpy_t(io, "cli.quick.unsupported_type", kw);
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "console_print", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    const json_t *ev = cpy_oget(qcmd, "command");
    json_t *exec_node = ev ? json_copy(ev) : json_string("");
    if (!cpy_truthy(exec_node)) {
        json_free(exec_node);
        json_t *kw = json_object();
        json_set(kw, "command", base_cmd ? json_copy(base_cmd) : json_null());
        json_t *t = cpy_t(io, "cli.quick.no_command", kw);
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "console_print", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    /* try region: imports + subprocess.run + output handling */
    json_t *body_raise = NULL;
    json_t *env = cpy_call(io, "build_subprocess_env", json_object());
    if (cpy_is_raise(env)) { body_raise = env; goto handle; }
    json_t *flags = cpy_call(io, "windows_hide_flags", json_object());
    if (cpy_is_raise(flags)) { json_free(env); body_raise = flags; goto handle; }
    json_t *ra = json_object();
    json_set(ra, "command", exec_node);   /* ownership transfers */
    exec_node = NULL;
    json_set(ra, "shell", json_bool(true));
    json_set(ra, "capture_output", json_bool(true));
    json_set(ra, "text", json_bool(true));
    json_set(ra, "encoding", json_string("utf-8"));
    json_set(ra, "errors", json_string("replace"));
    json_set(ra, "timeout", json_number(30));
    json_set(ra, "env", env ? json_copy(env) : json_null());
    json_set(ra, "creationflags", flags ? json_copy(flags) : json_null());
    json_set(ra, "check", json_bool(false));
    json_free(env);
    json_free(flags);
    json_t *result = cpy_call(io, "subprocess.run", ra);
    if (cpy_is_raise(result)) { body_raise = result; goto handle; }
    /* output = result.stdout.strip() or result.stderr.strip() */
    {
        const json_t *so = cpy_oget(result, "stdout");
        if (!so) {
            /* an object result is materialized to a SimpleNamespace double */
            const char *carrier = (result && result->type == JSON_OBJECT)
                                  ? "types.SimpleNamespace" : cpy_type_name(result);
            json_free(result);
            body_raise = cpy_attr_raise(carrier, "stdout");
            goto handle;
        }
        const char *sos = cpy_sval(so);
        if (!sos) {
            json_free(result);
            body_raise = cpy_attr_raise(cpy_type_name(so), "strip");
            goto handle;
        }
        char *out1 = cpy_strip(sos);
        char *output = NULL;
        if (out1 && out1[0]) {
            output = out1;
        } else {
            free(out1);
            const json_t *se = cpy_oget(result, "stderr");
            if (!se) {
                const char *carrier = (result && result->type == JSON_OBJECT)
                                      ? "types.SimpleNamespace" : cpy_type_name(result);
                json_free(result);
                body_raise = cpy_attr_raise(carrier, "stderr");
                goto handle;
            }
            const char *ses = cpy_sval(se);
            if (!ses) {
                json_free(result);
                body_raise = cpy_attr_raise(cpy_type_name(se), "strip");
                goto handle;
            }
            output = cpy_strip(ses);
        }
        json_free(result);
        if (output && output[0]) {
            json_t *da = json_object();
            json_set(da, "text", json_string(output));
            json_t *red = cpy_call(io, "redact_sensitive_text", da);
            if (cpy_is_raise(red)) { free(output); body_raise = red; goto handle; }
            json_t *rta = json_object();
            json_set(rta, "text", red ? json_copy(red) : json_null());
            json_t *rich = cpy_call(io, "rich_text_from_ansi", rta);
            json_free(red);
            if (cpy_is_raise(rich)) { free(output); body_raise = rich; goto handle; }
            json_t *ca = json_object();
            json_set(ca, "text", rich ? json_copy(rich) : json_null());
            json_t *pr = cpy_call(io, "console_print", ca);
            json_free(rich);
            if (cpy_is_raise(pr)) { free(output); body_raise = pr; goto handle; }
            json_free(pr);
        } else {
            json_t *t = cpy_t(io, "cli.quick.no_output", json_object());
            if (cpy_is_raise(t)) { free(output); body_raise = t; goto handle; }
            char *ts2 = cpy_str(t);
            char *text = cpy_fmt("[dim]%s[/]", ts2 ? ts2 : "None");
            free(ts2);
            json_free(t);
            json_t *pr = cpy_print_text(io, "console_print", text);
            free(text);
            if (cpy_is_raise(pr)) { free(output); body_raise = pr; goto handle; }
            json_free(pr);
        }
        free(output);
    }
handle:
    json_free(exec_node);
    if (!body_raise) return json_bool(true);
    if (!strcmp(cpy_raise_kind(body_raise), "TimeoutExpired")) {
        json_free(body_raise);
        json_t *t = cpy_t(io, "cli.quick.timed_out", json_object());
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "console_print", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
        return json_bool(true);
    }
    if (cpy_is_base_exc(cpy_raise_kind(body_raise))) return body_raise;
    {
        char *es = cpy_exc_str(cpy_raise_kind(body_raise), cpy_raise_msg(body_raise));
        json_free(body_raise);
        json_t *kw = json_object();
        json_set(kw, "error", json_string(es ? es : ""));
        free(es);
        json_t *t = cpy_t(io, "cli.quick.error", kw);
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("[bold red]%s[/]", ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "console_print", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
    }
    return json_bool(true);
}

/* PoP: cpy_run_plugin_slash_command @ cli.py:_run_plugin_slash_command */
json_t *cpy_run_plugin_slash_command(cpy_io_t *io, json_t *st, const json_t *base_cmd,
                                     const json_t *user_args) {
    const char *bs = cpy_sval(base_cmd);
    if (!bs) return cpy_attr_raise(cpy_type_name(base_cmd), "lstrip");
    char *bare = cpy_lstrip_slash(bs);
    json_t *a = json_object();
    json_set(a, "cli", cpy_cli_marker());
    json_set(a, "command", json_string(bare));
    json_set(a, "args", user_args ? json_copy(user_args) : json_null());
    json_t *answered = cpy_call(io, "gateway_plugin_commands.run", a);
    if (cpy_is_raise(answered)) { free(bare); return answered; }
    if (cpy_truthy(answered)) { free(bare); return answered; }   /* incl. GONE */
    json_free(answered);
    json_t *ha = json_object();
    json_set(ha, "command", json_string(bare));
    free(bare);
    json_t *handler = cpy_call(io, "get_plugin_command_handler", ha);
    if (cpy_is_raise(handler)) return handler;
    if (!cpy_truthy(handler)) { json_free(handler); return NULL; }
    const json_t *sidv = cpy_self_getdef(st, "session_id", NULL);
    char *sid = cpy_truthy(sidv) ? cpy_str(sidv) : cpy_fmt("%s", "");
    json_t *body_raise = NULL;
    json_t *plugin_result = NULL;
    json_t *ae = NULL;
    const json_t *cb = cpy_self_read(st, "_clarify_callback", &ae);
    if (ae) { body_raise = ae; goto handle; }   /* inside the live try */
    json_t *ca = json_object();
    json_set(ca, "callback", json_copy(cb));
    json_t *asker = cpy_call(io, "clarify_asker", ca);
    if (cpy_is_raise(asker)) { body_raise = asker; goto handle; }
    json_t *ea = json_object();
    json_set(ea, "asker", asker ? json_copy(asker) : json_null());
    json_t *er = cpy_call(io, "bind_asker.enter", ea);
    json_free(asker);
    if (cpy_is_raise(er)) { body_raise = er; goto handle; }   /* no exit on enter raise */
    json_free(er);
    {
        json_t *ia = json_object();
        json_set(ia, "handler", json_copy(handler));
        json_set(ia, "args", user_args ? json_copy(user_args) : json_null());
        json_set(ia, "source", json_null());
        json_set(ia, "session_id", json_string(sid ? sid : ""));
        json_set(ia, "session_key", json_string(sid ? sid : ""));
        json_set(ia, "platform", json_string("cli"));
        json_t *raw = cpy_call(io, "invoke_plugin_command", ia);
        if (cpy_is_raise(raw)) { body_raise = raw; goto cm_exit; }
        json_t *rra = json_object();
        json_set(rra, "result", raw ? json_copy(raw) : json_null());
        plugin_result = cpy_call(io, "resolve_plugin_command_result", rra);
        json_free(raw);
        if (cpy_is_raise(plugin_result)) { body_raise = plugin_result; plugin_result = NULL; goto cm_exit; }
    }
cm_exit:
    {
        json_t *xa = json_object();
        if (body_raise) {
            json_t *exc = json_object();
            json_set(exc, "kind", json_string(cpy_raise_kind(body_raise)));
            char *es = cpy_exc_str(cpy_raise_kind(body_raise), cpy_raise_msg(body_raise));
            json_set(exc, "msg", json_string(es ? es : ""));
            free(es);
            json_set(xa, "exc", exc);
        } else {
            json_set(xa, "exc", json_null());
        }
        json_t *xr = cpy_call(io, "bind_asker.exit", xa);
        if (cpy_is_raise(xr)) { json_free(body_raise); body_raise = xr; }
        else json_free(xr);
    }
    /* `if result: _cprint(str(result))` sits OUTSIDE the with block (live
     * dedent): the CM exit fires FIRST, then the success print. */
    if (!body_raise && cpy_truthy(plugin_result)) {
        char *rs = cpy_str(plugin_result);
        json_t *pr = cpy_print_text(io, "cprint", rs);
        free(rs);
        if (cpy_is_raise(pr)) body_raise = pr;
        else json_free(pr);
    }
    json_free(plugin_result);
handle:
    free(sid);
    json_free(handler);
    if (!body_raise) return NULL;
    if (cpy_is_base_exc(cpy_raise_kind(body_raise))) return body_raise;
    {
        char *es = cpy_exc_str(cpy_raise_kind(body_raise), cpy_raise_msg(body_raise));
        json_free(body_raise);
        json_t *kw = json_object();
        json_set(kw, "error", json_string(es ? es : ""));
        free(es);
        json_t *t = cpy_t(io, "cli.plugin.command_error", kw);
        if (cpy_is_raise(t)) return t;
        char *ts2 = cpy_str(t);
        char *text = cpy_fmt("\x1b[1;31m%s" CPY_RST, ts2 ? ts2 : "None");
        free(ts2);
        json_free(t);
        json_t *r = cpy_print_text(io, "cprint", text);
        free(text);
        if (cpy_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

static void cpy_append_unique(json_t *arr, const char *s) {
    if (!s) return;
    if (cpy_arr_has_str(arr, s)) return;
    json_append(arr, json_string(s));
}

/* PoP: cpy_process_unregistered_slash @ cli.py:_process_unregistered_slash */
json_t *cpy_process_unregistered_slash(cpy_io_t *io, json_t *st,
                                       const json_t *cmd_original, const json_t *cmd_lower) {
    const char *cl = cpy_sval(cmd_lower);
    if (!cl) return cpy_attr_raise(cpy_type_name(cmd_lower), "split");
    char *base_cmd = cpy_first_word(cl, NULL);
    if (!base_cmd) return cpy_mk_raise("IndexError", "list index out of range");
    char *bare = cpy_lstrip_slash(base_cmd);
    json_t *bc_node = json_string(base_cmd);
    json_t *ret = NULL;
    json_t *bundles = NULL;
    json_t *skill_commands = NULL;
    json_t *ua_node = NULL;
    bool gone = false;

    bundles = cpy_call(io, "get_skill_bundles", json_object());
    if (cpy_is_raise(bundles)) { ret = bundles; bundles = NULL; goto done; }

    {
        json_t *ae = NULL;
        const json_t *config = cpy_self_read(st, "config", &ae);
        if (ae) { ret = ae; goto done; }
        if (!config || config->type != JSON_OBJECT) {
            ret = cpy_attr_raise(cpy_type_name(config), "get");
            goto done;
        }
        const json_t *quick = cpy_oget(config, "quick_commands");
        const char *co = cpy_sval(cmd_original);
        if (!co) co = "";
        size_t bl = strlen(base_cmd);
        size_t cn = strlen(co);
        char *ua_raw = cpy_fmt("%s", co + (bl <= cn ? bl : cn));
        char *user_args = cpy_strip(ua_raw ? ua_raw : "");
        free(ua_raw);
        ua_node = json_string(user_args ? user_args : "");
        free(user_args);
        /* if bare in quick_commands */
        if (quick && quick->type != JSON_OBJECT && quick->type != JSON_ARRAY) {
            char *m = cpy_fmt("argument of type '%s' is not a container or iterable",
                              cpy_type_name(quick));
            ret = cpy_mk_raise("TypeError", m);
            free(m);
            goto done;
        }
        bool in_quick = quick ? (quick->type == JSON_OBJECT ? cpy_has(quick, bare)
                                                            : cpy_arr_has_str(quick, bare))
                              : false;
        if (in_quick) {
            const json_t *qcmd = quick->type == JSON_OBJECT ? cpy_oget(quick, bare) : NULL;
            ret = cpy_run_quick_command(io, st, bc_node, qcmd, ua_node);
            goto done;
        }
    }

    {
        json_t *la = json_object();
        json_set(la, "cli", cpy_cli_marker());
        json_set(la, "name", json_string(bare));
        json_t *plugin = cpy_call(io, "gateway_plugin_commands.lookup", la);
        if (cpy_is_raise(plugin)) {
            if (!strcmp(cpy_raise_kind(plugin), "CatalogUnavailable")) {
                json_free(plugin);
                ret = json_bool(true);
                goto done;
            }
            ret = plugin;
            goto done;
        }
        skill_commands = cpy_session_skill_commands(io, st);
        if (cpy_is_raise(skill_commands)) { ret = skill_commands; skill_commands = NULL; goto done; }
        bool run_plugin = false;
        if (cpy_truthy(plugin)) {
            run_plugin = true;
        } else if (!plugin) {   /* plugin is None */
            json_t *names = cpy_call(io, "get_plugin_cmd_handler_names", json_object());
            if (cpy_is_raise(names)) { ret = names; goto done; }
            run_plugin = cpy_arr_has_str(names, bare);
            json_free(names);
        }
        json_free(plugin);
        if (run_plugin) {
            json_t *pr = cpy_run_plugin_slash_command(io, st, bc_node, ua_node);
            if (cpy_is_raise(pr)) { ret = pr; goto done; }
            gone = cpy_is_gone(pr);
            json_free(pr);
            if (!gone) { ret = json_bool(true); goto done; }
            json_free(skill_commands);
            skill_commands = cpy_session_skill_commands(io, st);
            if (cpy_is_raise(skill_commands)) { ret = skill_commands; skill_commands = NULL; goto done; }
        }
    }

    /* bundles win over same-named skills; keys carry the leading slash */
    if (bundles && bundles->type != JSON_OBJECT && bundles->type != JSON_ARRAY) {
        char *m = cpy_fmt("argument of type '%s' is not a container or iterable",
                          cpy_type_name(bundles));
        ret = cpy_mk_raise("TypeError", m);
        free(m);
        goto done;
    }
    if (bundles && (bundles->type == JSON_OBJECT ? cpy_has(bundles, base_cmd)
                                                 : cpy_arr_has_str(bundles, base_cmd))) {
        const json_t *bi = bundles->type == JSON_OBJECT ? cpy_oget(bundles, base_cmd) : NULL;
        ret = cpy_run_skill_bundle_command(io, st, bc_node, bi, ua_node);
        if (cpy_is_raise(ret)) goto done;
        json_free(ret);
        ret = json_bool(true);
        goto done;
    }
    if (skill_commands && skill_commands->type != JSON_OBJECT && skill_commands->type != JSON_ARRAY) {
        char *m = cpy_fmt("argument of type '%s' is not a container or iterable",
                          cpy_type_name(skill_commands));
        ret = cpy_mk_raise("TypeError", m);
        free(m);
        goto done;
    }
    if (skill_commands && (skill_commands->type == JSON_OBJECT ? cpy_has(skill_commands, base_cmd)
                                                               : cpy_arr_has_str(skill_commands, base_cmd))) {
        const json_t *si = skill_commands->type == JSON_OBJECT ? cpy_oget(skill_commands, base_cmd) : NULL;
        ret = cpy_run_skill_slash_command(io, st, bc_node, si, ua_node);
        if (cpy_is_raise(ret)) goto done;
        json_free(ret);
        ret = json_bool(true);
        goto done;
    }
    {
        bool skip_expand = false;
        if (gone) {
            json_t *ra = json_object();
            json_set(ra, "name", json_string(bare));
            json_t *known = json_array();
            json_append(known, skill_commands ? json_copy(skill_commands) : json_null());
            json_append(known, bundles ? json_copy(bundles) : json_null());
            json_set(ra, "known", known);
            json_t *rg = cpy_call(io, "gateway_plugin_commands.report_gone", ra);
            if (cpy_is_raise(rg)) { ret = rg; goto done; }
            skip_expand = cpy_truthy(rg);
            json_free(rg);
        }
        if (!skip_expand) {
            ret = cpy_expand_slash_prefix(io, st, cmd_original, cmd_lower,
                                          skill_commands, bundles);
            goto done;
        }
    }
    ret = json_bool(true);
done:
    free(base_cmd);
    free(bare);
    json_free(bc_node);
    json_free(ua_node);
    json_free(bundles);
    json_free(skill_commands);
    return ret;
}
/* PoP: cpy_expand_slash_prefix @ cli.py:_expand_slash_prefix */
json_t *cpy_expand_slash_prefix(cpy_io_t *io, json_t *st, const json_t *cmd_original,
                                const json_t *cmd_lower, const json_t *skill_commands,
                                const json_t *skill_bundles) {
    (void)st;
    json_t *cmds = cpy_call(io, "COMMANDS", json_object());
    if (cpy_is_raise(cmds)) return cmds;
    const char *cl = cpy_sval(cmd_lower);
    if (!cl) { json_free(cmds); return cpy_attr_raise(cpy_type_name(cmd_lower), "split"); }
    char *typed_base = cpy_first_word(cl, NULL);
    if (!typed_base) {
        json_free(cmds);
        return cpy_mk_raise("IndexError", "list index out of range");
    }
    /* all_known = set(COMMANDS) | set(skill_commands) | set(skill_bundles).
     * Live set iteration is hash-randomized, but every observable (sorted
     * join, min-length, single match) is order-independent; C keeps an
     * ordered dedup (COMMANDS, then skill keys, then bundle keys). */
    json_t *all = json_array();
    if (cmds && cmds->type == JSON_ARRAY)
        for (size_t i = 0; i < cmds->c.count; i++)
            cpy_append_unique(all, cpy_sval(cmds->c.items[i]));
    json_free(cmds);
    if (skill_commands && skill_commands->type == JSON_OBJECT)
        for (size_t i = 0; i < skill_commands->c.count; i++)
            cpy_append_unique(all, skill_commands->c.keys[i]);
    if (skill_bundles && skill_bundles->type == JSON_OBJECT)
        for (size_t i = 0; i < skill_bundles->c.count; i++)
            cpy_append_unique(all, skill_bundles->c.keys[i]);

    json_t *matches = json_array();
    for (size_t i = 0; i < all->c.count; i++) {
        const char *c = cpy_sval(all->c.items[i]);
        if (c && cpy_startswith(c, typed_base)) json_append(matches, json_string(c));
    }
    json_t *ret = NULL;
    size_t mc = matches->c.count;
    if (mc > 1) {
        if (cpy_arr_has_str(matches, typed_base)) {
            json_free(matches);
            matches = json_array();
            json_append(matches, json_string(typed_base));
        } else {
            size_t min_len = SIZE_MAX;
            for (size_t i = 0; i < matches->c.count; i++) {
                size_t l = strlen(cpy_sval(matches->c.items[i]));
                if (l < min_len) min_len = l;
            }
            json_t *shortest = json_array();
            for (size_t i = 0; i < matches->c.count; i++) {
                const char *c = cpy_sval(matches->c.items[i]);
                if (strlen(c) == min_len) json_append(shortest, json_string(c));
            }
            if (shortest->c.count == 1) {
                json_free(matches);
                matches = shortest;
            } else {
                json_free(shortest);
            }
        }
    }
    mc = matches->c.count;
    if (mc == 1 && !cpy_str_eq(matches->c.items[0], typed_base)) {
        const char *m0 = cpy_sval(matches->c.items[0]);
        const char *co = cpy_sval(cmd_original);
        if (!co) co = "";
        char *cos = cpy_strip(co);
        size_t tb = strlen(typed_base);
        size_t cn = strlen(cos ? cos : "");
        char *full = cpy_fmt("%s%s", m0 ? m0 : "", (cos ? cos : "") + (tb <= cn ? tb : cn));
        free(cos);
        json_t *a = json_object();
        json_set(a, "command", json_string(full ? full : ""));
        json_set(a, "redispatch", json_bool(true));
        free(full);
        ret = cpy_call(io, "process_command", a);
        goto done;
    }
    if (mc > 1) {
        json_t *kw = json_object();
        json_set(kw, "command", cmd_lower ? json_copy(cmd_lower) : json_null());
        json_t *t = cpy_t(io, "cli.command.ambiguous", kw);
        if (cpy_is_raise(t)) { ret = t; goto done; }
        char *ts = cpy_str(t);
        char *text = cpy_fmt(CPY_ACCENT "%s" CPY_RST, ts ? ts : "None");
        free(ts);
        json_free(t);
        json_t *r = cpy_print_text(io, "cprint", text);
        free(text);
        if (cpy_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        json_t *sm = cpy_sorted_strings(matches);
        json_t *je = NULL;
        char *cands = cpy_join(sm, ", ", &je);
        json_free(sm);
        if (je) { ret = je; goto done; }
        json_t *kw2 = json_object();
        json_set(kw2, "candidates", json_string(cands ? cands : ""));
        free(cands);
        json_t *t2 = cpy_t(io, "cli.command.did_you_mean_many", kw2);
        if (cpy_is_raise(t2)) { ret = t2; goto done; }
        char *ts2 = cpy_str(t2);
        char *text2 = cpy_fmt(CPY_DIM "%s" CPY_RST, ts2 ? ts2 : "None");
        free(ts2);
        json_free(t2);
        r = cpy_print_text(io, "cprint", text2);
        free(text2);
        if (cpy_is_raise(r)) { ret = r; goto done; }
        json_free(r);
    } else {
        json_t *known = cpy_sorted_strings(all);
        json_t *ua = json_object();
        json_set(ua, "command", cmd_lower ? json_copy(cmd_lower) : json_null());
        json_set(ua, "known", known);
        json_t *ul = cpy_call(io, "unknown_command_lines", ua);
        if (cpy_is_raise(ul)) { ret = ul; goto done; }
        json_t *parts[2];
        json_t *ue = cpy_unpack(ul, 2, parts);
        if (ue) { json_free(ul); ret = ue; goto done; }
        char *lead = cpy_str(parts[0]);
        char *text = cpy_fmt("\x1b[1;31m%s" CPY_RST, lead ? lead : "None");
        free(lead);
        json_t *r = cpy_print_text(io, "cprint", text);
        free(text);
        if (cpy_is_raise(r)) { json_free(ul); ret = r; goto done; }
        json_free(r);
        char *pointer = cpy_str(parts[1]);
        char *text2 = cpy_fmt(CPY_DIM CPY_ACCENT "%s" CPY_RST, pointer ? pointer : "None");
        free(pointer);
        json_free(ul);
        r = cpy_print_text(io, "cprint", text2);
        free(text2);
        if (cpy_is_raise(r)) { ret = r; goto done; }
        json_free(r);
    }
    ret = json_bool(true);
done:
    free(typed_base);
    json_free(all);
    json_free(matches);
    return ret;
}