# Evidence sweep — 2026-11-27

What the 1,230 gates the evidence ratchet admits ("PORT, not input-independent, not
tuple-shaped, not stub-backed, twin asks a single positional question") actually
proved when each one was executed against live Python.

## Result

| verdict | gates | meaning |
|---|---:|---|
| MATCH | 8 | every cup matched live Python |
| NON-DISC | 558 | one cup for every input — proves nothing |
| MISMATCH | 333 | at least one cup differed: a real divergence |
| TWIN-BLIND | 160 | C varies, twin constant (or the reverse) |
| ERROR | 159 | the gate could not run at all |
| UNKNOWN / TIMEOUT | 3 | unparsed output / exceeded 90 s |

Measured, not bounded: **8 of 1,230** candidates are working parity evidence (0.65%).
The ratchet's 1,198 is an upper bound on *eligible* gates, and eligibility is not proof.

Earlier hand-fixed gates in this same class are green individually and are not counted
here (they were re-run one at a time after their fixes): `can_change_plan`,
`check_codex_binary_ok`, the session-yolo registry, the permanent allowlist, the
shell-hook allowlist, `_path_is_public`, `is_anthropic_bedrock_model`,
`_model_name_suggests_kimi`, `_is_custom_endpoint`, `is_local_endpoint`.

## Why the 159 errors are not one bug

| cause | gates |
|---|---:|
| harness/prototype mismatch (`too few arguments to function 'ap_translate_remote'; expected 3, have 0`) | 109 |
| C access violation (exit 0xC0000005) | 15 |
| `SystemExit(N)` raised by the oracle | 14 |
| other (parser/config side effects) | 11 |
| missing `twin.py` / path not found / missing python dependency | 11 |

The 109 are the mechanical class: the generated harness declares One argument
(or None) while the C implementation takes out-parameters or several inputs
(`void fn(const char *in, char *out, size_t n)`; `cli_display_response(const char *,
size_t, const char *)`). The symbol is present in the object archive in every case —
the harness simply cannot call it. *The failure is in the repo's own harness
generator* (`fix_all_gates.categorize_signature`/`generate_port_harness`): it mis-
classifies real prototypes into a no-arg category, so the emitted call has the wrong
arity. Reconfirmed by rerunning `fix_all_gates(... only_gates=...)` in targeted mode:
of 121 gates it "updated", a large subset still compiled with `too few arguments to
function ...`, and at least one gate that DID have a working port (`redact_key`) was
downgraded to a GAP sentinel. The pass was reverted; it made the fleet strictly worse.

The 15 access violations are the same class, not latent C bugs: `char *const *segment`
array-of-pointers parameters (e.g. `cron_lifecycle_command_token_index`) get called with
a bare string, which the function dereferences as a pointer array → 0xC0000005. A few
"crash" gates are worse — their twin imports the real Hermes package and boots real
side effects (`gateway_command` started the actual gateway during the sweep). Those are
unsafe differential oracles and must be excluded from the evidence set, not "fixed".

So: neither bucket is a per-function C defect waiting to be found. Both are gate-shape
failures in the tooling (wrong generated prototype) or the oracle design (boots real
I/O). Closing them means fixing `fix_all_gates` to emit the correct harness from the
real prototype — a bounded refactor of that one module — and excluding
side-effect twins from the evidence set.

## What this rules out

- Fixture size. 30 NON-DISC gates given a 40-input adversarial corpus (property-based
  classes: empty, unicode, quotes, JSON fragments, boundaries) stayed NON-DISC 30/30.
- Annotation. Every gate here already carries a `PoP:` line.
- Twinning as a whole. 558 gates produce one cup for every input: the answer genuinely
  does not depend on the fixture, so the gate shape (return value from one string) is
  wrong for them — they need effect observation or a constructed object.

## What it points at

The productive queue is the 333 MISMATCH gates: each is a named behavioural divergence
with a reproducing input. The four fixed this session all had the same shape —
substring where Python anchors (`strstr("anthropic")`, `strstr("kimi")`, `strstr(
"openrouter.ai")`), or raw string where Python normalizes (leading/trailing whitespace
and slashes), or a missing rule entirely (unqualified hostnames are local).
