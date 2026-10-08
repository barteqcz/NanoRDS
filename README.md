# NanoRDS — Linux & Windows (GNU Make)

Minimal NanoRDS C source. **The RDS subcarrier is fixed at the standard 57 kHz** in `fm_mpx.c`; the separate stereo pilot remains 19 kHz.

**GNU Make on both platforms; no bundled third-party libraries or CMake needed.** Install the audio and resampling libraries using your OS package manager first.

## Linux (Debian/Ubuntu)

```sh
sudo apt update
sudo apt install build-essential pkg-config libsamplerate0-dev libao-dev
make
./build/linux/nanords --help
```

`--fifo` uses a **real POSIX FIFO** on Linux:

```sh
mkfifo /tmp/nanords-control
./build/linux/nanords --ps MyRadio --fifo /tmp/nanords-control
```

In another terminal: `printf 'RT Artist - Song\n' > /tmp/nanords-control`

## Windows (MSYS2 UCRT64 / MinGW-w64)

Install [MSYS2](https://www.msys2.org/), open the **UCRT64** terminal (not plain MSYS), then install the native packages:

```sh
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-libsamplerate mingw-w64-ucrt-x86_64-portaudio
make
./build/windows/nanords.exe --list-devices
```

The Windows output backend uses **PortAudio WASAPI exclusive mode, 192 kHz stereo**; select a compatible audio device. An on-device Windows test is recommended.

Windows requires the matching runtime DLLs from your MSYS2 environment (for example, `libsamplerate-0.dll` and `libportaudio.dll`) to be available next to `nanords.exe` or in your Windows `PATH`. For manual/local builds, copy the required non-system DLLs next to the executable. **GitHub release downloads include those DLLs automatically**. Third-party library licenses still apply when distributing DLLs.

On Windows, `--fifo` uses an **ordinary existing `.txt` file**, not a FIFO or Windows named pipe. It reads its contents at startup and reloads saved edits approximately every 250 ms:

```sh
cp commands.example.txt commands.txt
./build/windows/nanords.exe --ps MyRadio --fifo commands.txt
```

Example `commands.txt` contents (one command per line):

```text
PS MyRadio
RT Artist - Song Title
PTY 10
STEREO 9
RDS 9
```

The file accepts UTF-8 (optional BOM), Windows CRLF, and files up to 64 KiB. Changes are loaded as a whole when saved. Removing a command does **not** undo the previously applied setting; use a replacement command or `RESET`.

## Build options

```sh
make                         # build: fixed 57 kHz RDS subcarrier
make clean && make RBDS=1    # enable North American RBDS options
make clean                   # remove generated build files
```

The 57 kHz RDS carrier is hardcoded in `fm_mpx.c` and cannot be changed via Makefile flags. **Run `make clean` before changing `RBDS`**. Use `make CC=... PKG_CONFIG=...` if your compiler or pkg-config command differs. The Makefile detects MinGW compiler targets.

## Automatic GitHub Releases

The `.github/workflows/release.yml` workflow builds Windows (MSYS2 UCRT64) and Linux (Ubuntu x64) binaries using the **same Makefile**. It does not add a `make package` target.

1. Commit and push this source repository to GitHub (including `.github/workflows/release.yml`).
2. To verify builds before a release, go to **Actions → Build and publish NanoRDS → Run workflow**. It uploads downloadable build artifacts but **does not create a Release**.
3. When ready to publish a version, create and push a version tag:

   ```sh
   git tag v1.0.0
   git push origin v1.0.0
   ```

4. After the workflow succeeds, open **Releases**. It will contain `NanoRDS-Windows-x64.zip` (executable, runtime DLLs, license notices) and `NanoRDS-Linux-x64.tar.gz` (executable and documentation).

The Windows CI build detects **direct and transitive** MinGW DLL imports and copies those found in the UCRT64 environment into the ZIP. Windows system DLLs are intentionally not included. Local `make` behavior remains unchanged; a manually built Windows executable still needs its runtime DLLs available. This release automation has **not yet been run on GitHub Actions**. The Linux binary dynamically links to distro libraries and is not guaranteed to work on every Linux distribution; note the target distro/architecture in each release.

## Notes

- Output: 192 kHz, 16-bit stereo, FM multiplex on left channel and silence on right; no music/stereo audio input.
- Linux audio: default `libao` device. Windows audio: PortAudio WASAPI (`--device`, `--list-devices`).
- Run `nanords --help` for the remaining CLI options.
- `--fifo` on Linux requires an existing FIFO made with `mkfifo`; on Windows an existing `.txt` file.
- This GitHub repository contains **source only**. The executable links to separately installed libsamplerate, libao (Linux), or PortAudio (Windows).

## License and provenance

Application license: GPL-3.0; see `LICENSE`. Application source comes from the user-supplied NanoRDS Windows port, with the waveform source and license based on [barteqcz/NanoRDS](https://github.com/barteqcz/NanoRDS), commit `350a877634db6803d7fcf126a3dbb16aaf2f8884`. Bundled third-party source has been removed; the third-party library licenses apply to their separately installed packages.
