# RTX 3050 Ti / 4 GB baseline plan (fork)

This is a **measurement plan**, not a claim that any optimization has already been implemented. It belongs to the AiBreadguy/MKW-Recomp-3050Ti fork. Upstream defaults and existing features stay unchanged until measurements justify a specific code change.

## Target system

| Component | Hardware |
|---|---|
| GPU | NVIDIA RTX 3050 Ti Laptop, 4 GB dedicated VRAM |
| CPU | AMD Ryzen 5 5600H |
| RAM | 16 GB system memory |
| OS | Windows 11 |
| Starting point | Current fork baseline; exact commit, game version, and NVIDIA driver to be recorded per run |

## Before each run

1. Record the fork commit SHA, game version, GPU driver version, graphics settings, and whether the laptop is plugged in.
2. Keep Windows power mode, display mode, and the tested course the same across comparisons.
3. Start with 720p internal resolution, stock texture settings, frame generation **off**, and the project's full performance overlay. Record all non-default settings.
4. Let first-run shader/setup work settle before comparing steady-state race frame times; report shader stutter separately.
5. Record at least three comparable runs on the same race segment, plus a short free-roam sample. Do not mistake a lighter scene for a code speedup.
6. For a 60 FPS throughput test, select a 60 FPS target so a 30 FPS cap cannot hide performance. Separately assess whether 30 FPS gameplay maintains normal speed.

## Data to record

- Actual game FPS and frame times (median and p95, plus visible stutters), not just generated/displayed FPS.
- Game-speed indication from the log where available; note if play visibly slows below target.
- Reported CPU and GPU timings and any waits or synchronization spikes.
- Dedicated GPU memory usage against the 4 GB budget, plus system RAM and signs of memory spill.
- Rendering correctness, texture/shadow issues, hangs and crashes.

| Run | Commit | Game version | Driver | Scene | Resolution | FPS/median ms | p95 ms | VRAM peak | Notes |
|---|---|---|---|---|---|---|---|---|---|
| Baseline | TBD | TBD | TBD | Mario Kart Circuit, race | 720p | TBD | TBD | TBD | Frame generation off |
| Test A | TBD | TBD | TBD | Same race segment | 720p | TBD | TBD | TBD | One change only |

## Decision rules

- If dropping resolution greatly improves frame time, inspect GPU-side work and bandwidth first.
- If lowering resolution makes little difference, inspect Vulkan command recording, synchronization, and CPU work.
- If dedicated VRAM approaches 4 GB and stutter coincides with allocations or evictions, investigate texture residency and staging.
- Apply one targeted change per branch/experiment; compare against the same baseline. Revert changes that harm correctness or make frame pacing worse.
- Prefer the project's existing replay, self-test, and performance tracing tools described in AGENTS.md; do not infer speedups from a single run.

## Targets, not promises

First: stable 30 FPS at normal game speed without reduced visual quality relative to the chosen baseline. Stretch: better frame pacing and higher real FPS, eventually testing 60 FPS where feasible.

**Repository hygiene:** Never commit game files, keys, generated game code or shaders, player screenshots, or personal paths from raw logs. Keep licensed upstream attribution intact. Mark unmeasured claims as hypotheses.

## Two experimental code changes (not yet bench-tested on the laptop)

- `MKW_DRAW_CACHE=1`: fast descriptor-key hashing for the existing experimental draw cache. Exact descriptor comparisons still guard every hit. The standalone C test passes; FPS improvement has not been established.
- `MKW_TEX_POOL_BUDGETED=1`: shrink the pool's **preallocation** on 4-GB cards when the driver's exact VRAM budget is smaller than the physical heap. Leaves the default unchanged, leaves explicit `MKW_VK_TEX_POOL_MIN` unchanged. Could reduce paging; could also make texture allocation stutter worse. Requires a controlled A/B test.

Use the same race scene, identical settings, and `MKW_VRAM_LOG=300`. Compare real game frame time, p95 stutters, speed and memory usage. Revert the experiment if it harms correctness.


### Headroom-sensitive follow-up

In the `perf/3050ti-pool-headroom` branch, the optional texture-pool experiment considers **already allocated dedicated VRAM** as well as the driver budget. It preallocates at most half of the *remaining* reported budget. This is an opt-in hypothesis: it may avoid spill/stutters when other applications use VRAM, but fewer preallocated blocks may also make first-load stutter worse. Test it separately against `dev/3050ti-baseline`; do not claim an FPS gain from the calculation alone.


### Fork-specific updater safety

The packaged launcher now checks `AiBreadguy/MKW-Recomp-3050Ti` for releases by default, instead of offering official upstream releases over this experimental build. There are no fork releases yet; a 404 during an update check is silently treated as no offer. Users can explicitly opt into the upstream update channel with `MKW_UPDATE_REPO=adriadam10/mkw-recomp`. The main source's existing upstream URL is still credited in the README.
