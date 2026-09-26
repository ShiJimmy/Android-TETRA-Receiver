/* Implementation of some PDU parsing of the TETRA LLC */

/* (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#include <osmocom/core/utils.h>

#include "tetra_common.h"
#include "tetra_llc_pdu.h"

static const struct value_string tetra_llc_pdut_names[] = {
	{ TLLC_PDUT_BL_ADATA,		"BL-ADATA" },
	{ TLLC_PDUT_BL_DATA,		"BL-DATA" },
	{ TLLC_PDUT_BL_UDATA,		"BL-UDATA" },
	{ TLLC_PDUT_BL_ACK,		"BL-ACK" },
	{ TLLC_PDUT_BL_ADATA_FCS,	"BL-ADATA-FCS" },
	{ TLLC_PDUT_BL_DATA_FCS,	"BL-DATA-FCS" },
	{ TLLC_PDUT_BL_UDATA_FCS,	"BL-UDATA-FCS" },
	{ TLLC_PDUT_BL_ACK_FCS,		"BL-ACK-FCS" },
	{ TLLC_PDUT_AL_SETUP,		"AL-SETUP" },
	{ TLLC_PDUT_AL_DATA_FINAL,	"AL-DATA/FINAL" },
	{ TLLC_PDUT_AL_UDATA_UFINAL,	"AL-UDATA/FINAL" },
	{ TLLC_PDUT_AL_ACK_RNR,		"AL-ACK/AL-RNR" },
	{ TLLC_PDUT_AL_RECONNECT,	"AL-RECONNECT" },
	{ TLLC_PDUT_SUPPL,		"AL-SUPPLEMENTARY" },
	{ TLLC_PDUT_L2SIG,		"AL-L2SIG" },
	{ TLLD_PDUT_AL_DISC,		"AL-DISC" },
	{ 0, NULL }
};
const char *tetra_get_llc_pdut_name(uint8_t pdut)
{
	return get_value_string(tetra_llc_pdut_names, pdut);
}

static const struct value_string pdut_dec_names[] = {
	{ TLLC_PDUT_DEC_BL_ADATA,	"BL-ADATA" },
	{ TLLC_PDUT_DEC_BL_DATA,	"BL-DATA" },
	{ TLLC_PDUT_DEC_BL_UDATA,	"BL-UDATA" },
	{ TLLC_PDUT_DEC_BL_ACK,		"BL-ACK" },
	{ TLLC_PDUT_DEC_AL_SETUP,	"AL-SETUP" },
	{ TLLC_PDUT_DEC_AL_DATA,	"AL-DATA" },
	{ TLLC_PDUT_DEC_AL_FINAL,	"AL-FINAL" },
	{ TLLC_PDUT_DEC_AL_UDATA,	"AL-UDATA" },
	{ TLLC_PDUT_DEC_AL_UFINAL,	"AL-UFINAL" },
	{ TLLC_PDUT_DEC_AL_ACK,		"AL-ACK" },
	{ TLLC_PDUT_DEC_AL_RNR,		"AL-RNR" },
	{ TLLC_PDUT_DEC_AL_RECONNECT,	"AL-RECONNECT" },
	{ TLLC_PDUT_DEC_AL_DISC,	"AL-DISC" },
	{ TLLC_PDUT_DEC_ALX_DATA,	"ALX-DATA" },
	{ TLLC_PDUT_DEC_ALX_FINAL,	"ALX-FINAL" },
	{ TLLC_PDUT_DEC_ALX_UDATA,	"ALX-UDATA" },
	{ TLLC_PDUT_DEC_ALX_UFINAL,	"ALX-UFINAL" },
	{ TLLC_PDUT_DEC_ALX_ACK,	"ALX-ACK" },
	{ TLLC_PDUT_DEC_ALX_RNR,	"ALX-RNR" },
	{ 0, NULL }
};

const char *tetra_get_llc_pdut_dec_name(enum tllc_pdut_dec pdut)
{
	return get_value_string(pdut_dec_names, pdut);
}

static const uint8_t tetra_llc_pdu_lengths[16] = {
	6,	/* TLLC_PDUT_BL_ADATA */
	5,	/* TLLC_PDUT_BL_DATA */
	4,	/* TLLC_PDUT_BL_UDATA */
	5,	/* TLLC_PDUT_BL_ACK */
	6 + 32,	/* TLLC_PDUT_BL_ADATA_FCS */
	5 + 32,	/* TLLC_PDUT_BL_DATA_FCS */
	4 + 32,	/* TLLC_PDUT_BL_UDATA_FCS */
	5 + 32,	/* TLLC_PDUT_BL_ACK_FCS */
	16,	/* TLLC_PDUT_AL_SETUP */
	13,	/* TLLC_PDUT_AL_DATA_FINAL */
	17,	/* TLLC_PDUT_AL_UDATA_UFINAL */
	9,	/* TLLC_PDUT_AL_ACK_RNR */
	16,	/* TLLC_PDUT_AL_RECONNECT */
	0,	/* TLLC_PDUT_SUPPL (variable) */
	0,	/* TLLC_PDUT_L2SIG (variable) */
	8,	/* TLLD_PDUT_AL_DISC */
};

