# Third-Party Notices

The TETRA Android receiver is licensed under the **GNU Affero General Public
License, version 3 or later** (AGPL-3.0-or-later) — see [`LICENSE`](LICENSE).

It is built from, links, or is derived from the third-party components listed
below.  Each component remains under its own licence and copyright.

## Components

| Component | Vendored at | Licence | Copyright / source |
|---|---|---|---|
| **liquid-dsp** | `app/src/main/cpp/thirdparty/liquid-dsp` | **MIT** | (c) 2007–2026 Joseph Gaeddert — https://github.com/jgaeddert/liquid-dsp |
| **libusb** | `app/src/main/cpp/thirdparty/libusb` | **LGPL-2.1** | libusb project — https://libusb.info |
| **librtlsdr** | `app/src/main/cpp/thirdparty/librtlsdr` | **GPL-2.0-or-later** | rtlsdr project (rtl-sdr-blog fork) — https://github.com/rtlsdrblog/rtl-sdr-blog |
| **osmo-tetra (sq5bpf fork)** — PHY, lower/upper MAC, MLE, MAC-PDU, LLC, CMCE, SNDCP | `app/src/main/cpp/tetra` | **AGPL-3.0-or-later** | (c) Harald Welte / the osmocom & sq5bpf contributors — https://github.com/sq5bpf/osmo-tetra-sq5bpf-2 |
| **osmo-tetra (sq5bpf fork)** — crypto (hurdle, taa1, tea1–tea3, tetra_crypto) | `app/src/main/cpp/tetra/crypto` | **AGPL-3.0-or-later** | (c) 2023 Midnight Blue B.V. |
| **libosmocore** — convolutional codec (`compat/src/conv.c`, `compat/include/osmocom/core/conv.h`) and the message-buffer / list / talloc compatibility API | `app/src/main/cpp/compat` | **GPL-2.0-or-later** | (c) 2011 Sylvain Munaut and the osmocom contributors — https://github.com/osmocom/libosmocore |
| **tetra-rtlsdr** — π/4-DQPSK demodulator (ported into `engine/dqpsk_demod.c`) | — | **GPL-3.0-or-later** | (c) 2026 CEMAXECUTER LLC — the source this demodulator was ported from |
| **ETSI EN 300 395-2 TETRA speech codec** | `app/src/main/cpp/thirdparty/etsi_codec` (**not distributed**) | **ETSI reference-software terms** (not an open-source licence) | ETSI — see `app/src/main/cpp/thirdparty/etsi_codec/PROVENANCE.txt` |

## Licence text

The full licence texts are kept next to the corresponding sources:

- `app/src/main/cpp/thirdparty/liquid-dsp/LICENSE` (MIT)
- `app/src/main/cpp/thirdparty/libusb/COPYING` (LGPL-2.1)
- `app/src/main/cpp/thirdparty/librtlsdr/COPYING` (GPL-2.0)
- `app/src/main/cpp/tetra/COPYING` (AGPL-3.0)
- `LICENSE` (AGPL-3.0, for the project as a whole)
- `app/src/main/cpp/compat/COPYING.AGPL` (AGPL-3.0, for the compat layer)

## How the licences combine

The native library statically links AGPL-3.0-or-later code (osmo-tetra), so the
combined work is distributed under **AGPL-3.0-or-later** and the complete
corresponding source is published alongside it.

- GPL-2.0-or-later and GPL-3.0-or-later components are compatible with, and
  combined into, the AGPL-3.0 work (GPL-2.0-or-later may be upgraded to
  GPL-3.0).
- MIT components require only that their copyright and licence notice be
  preserved (they are).
- **libusb (LGPL-2.1)** is statically linked.  To satisfy LGPL-2.1 §6, the
  complete corresponding source and build scripts for the app (including the
  unmodified libusb sources under `app/src/main/cpp/thirdparty/libusb`) are
  published, so the library can be rebuilt and the application relinked.
- **The ETSI speech codec is deliberately not redistributed.**  It is fetched
  from the ETSI website by the build scripts and is governed by ETSI's own
  terms; see `thirdparty/etsi_codec/PROVENANCE.txt`.

## Modifications

The files carried over from upstream were adapted for Android (build system,
removal of the telive/TETMON network glue, audio plumbing).  Their original
copyright and licence headers are retained.  New files carry an
`SPDX-License-Identifier` header.

The Android adaptations and all new files are:

    Copyright (C) 2026 Shi Jimmy — AGPL-3.0-or-later
