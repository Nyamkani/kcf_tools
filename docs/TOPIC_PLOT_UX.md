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
`source,type_id,observed_steady_us,observed_elapsed_ms,sequence,unobserved_sequence_steps,<numeric-fields>`

- observed_steady_us uses the observer-side steady_clock. It is not synchronized with remote devices and cannot establish end-to-end transport latency.
- observed_elapsed_ms is relative to the first currently retained record. If the ring evicts older records, CSV relative time restarts from the first retained one.
- source identifies Local Runtime PID/start_ticks and endpoint registration; no Host ID or remote session inference.
- numeric rendering parses values as double; very large integer fields may lose display precision.
- sequence gap steps denote unobserved source sequence positions in this latest-only polling path, not proven network loss.

## Follow-up scope
- Wire remote Subscription's latest DataSnapshot into the same bounded collector with explicit Host/Route/session distinction.
- Multi-Topic/multi-field plot, persistent recorder and worker-thread file writes, exact sample-count semantics, memory/performance caps under prolonged load.
- Actual ARM64/UGV02, remote reconnect, and combined KCF Core tests.

Status: initial code and CI tests pending. Draft; do not merge as fully implemented Tools #4.