static uint32_t tetra_llc_compute_fcs(const uint8_t *buf, int len)
{
	uint32_t crc = 0xFFFFFFFF;
	if (len < 32) {
		crc <<= (32 - len);
	}

	for (size_t i = 0; i < len; i++) {
		uint8_t bit = (buf[i] ^ (crc >> 31)) & 1;
		crc <<= 1;
		if (bit) {
			crc = crc ^ 0x04C11DB7;
		}
	}
	return ~crc;
}

static bool tetra_llc_check_fcs(struct tetra_llc_pdu *lpp, uint8_t *buf, int len)
{
	uint32_t computed_fcs = tetra_llc_compute_fcs(buf, len);
	return lpp->fcs == computed_fcs;
}

int tetra_llc_pdu_parse(struct tetra_llc_pdu *lpp, uint8_t *buf, int len)
{
	uint8_t *cur = buf;
	uint8_t pdu_type;

	lpp->have_fcs = false;
	lpp->fcs = 0;
	lpp->fcs_invalid = false;
	pdu_type = bits_to_uint(cur, 4);
	cur += 4;

	/* Check length to prevent out of bounds reads */
	if (len < tetra_llc_pdu_lengths[pdu_type]) {
		printf("WARNING llc pdu too small to parse, needed %d\n", tetra_llc_pdu_lengths[pdu_type]);
		lpp->tl_sdu_len = 0;
		return len;
	}

	switch (pdu_type) {

	case TLLC_PDUT_BL_ADATA:
	case TLLC_PDUT_BL_ADATA_FCS:
		lpp->nr = *cur++;
		lpp->ns = *cur++;
		lpp->tl_sdu = cur;
		lpp->tl_sdu_len = len - (cur - buf);
		lpp->pdu_type = TLLC_PDUT_DEC_BL_ADATA;

		if (pdu_type == TLLC_PDUT_BL_ADATA_FCS) {
			lpp->tl_sdu_len -= 32;
			lpp->have_fcs = true;
			lpp->fcs = bits_to_uint(buf + len - 32, 32);
			lpp->fcs_invalid = !tetra_llc_check_fcs(lpp, cur, lpp->tl_sdu_len);
		}
		break;

	case TLLC_PDUT_BL_DATA:
	case TLLC_PDUT_BL_DATA_FCS:
		lpp->ns = *cur++;
		lpp->tl_sdu = cur;
		lpp->tl_sdu_len = len - (cur - buf);
		lpp->pdu_type = TLLC_PDUT_DEC_BL_DATA;

		if (pdu_type == TLLC_PDUT_BL_DATA_FCS) {
			lpp->tl_sdu_len -= 32;
			lpp->have_fcs = true;
			lpp->fcs = bits_to_uint(buf + len - 32, 32);
			lpp->fcs_invalid = !tetra_llc_check_fcs(lpp, cur, lpp->tl_sdu_len);
		}
		break;

	case TLLC_PDUT_BL_UDATA:
	case TLLC_PDUT_BL_UDATA_FCS:
		lpp->tl_sdu = cur;
		lpp->tl_sdu_len = len - (cur - buf);
		lpp->pdu_type = TLLC_PDUT_DEC_BL_UDATA;

		if (pdu_type == TLLC_PDUT_BL_UDATA_FCS) {
			lpp->tl_sdu_len -= 32;
			lpp->have_fcs = true;
			lpp->fcs = bits_to_uint(buf + len - 32, 32);
			lpp->fcs_invalid = !tetra_llc_check_fcs(lpp, cur, lpp->tl_sdu_len);
		}
		break;

	case TLLC_PDUT_BL_ACK:
	case TLLC_PDUT_BL_ACK_FCS:
		lpp->nr = *cur++;
		lpp->tl_sdu = cur;
		lpp->tl_sdu_len = len - (cur - buf);
		lpp->pdu_type = TLLC_PDUT_DEC_BL_ACK;

		if (pdu_type == TLLC_PDUT_BL_ACK_FCS) {
			lpp->tl_sdu_len -= 32;
			lpp->have_fcs = true;
			lpp->fcs = bits_to_uint(buf + len - 32, 32);
			lpp->fcs_invalid = !tetra_llc_check_fcs(lpp, cur, lpp->tl_sdu_len);
		}
		break;

        case TLLC_PDUT_AL_SETUP:
                lpp->nr = bits_to_uint(cur, 8); cur += 8;
                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = len - (cur - buf);
                lpp->pdu_type = TLLC_PDUT_DEC_AL_SETUP;
                break;

	case TLLC_PDUT_AL_DATA_FINAL:
		if (*cur++) {
			/* FINAL */
			cur++; /* if set, AL_FINAL_AR */
			lpp->ns = bits_to_uint(cur, 3); cur += 3;
			lpp->ss = bits_to_uint(cur, 8); cur += 8;

			lpp->tl_sdu = cur;
			lpp->tl_sdu_len = len - (cur - buf);
			lpp->pdu_type = TLLC_PDUT_DEC_AL_FINAL;

			/* Needs to be defragmented so FCS is checked elsewhere */
			lpp->have_fcs = 1;

		} else {
			/* DATA Table 21.19 */
			cur++;
			lpp->ns = bits_to_uint(cur, 3); cur += 3;
			lpp->ss = bits_to_uint(cur, 8); cur += 8;
			lpp->tl_sdu = cur;
			lpp->tl_sdu_len = len - (cur - buf);
			lpp->pdu_type = TLLC_PDUT_DEC_AL_DATA;
		}
		break;

	case TLLC_PDUT_AL_UDATA_UFINAL:
		if (*cur++) {
			/* UFINAL 21.2.3.7 / Table 21.26 */
			lpp->ns = bits_to_uint(cur, 8); cur += 8;
			lpp->ss = bits_to_uint(cur, 8); cur += 8;
			lpp->tl_sdu = cur;
			lpp->tl_sdu_len = len - (cur - buf);
			lpp->pdu_type = TLLC_PDUT_DEC_AL_UFINAL;

			/* Needs to be defragmented so FCS is checked elsewhere */
			lpp->have_fcs = 1;

		} else {
			/* UDATA 21.2.3.6 / Table 21.24 */
			lpp->ns = bits_to_uint(cur, 8); cur += 8;
			lpp->ss = bits_to_uint(cur, 8); cur += 8;
			lpp->tl_sdu = cur;
			lpp->tl_sdu_len = len - (cur - buf);
			lpp->pdu_type = TLLC_PDUT_DEC_AL_UDATA;
		}
		break;

        case TLLC_PDUT_AL_ACK_RNR:
                if (*cur++)
                        lpp->pdu_type = TLLC_PDUT_DEC_AL_ACK;
                else
                        lpp->pdu_type = TLLC_PDUT_DEC_AL_RNR;
                lpp->nr = bits_to_uint(cur, 8); cur += 8;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = 0;
                break;

        case TLLC_PDUT_AL_RECONNECT:
                lpp->nr = bits_to_uint(cur, 8); cur += 8;
                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = len - (cur - buf);
                lpp->pdu_type = TLLC_PDUT_DEC_AL_RECONNECT;
                break;

        case TLLD_PDUT_AL_DISC:
                lpp->nr = bits_to_uint(cur, 8); cur += 8;
                lpp->pdu_type = TLLC_PDUT_DEC_AL_DISC;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = 0;
                break;

        case TLLC_PDUT_SUPPL:
                switch (bits_to_uint(cur, 2)) {
                case TLLC_PDUT_SUPPL_ALX_DATA_FINAL:
                        cur += 2;
                        if (*cur++) {
                                cur++; /* AL_FINAL_AR */
                                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                                lpp->ss = bits_to_uint(cur, 8); cur += 8;
                                lpp->tl_sdu = cur;
                                lpp->tl_sdu_len = len - (cur - buf);
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_FINAL;
                                lpp->have_fcs = 1;
                        } else {
                                cur++;
                                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                                lpp->ss = bits_to_uint(cur, 8); cur += 8;
                                lpp->tl_sdu = cur;
                                lpp->tl_sdu_len = len - (cur - buf);
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_DATA;
                        }
                        break;
                case TLLC_PDUT_SUPPL_ALX_UDATA_UFINAL:
                        cur += 2;
                        if (*cur++) {
                                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                                lpp->ss = bits_to_uint(cur, 8); cur += 8;
                                lpp->tl_sdu = cur;
                                lpp->tl_sdu_len = len - (cur - buf);
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_UFINAL;
                                lpp->have_fcs = 1;
                        } else {
                                lpp->ns = bits_to_uint(cur, 8); cur += 8;
                                lpp->ss = bits_to_uint(cur, 8); cur += 8;
                                lpp->tl_sdu = cur;
                                lpp->tl_sdu_len = len - (cur - buf);
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_UDATA;
                        }
                        break;
                case TLLC_PDUT_SUPPL_ALX_ACK_RNR:
                        cur += 2;
                        if (*cur++)
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_ACK;
                        else
                                lpp->pdu_type = TLLC_PDUT_DEC_ALX_RNR;
                        lpp->nr = bits_to_uint(cur, 8); cur += 8;
                        lpp->tl_sdu = cur;
                        lpp->tl_sdu_len = 0;
                        break;
                default:
                        lpp->pdu_type = TLLC_PDUT_DEC_UNKNOWN;
                        lpp->tl_sdu = cur;
                        lpp->tl_sdu_len = 0;
                        break;
                }
                break;
        case TLLC_PDUT_L2SIG:
                cur += 3; /* skip subtype */
                lpp->pdu_type = TLLC_PDUT_DEC_UNKNOWN;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = len - (cur - buf);
                break;
        default:
                lpp->pdu_type = TLLC_PDUT_DEC_UNKNOWN;
                lpp->tl_sdu = cur;
                lpp->tl_sdu_len = 0;
        }

	/* Sanity check to prevent (further) out of bounds reads */
	if (len < cur - buf) {
		lpp->tl_sdu_len = 0;
		return len;
	}

	return cur - buf;
}
