<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Building

## Toolchain

| Tool | Version used |
|---|---|
| JDK | 21 |
| Gradle | 8.7 (via the bundled wrapper) |
| Android Gradle Plugin | 8.5.0 |
| Kotlin | 1.9.24 |
| Android SDK platform | 34 |
| Android NDK | 26.1.10909125 |
| CMake | 3.22.1 |
| minSdk / targetSdk | 23 / 34 |

## 1. Get the ETSI speech codec

The ETSI EN 300 395-2 TETRA speech codec is **not part of this repository**
(it is not redistributable — see `THIRD_PARTY.md`).  Download it first:

```sh
# Linux / macOS
tools/fetch_etsi_codec.sh
```

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File tools\fetch_etsi_codec.ps1
```

This unpacks the codec into `app/src/main/cpp/thirdparty/etsi_codec/`.  If it
is missing, CMake aborts with an explanatory error.  Read the licence notes
that ship inside the archive (`C-WORD/C_WORD_*.DOC`) before using it.

## 2. Point Gradle at your SDK

Create `local.properties` in the project root:

```properties
sdk.dir=/path/to/Android/Sdk
```

## 3. Build

```sh
./gradlew :app:assembleDebug      # debug build (with in-app diagnostics)
./gradlew :app:assembleRelease    # clean build
```

Output:

- debug:   `app/build/outputs/apk/debug/app-debug.apk`
- release: `app/build/outputs/apk/release/app-release.apk`

### Debug vs. release

The native build takes a `-DTETRA_DEBUG` switch, set by the Gradle build type:

- **debug** (`-DTETRA_DEBUG=ON`): in-app log capture (`nativeGetLog`), raw IQ
  dump (`iq.bin`), bit dump (`bits.bin`), a `diag.txt` status file, and
  periodic status output.
- **release** (`-DTETRA_DEBUG=OFF`, the default): all of the above is compiled
  out, the diagnostic `printf()` calls are removed from the native code, the
  Java/Kotlin is minified and resource-shrunk, and native symbols are stripped.

To publish a clean build, just run `assembleRelease`.  (The release build type
is signed with the debug key so it installs directly; use your own keystore for
distribution.)

## Host tests (optional)

`host/` contains a small CMake project that builds the decoder and engine for
the desktop, used for offline testing (e.g. `iq_decode <iq.bin> <bits.bin>
[offset_hz]`).  It is not needed to build the app.
