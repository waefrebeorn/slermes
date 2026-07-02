# 🏛️ Slermes Vault — Museum of Legacy & Heritage Content

> **"The past is never dead. It's not even past."** — Faulkner

This branch is the **official Slermes museum** — an append-only archive of:

- Legacy build scripts and CI workflows replaced by newer versions
- Old `port_*.c` files that were superseded by native C implementations
- Historical design documents and architectural notes
- Upstream Python reference snapshots at various points in time
- Experimental code that didn't make the cut

## Structure

```
vault/
├── workflows/           # Previous .github/workflows/ versions
│   ├── ci-v500.yml     # CI from the original fork
│   ├── docker-v504.yml # Docker workflow before multi-arch
│   └── ...
├── scripts/            # Retired build/release scripts
├── port-files/         # Superseded port_*.c implementations
│   └── README.md       # What was deprecated and why
├── notes/              # Historical notes, design decisions
└── upstream-snapshots/ # Frozen copies of upstream Python at key commits
```

## Rules

1. **Append-only.** Once something lands in the vault, it never leaves.
2. **Never rebased.** The branch history is linear and immutable.
3. **Tag-linked.** Each release tag on `main` links to the vault state at that time.
4. **ROADMAP references.** The ROADMAP.md on `main` points to vault paths for deprecated features.

## How to Add to the Vault

```bash
git checkout vault
cp /path/to/old-thing vault/category/old-thing
git add vault/category/old-thing
git commit -m "vault: archive old-thing (deprecated in vX.Y.Z because ...)"
git push origin vault
```

**Commitment:** No squashing, no rebasing, no force-pushing to this branch.

---

*🌻 Slermes — We SLERMEd Hermes Agent into C11*
