/* Stub <tetra_sds.h> for the Android TETRA receiver.
 *
 * The upstream osmo-tetra has a full SDS text decoder (tetra_sds.c).  The
 * Android receiver currently drops SDS payloads, so only the entry point
 * referenced by the vendored decoder is declared here.
 */
#ifndef TETRA_SDS_H
#define TETRA_SDS_H

#include <stdint.h>
#include <osmocom/core/msgb.h>
#include "tetra_common.h"

unsigned int parse_d_sds_data(struct tetra_mac_state *tms, struct msgb *msg, unsigned int len);

#endif
