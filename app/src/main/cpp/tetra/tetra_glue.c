/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Definitions of the osmo-tetra "tetra hack" (telive) global state.
 *
 * In the upstream osmo-tetra-sq5bpf-2 these live in tetra_hack.c and are used
 * to emit TETMON UDP messages to the telive monitoring UI.  For the Android
 * receiver they are kept only to satisfy the decoder's references; the socket
 * is never opened so the sendto() calls become harmless no-ops.
 */

#include "tetra_common.h"

struct tetra_hack_struct tetra_hack_db[HACK_NUM_STRUCTS];

int tetra_hack_live_socket;
struct sockaddr_in tetra_hack_live_sockaddr;
int tetra_hack_socklen;

int tetra_hack_live_idx;
int tetra_hack_live_lastseen;
int tetra_hack_rxid;

int tetra_hack_packet_counter;

uint32_t tetra_hack_dl_freq, tetra_hack_ul_freq;
uint16_t tetra_hack_la;

uint8_t tetra_hack_freq_band;
uint8_t tetra_hack_freq_offset;

int tetra_hack_encoption;

uint8_t tetra_hack_seen_encryptions;

int tetra_hack_all_sds_as_text;
int tetra_hack_allow_encrypted;

/* Voice sink hook (installed by the engine). */
tetra_voice_fn tetra_voice_cb;
void *tetra_voice_cb_priv;

/* Call-status hook (installed by the engine). */
tetra_call_fn tetra_call_cb;
void *tetra_call_cb_priv;
