# DJFX Looper — portable DSP and native MPC interface

Rare Data's short-loop audio effect: Length, Speed and Loop. The portable engine
also powers DJFX Looper and QuotaFX. This source edition adds an original native
MPC skin with three Q-Link knobs and an on-device quick guide. It does not include the shared
proprietary QuotaFX/DJFX desktop or iOS interface or QuotaScope library.

**Release 1.1.3:** the scope-free Native package passed sound, three-knob control,
steady-display, Help and test-copy save/reload checks on Akai Force 3.10.0.49.
Its own package uninstaller and installer were checked on that device; the
same accepted binary remains installed. Audio CPU: 1.2% p99 / 1.5% maximum of
a 128-frame block at 44.1 kHz. See `docs/hardware-1.1.3.json` for exact hashes.
Other MPC models/firmware and Gen2 are not verified. No firmware, network or
startup changes are included. The OpenMPC catalog submission is separate from
this public GitHub download.

[Download Native 1.1.3](https://github.com/macdigi/djfx-looper/releases/tag/v1.1.3).
The free enhanced-scope edition is available through [Rare Data HQ](https://getquota.app/djfx-looper).

## Audio and controls

- Length: 230–12 ms, with the accepted reversed logarithmic knob mapping.
- Speed: −100 to +100, including reverse and stopped playback.
- Loop: a continuous Q-Link knob; OFF through halfway, ON above halfway.
- FX ON: a separate bypass control with the accepted five-millisecond transition.

Length affects the rolling sound even with Loop OFF. Use FX ON for dry bypass.
The accepted audio kernel is unchanged. Automated comparisons establish equality
to Rare Data's accepted implementation, not universal bit identity to hardware.

## The native interface

PLAY has three matching native Q-Link knobs: Length, Speed and Loop, plus FX ON.
HELP gives a short on-device guide and the Rare Data HQ / product URLs. It
points to DJFX Looper with enhanced scopes, QuotaFX, Quota Stems and RD303.
Website addresses are readable on-device; the Force page does not launch a
browser. This Native source edition intentionally has no audio scope. The
finished enhanced-scope edition remains available through Rare Data HQ.

The scope experiments in 1.1.0–1.1.2 did not pass hardware acceptance: parameter
meters failed to refresh, then direct buffer drawing flashed as the host
repainted. At the owner's request, 1.1.3 removes the renderer entirely. It
opens no display device, maps no display buffers and starts no graphics worker
or timer thread. A binary check guards that boundary. Neither the private
QuotaScope code nor a replacement display hook is included.

The real Rare Data logo is included under its separate branding permission;
Apache-2.0 covers the code, not that logo. Forks can build with an unbranded
skin. Archivo Black retains its SIL OFL terms.

The native identity stays `446a4e73` / `DJFXLooperNative.so`, separate from the
working HQ edition. The fourteen saved parameters and v1 state layout remain;
the 128 legacy display/selector parameters remain reserved for compatibility.
Audio processing allocates no memory. The retired display parameters remain
reserved for saved-project compatibility and produce no display activity.

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
included. The 1.1.3 release ZIP passed the catalog checks and the Force workflow described
above. A build target does not establish other-device hardware acceptance.

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
