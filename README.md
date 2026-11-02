# DJFX Looper — portable DSP and native MPC interface

Rare Data's short-loop audio effect: Length, Speed and Loop. The portable engine
also powers DJFX Looper and QuotaFX. This source edition adds an original native
MPC skin and self-contained display analysis. It does not include the shared
proprietary QuotaFX/DJFX desktop or iOS interface or QuotaScope library.

**Development status:** offline core, native display and package checks pass. The
Force 3.10.0.49 headless CPU test passed (3.3% p99, 4.1% worst block). The native
display, listening and project-recall acceptance check is still pending before
a downloadable catalog release.
There is no current OpenMPC catalog listing or hardware-verified binary release
from this repository. Do not confuse host tests with an MPC listening test.

![Offline native skin preview, using synthetic audio; not a Force screenshot](docs/native-interface-preview.png)

Offline asset composition only. Live native display responsiveness still needs the Force check.

## Known hardware failure — native display revision in progress

The first native filmstrip scope prototype failed its Force test: the display
was frozen and visually coarse. Corrective 1.1.1 replaces that path with a
connected in-process renderer; its live GUI acceptance is still pending. The released branded HQ edition works; use that
for normal work. No stable binary/catalog release is published here. The code
and native graphics are being corrected, including the authentic Rare Data logo.

## Audio and controls

- Length: 230–12 ms, with the accepted reversed logarithmic knob mapping.
- Speed: −100 to +100, including reverse and stopped playback.
- Loop: a continuous Q-Link knob; OFF through halfway, ON above halfway.
- FX ON: a separate bypass control with the accepted five-millisecond transition.

Length affects the rolling sound even with Loop OFF. Use FX ON for dry bypass.
The accepted audio kernel is unchanged. Automated comparisons establish equality
to Rare Data's accepted implementation, not universal bit identity to hardware.

## The native interface

PLAY retains three matching native Q-Link knobs. DISPLAY chooses seven original
scope views, ink and Scope ON. Version 1.1.0's experimental filmstrip meters
failed the hardware refresh check; they are not used by 1.1.1.

The 1.1.1 candidate draws connected curves in-process from coherent atomic audio
snapshots. Its renderer verifies four instance-specific panel boundaries before
writing any pixels, supports rotated/triple-buffered Force framebuffers and
never draws without its own markers. It starts only inside the Force MPC
process. It is not an external daemon and installs no startup hook or private
QuotaScope helper. The shared desktop/iOS scope implementation stays private.

The real Rare Data logo is included under its separate branding permission;
Apache-2.0 covers the code, not that logo. Forks can build with an unbranded
skin. Archivo Black retains its SIL OFL terms.

The native identity stays `446a4e73` / `DJFXLooperNative.so`, separate from the
working HQ edition. The fourteen saved parameters and v1 state layout remain;
the 128 legacy display/selector parameters remain reserved for compatibility.
Audio processing allocates no memory and never waits for the display thread.

## Build and verify

Core and native-host checks need CMake 3.22+ and a C++17 compiler:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel 3
ctest --test-dir build/native --output-on-failure
```

To also build an ARMv7 Linux module, draw the skin and check a local catalog ZIP:

```sh
python3 -m pip install 'Pillow>=11,<12'
python3 tools/build_openmpc.py --output build/release
```

This needs Zig 0.16.0, CMake, `rsvg-convert` (librsvg), and Python 3.10+ on PATH.
The host build supports macOS and Linux. `sha256sum` must also be available for
the isolated installer checks. The command downloads the pinned public OpenMPC build tools, verifies the
download's SHA-256 and checks the generated package. An existing matching tools
directory can be passed with `--tools /path/to/mpc-vst-plugins/tools`.
Builds do not connect to or install anything on a device.

The compiler targets ARM32 hard-float with glibc 2.31. That is a build target,
not a claim of support for every MPC model or firmware. Gen2/AArch64 is not
included. Native meters and the actual release ZIP must still pass the catalog's
hardware and CPU workflow before a stable release is published.

## Licensing and provenance

The portable code and this dedicated native interface are under Apache-2.0;
see LICENSE and NOTICE. The included Archivo Black font has its own SIL Open
Font License. External build tools retain their own licenses and attribution.
The license does not grant Rare Data's trademarks or the private QuotaFX app,
interface, scope library, artwork collection or other effect engines.

The engine was written from a mathematical description informed by firmware
analysis, behavioral measurements and hardware listening in the same project.
It is not a formal clean-room reconstruction. This repository contains no vendor
firmware, executable objects, dumps, disassembly, recovered data tables or SDK.
Hardware references identify inspiration; this is not an official Roland or Akai
product and neither company endorses it.

Finished desktop and iPhone/iPad apps: https://getquota.app/djfx-looper
The full 64-effect collection: https://getquota.app/fx

## Reproduce the CPU check

`ports/openmpc/native_bench.c` extends the pinned OpenMPC MIT benchmark with
seven fixed-scope stages. It lowers only its own priority and uses a 40-second
process alarm. It does not open an audio device or control the instrument.
Build it with Zig for ARMv7, then run on a compatible device against the exact
module you intend to release. The report measures audio/analysis, not UI cost.

The [recorded Force result](benchmarks/force-3.10.0.49.json) identifies its module
hash. `tools/build_openmpc.py --bench your-result.json` includes a matching
PASS/WARN report in the local package and rejects a report for another binary.
The recorded CPU result does not establish graphical or audible acceptance.
