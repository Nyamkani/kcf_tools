# Topic Plot — Local Echo first slice (Draft, Tools #4)

This implementation records only latest snapshots that the Local Echo UI actually observes at its 100ms polling cadence. It does NOT capture every published source sample.

## Interface
- Select a Local Topic Publisher and click Start Echo. Pick one numeric scalar/fixed-array element to plot.
- Values with unsupported types (including bool and string) are not plotted.
- When sequence advances by more than one, intermediate samples were not observed: render separate points without an interpolating line.
- Export observed CSV on explicit user request only. A bounded buffer retains at most 600 observed snapshots with at most 64 numeric fields per snapshot.
- Display observed count, sequence gap steps, and ring buffer evictions; these are NOT UDP network packet loss statistics.
- Stop Echo retains the existing plot; switching Topic or restarting Echo resets history. Type ID/schema or sequence generation change clears the previous segment.

## CSV columns
`source,type_id,observed_steady_us,observed_elapsed_ms,transport_sequence,unobserved_sequence_steps,<numeric-fields>`

- transport_sequence is the KCF transport publish counter; a payload field named sequence remains separate.
- observed_steady_us uses the observer-side steady_clock. It is not synchronized with remote devices and cannot establish end-to-end transport latency.
- observed_elapsed_ms is relative to the first currently retained record. If the ring evicts older records, CSV relative time restarts from the first retained one.
- source identifies Local Runtime PID/start_ticks and endpoint registration; no Host ID or remote session inference.
- numeric rendering parses values as double; very large integer fields may lose display precision.
- sequence gap steps denote unobserved source sequence positions in this latest-only polling path, not proven network loss.

## Follow-up scope
- Wire remote Subscription's latest DataSnapshot into the same bounded collector with explicit Host/Route/session distinction.
- Multi-Topic/multi-field plot, persistent recorder and worker-thread file writes, exact sample-count semantics, memory/performance caps under prolonged load.
- Actual ARM64/UGV02, remote reconnect, and combined KCF Core tests.

Status: CI and Ubuntu VM Local Plot/CSV smoke validation passed; further remote and ARM64 validation pending. Draft; do not merge as fully implemented Tools #4.

## 2026-11-24/25 Ubuntu VM validation and fixes

- Ubuntu 24.04 VM, Qt 6.9.3: standalone `kcf_tool_real_fixture --element /ux_plot_test` Local Publisher and live Tools GUI Echo/Plot confirmed. The test Publisher runs at approximately 100 Hz, while the 100 ms GUI poll observes about 10 snapshots per second; missed intermediate sequence positions are **not** UDP packet loss.
- The first manual CSV export was inspected for observed-only records, sequence growth, and field values. A conflicting CSV column name was found when the payload also included `sequence`; the transport column is now `transport_sequence`. Post-fix CSV contract test passed; a second manual file export was deliberately skipped.
- Updated Local UI labels distinguish `Local Runtime: None discovered` from independently running Remote discovery. The state was visually checked on the VM while the remote Gateway remained ONLINE.
- Updated 2026-11-25 VM regression: `kcf_tool_topic_plot_history`, `kcf_tool_smoke`, `kcf_tool_real_smoke` **3/3 PASS** (4.05 s), including the renamed CSV header contract and status-label expectations. Earlier Core-linked integration CI on the previous source snapshot: [#45401008075](https://github.com/Nyamkani/kcf_tools/actions/runs/45401008075) PASS; revalidation of every combined workflow on the latest commit is tracked separately.
- Existing OPI5+ v5.6.1 Gateway → new Tools remote discovery, `/ugv02_feedback` Subscribe/payload and observed 49.08 Hz were confirmed as **compatibility regression**, not proof of remote Plot or deployment of the new Gateway.
- Remaining: Remote Plot, high-rate full capture, ARM64 deployment of new components, sustained memory/CPU profiling. PR remains Draft; Tools #4 remains OPEN.
