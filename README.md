# Deskflow authenticated reconnect for Windows

Based on Deskflow **v1.27.0**. A reconnecting Windows server session can release an unresponsive connection after one unanswered heartbeat period when both clients have the same configured screen name and verified certificate fingerprint. This shortens recovery after a stall.

The `upstream/` directory contains the exported source, heartbeat diagnostics, reconnect changes, and regression tests. See `PROVENANCE.json` and `SOURCE-MANIFEST.json`.

New `HB_DIAG` records contain only numeric connection IDs, a local sequence counter, monotonic timestamps, an estimated scheduled deadline, callback delay, submission duration, and heartbeat echo receipt. They do not include peer names, key values, mouse values, clipboard contents, certificates, configuration, or credentials. Normal upstream Info logging is otherwise unchanged; verbose input logging is not enabled.

`expected_due_us` is an estimated schedule deadline, not a separately observed internal timer-expiry event. `submit_return` means the unchanged protocol write returned after queueing bytes; it does **not** mean the network or remote client received them. `last_submitted_seq` does not claim wire-level echo correlation because the protocol carries no heartbeat sequence.

The existing wire protocol and heartbeat period are preserved. Tests cover heartbeat encoding and the identity and liveness conditions for replacement.

Only the manually dispatched `diagnostic-windows.yml` workflow is provided. It uses the standard `windows-2022` GitHub-hosted runner, has a 35-minute timeout, and refuses private-repository runs. There are no scheduled builds, larger/self-hosted runners, Actions artifact uploads, or Actions caches. A successful tested build is published as a diagnostic prerelease with checksums and provenance.

Builds use Release mode, Qt 6.11.3, the runner's preinstalled MSVC/OpenSSL tools, and aqtinstall 3.3.0 from PyPI. A narrow compatibility wrapper selects Qt 6.11's official architecture-specific repository directory; archive and metadata checksum checks are unchanged. This package is unsigned. Do not run it until its source, tests, provenance, and checksum have been reviewed and execution has been authorized.

This repository contains no production-machine logs, settings, keys, certificates, backups, credential helpers, or account billing information.
