# Osprey

**Osprey** stands for **O**blivious **S**peculation for **PR**ogramming m**E**mor**Y**.

It is a memory-management runtime for *memory-oblivious* workloads — programs (typically secure-computation engines such as garbled circuits, secret sharing, and homomorphic encryption) whose memory access pattern must not depend on private inputs.
Such workloads frequently exceed available DRAM, and naïvely paging them to disk is both slow and leaks information through the page-fault sequence.
Osprey solves this by running the target program **twice**:

1. **Speculative pass** — the program runs against a memory-resident overlay while Osprey *traces* its page-access pattern using an eBPF page-fault tracer (`pftracer`) and a userfaultfd watcher.
2. **Programmed pass** — the recorded trace is fed to a planner (the *3PO* algorithm) which produces an oblivious prefetch/eviction schedule. The program is then re-executed against that schedule, so DRAM is *programmed* rather than demand-paged. The observable I/O pattern depends only on the trace, not on private data.

This repository is the umbrella build for Osprey and the third-party engines it has been integrated with, Microsoft SEAL (oblivious fork), EMP-toolkit, — together with end-to-end scripts that exercise CKKS, and garbled circuits under Osprey, MAGE, or a baseline.

## Repository layout

| Path | Contents |
| --- | --- |
| `osprey/` | Core Osprey runtime: tracer, watcher, programmer, BPF page-fault tracer, overlay management. Builds `libosprey.so`. |
| `tools/` | Workloads that wraps each backend (`ckks_utils`, `emp_utils`, `mage`, `example_input`, …). |
| `linux/` | Custom Linux kernel source — used to install UAPI headers and build `libbpf` / `bpftool` against a known version. |
| `mage/` | MAGE — the comparison oblivious-execution engine. |
| `oblivious-SEAL/` | Microsoft SEAL fork with an oblivious-CKKS code path that links against Osprey. |
| `yaml-cpp/`, `emp-tool/` | Third-party libraries vendored as build dependencies. |
| `scripts/` | Top-level driver scripts: `ckks.sh`, `gc.sh`, `mpspdz.sh`, `senate.sh`, `mage.sh`. |
| `Makefile` | Top-level orchestrator that builds every dependency in the right order and stages outputs into `build/` and `install/`. |

After a successful build:

- `install/tools/` holds the user-facing executables (`osprey`, `mage`, `planner`, `ckks_utils`, `mpspdz_utils`, `emp_utils`, `senate_utils`, `example_input`).
- `install/<dep>/` holds the installed headers/libraries of each third-party engine.
- `build/` holds the per-component CMake/Make build trees.

## Installation

Osprey targets Linux (developed on Ubuntu) and requires `clang`/`clang++` (C++20), `cmake`, GNU Make, Boost, and the cgroup tools. eBPF support requires a recent kernel with BTF.

### 1. Install system dependencies

From the repository root:

```sh
make install_deps
```

This target:

1. Builds a small `fastsudo` helper used by some scripts.
2. Installs the apt packages needed by Osprey and its backends:
   `build-essential clang cmake libssl-dev libaio-dev cgroup-tools binutils-dev libreadline-dev llvm libsodium-dev libgmp-dev libboost-program-options-dev libboost-random-dev`.
3. Builds `bpftool` from the bundled `linux/tools/bpf/bpftool` tree.
4. Runs `osprey/install_deps.sh --install-osprey-deps`.

If you prefer to drive these manually, `osprey/install_deps.sh` accepts:

- `--install-osprey-deps` — Osprey’s own apt prerequisites.
- `--install-utils` — `clang-format` and `libbenchmark-dev` for development.
- `--set-vma-limit` — raises `vm.max_map_count` to `1048576` (recommended; Osprey creates many VMAs).

### 2. Build everything

```sh
make all -j$(nproc)
```

`all` builds, in order: `linux-headers`, `yaml-cpp`, `oblivious-SEAL`, `osprey` (`libosprey.so`), `mage`, `emp-tool`, `libOTe`, `MP-SPDZ`, and finally `tools`. Override the parallelism with `JOBS=…` if you need to.

Useful sub-targets:

| Target | What it builds |
| --- | --- |
| `make osprey` | Just `osprey/bin/libosprey.so` and the Osprey driver. |
| `make tools` | The workload drivers in `install/tools/` (depends on every backend). |
| `make mage` / `make MP-SPDZ` / `make emp-tool` / `make libOTe` / `make oblivious-SEAL` / `make yaml-cpp` | A single backend. |
| `make linux-headers` | Stages kernel UAPI headers under `build/linux-headers`. |
| `make clean` | Wipes `build/` and `install/` and runs `clean` in `osprey/`, `mage/`, and `MP-SPDZ/`. |

The Makefile uses `clang++` for the C++ toolchain. Builds are performed out-of-tree under `build/<component>/` and then installed into `install/<component>/`.

