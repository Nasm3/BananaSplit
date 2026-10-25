# Splitter

Splitter is BananaSplit's own set of virtual audio cables, packaged
separately from BananaSplit. It replaces VB-Audio's cables for the input
strips.

There are six cables, one for each input strip, and their numbers are the
strips' numbers:

| Cable | Apps play into (playback device) | BananaSplit records (recording device) |
| --- | --- | --- |
| 1 | `Splitter 1` | `Splitter 1 Output` → strip 1 |
| 2 | `Splitter 2` | `Splitter 2 Output` → strip 2 |
| 3 | `Splitter 3` | `Splitter 3 Output` → strip 3 |
| 4 | `Splitter 4` | `Splitter 4 Output` → strip 4 |
| 5 | `Splitter 5` | `Splitter 5 Output` → strip 5 |
| 6 | `Splitter 6` | `Splitter 6 Output` → strip 6 |

Windows shows each of them with the device name after it, for example
`Splitter 3 (Splitter Virtual Cables)`.

Status: **Windows only, version 0.1.0, test-signed.** Linux already gets its
cables from PipeWire (BananaSplit makes them itself), and macOS isn't
supported yet.

## Using it with BananaSplit

BananaSplit for Windows finds the cables by their numbers. Strip N uses
`Splitter N Output` whenever its virtual cable is switched on (the default),
and its channel menu says `Splitter N` so you know which device to pick in
the app. Without Splitter, BananaSplit falls back to VB-Audio cables, as
before.

The buses (B1, B2) still need VB-Audio cables to appear as microphones in
other apps. Splitter's six cables all go into the strips.

BananaSplit never follows the Windows default device into a Splitter cable.
If the default output is `Splitter 1`, A1 still plays on the real speakers,
because playing into the cable that strip 1 records would feed back.

## How a cable works

`windows/driver` is a WaveRT audio driver (PortCls and KMDF). It is a fork
of Microsoft's
[SimpleAudioSample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/simpleaudiosample),
which is under the MS-PL licence. It has twelve endpoints: a render and a
capture endpoint for each cable.

- **Data path:** the render stream of `Splitter N` converts what it plays
  to 32-bit float and pushes it into ring N. The capture stream of
  `Splitter N Output` pulls it out again, so the two halves of a cable may
  use different sample formats. Each ring holds 0.68 s.
  (`Source/Utilities/cablering.*`)
- **Clock:** both halves are clocked by the performance counter at exactly
  48 kHz, so they never drift apart. The reader keeps a 20 ms margin behind
  the writer, which covers the two streams' timer jitter.
  - The cable's latency is about 30 ms.
  - If nothing plays into a cable, it records silence.
  - If the recorder falls far behind, it skips ahead rather than playing
    old sound late.

  These rules are plain arithmetic in `Source/Utilities/cablerule.h`, and
  `tests/cablerule_test.cpp` tests them on any computer.
- **Formats:** 48 kHz stereo, in 16-bit, 24-bit (the default) or 32-bit
  float. NaN and infinity become silence.
- **Copy-protected content:** it plays as silence. The driver doesn't claim
  DRM support.
- **Names:** the INF writes each endpoint's name under the name GUID of its
  topology connector pin (`MediaCategories`). Those GUIDs are in
  `Source/Inc/endpoints.h`.

From the sample, Splitter removed the sine-tone generator, the dumping of
playback to `.wav` files, and the mic-array topology.

## Building

Run `pwsh -File splitter/windows/build.ps1` on Windows. It needs:

- Visual Studio 2022 or later, with the C++ desktop workload and the
  Spectre-mitigated libraries;
- `nuget`.

The Windows Driver Kit comes from NuGet (`driver/packages.config`), so you
don't need to install it.

The script does the following:

1. builds the driver and checks the INF with InfVerif;
2. makes the catalog;
3. signs the driver and the catalog with a new self-signed test
   certificate;
4. puts the result in `windows/dist/splitter-<version>-windows-x64.zip`,
   together with the installer and the licences.

CI builds that package on every push (job "Splitter (Windows driver)"). The
release workflow builds it for a `splitter-v*` tag.

The version is in `splitter/VERSION`. It must also be set in `Splitter.inx`,
`Splitter.rc` and `Directory.Build.props`, and the build checks that all
three match.

## Installing

Unzip the package and run `install.cmd`. Running `uninstall.cmd` removes
everything again. The package's `README.txt` explains both.

## Signing: why this build only runs in test mode

Windows 10 and 11 load a kernel driver only if Microsoft has signed it.

- **Test build:** the package that CI makes is signed with a test
  certificate. It loads only on a PC in test-signing mode (`bcdedit /set
  testsigning on`, then a restart; Secure Boot must be off).
  - `install.cmd` checks for that and explains what to do.
  - Use this build for development and trying it out, not for everyone's
    PC.
- **Public build:** a release that installs on any PC needs Microsoft's
  attestation signature. To get it, the project registers in the Partner
  Center Hardware Developer Program, which needs an EV code-signing
  certificate. From then on, each release goes as follows:
  1. the driver goes up as a signed CAB;
  2. Microsoft signs it;
  3. the signed `splitter.sys` and `splitter.cat` replace the ones in the
     package;
  4. `splitter-test.cer` is left out.

  Nothing in the code or the INF has to change for this.

## Not tested yet

CI can build the driver, check its INF and test the ring rules. It can't load
a test-signed driver, because GitHub's Windows runners can't restart into
test mode. Before a release, check these on a real PC in test mode:

- all twelve endpoints appear with their names;
- audio passes through each cable in all three formats;
- the latency;
- running for a long time without glitches;
- install, update and uninstall;
- Driver Verifier.

## Licence

The licence is MIT (`LICENSE`). Files derived from Microsoft's sample keep
Microsoft's copyright notice and are also under the Microsoft Public License
(`windows/driver/LICENSE-MS-PL.txt`).
