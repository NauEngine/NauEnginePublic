# Optional build progress events (v1)

The editor and engine changes may be shipped in either order. There is no new required command-line argument or shared editor/SDK progress type.

The editor sets `NAU_BUILD_EVENTS=1` in the **child process environment only**. A supporting BuildTool writes UTF-8 newline-delimited records to stdout:

```text
NAU_BUILD_EVENT {"v":1,"stage":"copy-resources","completed":1048576,"total":2097152,"unit":"bytes","file":"textures/sky.dds"}
```

`v` is the protocol version. `stage` identifies the current phase; `completed` is a nonnegative integer; `total` is a nonnegative integer or null for unknown work. `unit` is `bytes` or `files`. `file` is an optional display path. These are phase-local counts, not a prediction of remaining time. The counter resets when the stage changes.

Stage IDs: `prepare`, `configure`, `compile-sources`, `compile-assets`, `copy-binaries`, `package-assets`, `write-package`, `copy-resources`, `copy-config`, `finalize`. Compile phases currently have unknown totals. Packaging reports input files separately from the final package payload bytes. Config/resources/binaries report copied payload bytes (files if every file is empty). Event emission is throttled, preserving stage transitions and completion counts.

- Old BuildTool ignores the environment variable; the new editor keeps its busy indicator and ordinary success/error/cancellation handling.
- New BuildTool without opt-in emits no protocol records; existing editor invocations and progress callbacks remain valid.
- New editor rejects unsupported versions/stages, malformed JSON, invalid counts, and overlong records; regular text logs still work.
- Process reads can split records arbitrarily. Readers must buffer incomplete lines and handle CR/LF and a final line without a terminator.
- A phase reaching its total does **not** mean the entire operation succeeded. Success still requires normal process completion with exit code zero. Cancellation continues to terminate the existing Windows Job Object tree.

For engine clients, the existing `buildAssetPackage` entry point remains available. Detailed packaging is exposed by an additive `buildAssetPackageWithProgress` function, without changing `PackBuildOptions` or the existing `IBuildTool` ABI.

Compatibility checks: old tool + event opt-in, new tool + legacy invocation, new tool + opt-in; compare published package bytes. Editor checks: progress before process completion, old-output fallback, unknown/malformed events, a nonzero exit after a completed progress event, and close/Escape/Cancel/destruction during a build. Tests for this task are kept outside the repositories.
