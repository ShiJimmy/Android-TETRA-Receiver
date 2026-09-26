<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# TETRA Android Receiver

An Android app that receives and decodes **TETRA** (Terrestrial Trunked Radio)
using an RTL-SDR dongle connected over USB-OTG, and plays the decoded voice
through the phone's speaker.

> Receiving only. The app does not transmit, and it does not break any
> encryption — it decodes clear (unencrypted) networks only.

## Features

- RTL-SDR capture (1.8 MSps) with an asynchronous USB reader and a sample ring
  buffer, so the channel is decoded continuously without dropped samples.
- π/4-DQPSK demodulation with band-edge FLL carrier recovery and symbol-timing
  recovery (liquid-dsp).
- Full TETRA PHY → MAC → LLC → MLE/CMCE decode chain (from osmo-tetra).
- ETSI speech decoder (ACELP) for TCH/S voice frames, played via `AudioTrack`.
- Spectrum waterfall (channel-centred), centre/channel frequency entry in MHz,
  PPM correction, gain and span controls.
- Status line with lock state, MCC/MNC/colour code and active-call indication.

## Requirements

- Android 6.0 (API 23) or newer, with USB host / OTG support.
- A RTL-SDR dongle (R820T/R820T2/R828D based).
- A powered USB-OTG adapter is recommended.

## Building

See [`BUILDING.md`](BUILDING.md).  In short:

```sh
tools/fetch_etsi_codec.sh          # or tools\fetch_etsi_codec.ps1 on Windows
./gradlew :app:assembleRelease      # produces app/build/outputs/apk/release/
```

The ETSI speech codec is **not** part of this repository and is downloaded
separately (see below).

## License

This project is licensed under the **GNU Affero General Public License,
version 3 or later** ([`LICENSE`](LICENSE)).

It builds on the work of others; the complete list of third-party components
and their licences is in [`THIRD_PARTY.md`](THIRD_PARTY.md).  In particular:

- **osmo-tetra (sq5bpf fork)** — AGPL-3.0-or-later — the TETRA protocol stack.
- **tetra-rtlsdr** — GPL-3.0-or-later — the demodulator this port is based on.
- **liquid-dsp** — MIT — DSP.
- **librtlsdr** — GPL-2.0-or-later — RTL-SDR driver.
- **libusb** — LGPL-2.1 — USB.
- **ETSI EN 300 395-2** TETRA speech codec — ETSI terms, not open source, and
  not redistributed here (downloaded on demand).

Because the app links AGPL-3.0 code, if you distribute a build you must also
make the complete corresponding source available under AGPL-3.0-or-later.

Copyright (C) 2026 Shi Jimmy.

## Author

**Shi Jimmy** (original author, 2026-09-27) — see [`AUTHORS`](AUTHORS).

## Credits

Thanks to Harald Welte and the osmocom project, Jacek Lipkowski (SQ5BPF),
Midnight Blue B.V., Joseph Gaeddert (liquid-dsp), and the librtlsdr/libusb
projects.

Source: `https://github.com/ShiJimmy/Android-TETRA-Receiver`
