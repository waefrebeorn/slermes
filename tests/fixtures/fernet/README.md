# Fernet spec fixtures

`generate.json` and `invalid.json` are copied verbatim from https://github.com/fernet/spec
(master branch), the specification the Python `cryptography.fernet` implementation follows.

- `generate.json` — secret, IV, timestamp, source plaintext and the exact token, so a test can prove
  encryption byte for byte instead of round-tripping through its own decryptor.
- `invalid.json` — eight tokens that must be refused, each with the reason: incorrect MAC, too short,
  invalid base64, payload not a multiple of the block size, bad padding, far-future timestamp,
  expired TTL, and an incorrect IV that only shows up as a padding error.
- `Spec.md` — the format description.

One caveat worth knowing before trusting a diff: these tokens are long enough that some terminals and
log viewers abbreviate them with an ellipsis in the middle (`gAAAAA...DA==`). That is display
truncation, not the file contents — the files hold the full 100-character tokens.
