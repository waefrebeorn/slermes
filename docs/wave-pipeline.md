# Wave pipeline: where the waiting goes, and how each wait is removed

The porting pipeline used to spend most of its wall clock *waiting*: on the
slowest subagent in a barrier batch, on one interpreter startup per gate, on
regenerating the next wave's plan before dispatching it, and on relinking
harnesses that had not changed since the last run. Each of those was measured
on this box and then removed. This page records the measurements and the tool
that fixes each one, because a "faster" claim that is not re-measured here is
not a claim.

## The four waits

1. **Barrier tail.** Twelve subagent batches were dispatched as one wave and
   the next wave could not start until *every* child returned. A child that
   finished in 12 minutes sat idle behind one that took 40. This is the same
   failure mode reported for parallel subagent runners in the wild: the next
   batch waits for the entire previous batch (Cursor forum, "Parallel
   subagents: next batch waits for entire previous batch to finish"), and it is
   why async-subagent designs keep an orchestrator that refills on completion
   instead of on wave boundaries (LangChain, "Agents Are Going Async").
2. **One interpreter startup per gate.** Measured here: a plain
   `bash -lc true` costs 0.30 s and a fresh Python costs 0.70 s *unloaded*,
   with both climbing to 1.3-1.7 s under a full subagent load. The old gate
   runner spawned a Python per oracle, so a 60-gate wave paid 60 startups
   before any case ran.
3. **Relinking unchanged harnesses.** A gate run is dominated by `gcc + ld`
   per gate, not by the oracle (milliseconds). Every re-verification of the
   same wave relinked all of it.
4. **Generation pause between waves.** Choosing the next 60 modules, naming
   their gates and dealing assignments took a thinking round before any child
   could start.

## The four fixes

| tool | removes | mechanism |
| --- | --- | --- |
| `scripts/wave_queue.py` | barrier tail | one queue spanning waves; `claim N` hands out the next ready batch the moment a slot frees, `done` frees it; `idle` audits the event log for gaps where work was available |
| `scripts/gate_batch.py` | per-gate startup + relink | links in a thread pool, runs harnesses natively, then executes **every oracle in one interpreter** via `runpy`; caches the harness by a hash of sources, objects and flags |
| `scripts/wave_backlog.py` | generation pause | keeps K waves staged ahead of the queue frontier, widening the function band from 1-5 to 6-7 to 8-9 only when a band runs dry |
| `scripts/land_wave.sh` | red commits | build rc 0 and `port object gate: PASS` are hard gates before `git add`; a wave whose build is red cannot be committed |

## Measured on this box

- **30-gate verification** (`docs/wave23-plan.json`, `--jobs 6`):
  `fast_gates.py` 31.1-34.9 s; `gate_batch.py` cold 30.0 s; `gate_batch.py`
  warm (harness reuse) **4.8 s** best, 7.3-8.1 s typical. Same 30 gates, same
  `0 mismatches`. The reuse path is what a re-verification costs; the cold path
  is what a first verification costs.
- **60-gate wave build+verify** (wave 24, 60 new objects): 144 s end to end,
  including `make -j10` of the 60 objects, all 60 gates green, 2088 cases.
- **ccache**: 80.9 % hit rate over 124 126 cacheable calls, so repeated object
  builds are already cache-warm and were not the bottleneck.
- **Spawn cost**: `bash -lc true` 0.30-1.29 s, `python -c pass` 0.70-1.72 s
  depending on load. Anything that can avoid a shell or interpreter spawn per
  unit of work is worth doing.

## How a wave runs now

```
python scripts/wave_backlog.py --ahead 2          # stage waves N+1, N+2
python scripts/wave_queue.py seed --wave N        # queue it (batches of 5)
python scripts/wave_queue.py claim 12             # fill every slot
... children run, each returns ...
python scripts/wave_queue.py done <id>            # free that slot, claim again
python scripts/gate_batch.py --plan docs/waveN-plan.json --jobs 10
bash scripts/land_wave.sh N                       # build+health gate, then commit/push
```

The dispatch loop never waits for a wave to finish: a returned batch frees a
slot and the next `claim` fills it from whichever wave is next in the queue.

## Honesty rules this pipeline enforces

- A `PoP:` comment is provenance, not parity. Only a green gate, a linking
  binary (rc 0) and a clean health census count as progress.
- `land_wave.sh` refuses to commit when the build is red, because "gates green
  + build red" is not a landable wave.
- Counts of ported/annotated modules are never reported as progress.