## Usage

### The `osprey` driver

`install/tools/osprey` wraps any target program. It controls both passes and emits the trace:

```sh
install/tools/osprey [options] -- <target-program> [target-args...]
```

Key options (see `osprey --help` for the full list):

| Option | Purpose |
| --- | --- |
| `--trace-filebase=PATH` | **Required.** Base path for the recorded access trace. |
| `--speculative-only` / `--programmed-only` | Run only one pass instead of both. |
| `--tracing-algorithm={MICROSET,FIFO}` | Working-set extraction algorithm (default `MICROSET`). |
| `--window-size=N` | Tracer window size (default `2048`). |
| `--programming-algorithm=3PO` | Oblivious schedule generator (default `3PO`). |
| `--mem-limit-low / --mem-limit-high / --mem-limit-max` | 3PO memory budget in KiB. |
| `--batch-size=N` | Prefetch batch size (default `8192`). |
| `--trace-to-stdout` | Print a human-readable trace to stdout. |
| `--cleanup-only` | Remove leftover overlay directories and exit. |
| `--preserve-overlay-directories` | Keep the per-pass overlays for inspection. |

Memory budgets in the workload scripts are enforced with `cgexec -g memory:osprey`, so cgroup v2 with the `memory` controller must be available.

### Workload scripts

Each `scripts/*.sh` is a self-contained launcher that compiles inputs, sets up a memory-limited cgroup, monitors RSS, and runs the workload under `osprey`, `mage`, or `baseline`. They must be executed from the repository root so that `realpath .` resolves to the build’s `install/` directory. They generally need root (for `cgcreate`/`cgset`, `swapon`/`swapoff`, and dropping page caches).

Common flags across the scripts:

| Flag | Meaning |
| --- | --- |
| `--tool=osprey\|mage\|baseline` | Which runtime to drive the workload with. |
| `--tool_args="…"` | Extra flags forwarded to `osprey` (e.g. `--mem-limit-high=…`). |
| `--workload=NAME` | Workload identifier (depends on the script — e.g. CKKS kernel name, MPC circuit, TPC-H query). |
| `--input_size=N` | Problem size; semantics are workload-specific. |
| `--mem_limit=M` | Memory cap in MiB enforced via cgroup. |
| `--thread_num=N` | Worker count where supported. |
| `--compile=true` | Re-run `make clean && make tools` before launching. |

Script-specific options:

- **`scripts/ckks.sh`** — homomorphic CKKS workloads (oblivious-SEAL). Adds `--round_num=N` and `--batch_size=N`.
- **`scripts/gc.sh`** — two-party garbled circuits (EMP-toolkit). Adds `--this_ip=` / `--other_ip=` for the garbler/evaluator hosts.
- **`scripts/mpspdz.sh`** — secret-sharing MPC via MP-SPDZ.
- **`scripts/senate.sh`** — the Senate TPC-H benchmarks.
- **`scripts/mage.sh`** — drives the same workloads under MAGE for comparison; also takes `--this_ip=` / `--other_ip=`.

Per-run logs and an RSS time-series are written under `logs/<YYYYMMDD>/log_<timestamp>/`.

#### Example: CKKS under Osprey

```sh
sudo ./scripts/ckks.sh \
    --tool=osprey \
    --workload=matmul \
    --input_size=4096 \
    --round_num=1 \
    --thread_num=1 \
    --mem_limit=512 \
    --tool_args="--mem-limit-high=393216 --mem-limit-max=524288"
```

#### Example: MP-SPDZ baseline (no Osprey)

```sh
sudo ./scripts/mpspdz.sh \
    --tool=baseline \
    --workload=<circuit> \
    --input_size=<n> \
    --mem_limit=1024
```

#### Example: garbled circuits across two hosts

Run on both the garbler and evaluator (with matching arguments):

```sh
sudo ./scripts/gc.sh \
    --tool=osprey \
    --workload=<circuit> \
    --input_size=<n> \
    --mem_limit=1024 \
    --this_ip=10.0.0.1 --other_ip=10.0.0.2
```

### Running a custom program under Osprey

Any program can be wrapped directly without using a script:

```sh
install/tools/osprey \
    --trace-filebase=/tmp/myrun \
    --mem-limit-high=393216 \
    -- \
    ./my_program arg1 arg2
```

The first invocation produces the trace files under `/tmp/myrun*`; the second pass consumes them and runs the program under the oblivious schedule. Pass `--speculative-only` to stop after tracing, or `--programmed-only` to re-use an existing trace.

## License

Osprey is distributed under the **GNU Lesser General Public License, version 3** — see `osprey/COPYING` and `osprey/COPYING.LESSER`. Vendored third-party components retain their own licenses; consult their respective subdirectories.
