/* Implementation of TETRA MM PDU parsing */

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

#include <unistd.h>
#include <osmocom/core/utils.h>

#include "tetra_mm_pdu.h"

static const struct value_string mm_pdut_d_names[] = {
	{ TMM_PDU_T_D_OTAR,		"D-OTAR" },
	{ TMM_PDU_T_D_AUTH,		"D-AUTHENTICATION" },
	{ TMM_PDU_T_D_CK_CHG_DEM,	"D-CK CHANGE DEMAND" },
	{ TMM_PDU_T_D_DISABLE,		"D-DISABLE" },
	{ TMM_PDU_T_D_ENABLE,		"D-ENABLE" },
	{ TMM_PDU_T_D_LOC_UPD_ACC,	"D-LOCATION UPDATE ACCEPT" },
	{ TMM_PDU_T_D_LOC_UPD_CMD,	"D-LOCATION UPDATE COMMAND" },
	{ TMM_PDU_T_D_LOC_UPD_REJ,	"D-LOCATION UPDATE REJECT" },
	{ TMM_PDU_T_D_LOC_UPD_PROC,	"D-LOCATION UPDATE PROCEEDING" },
	{ TMM_PDU_T_D_ATT_DET_GRP,	"D-ATTACH/DETACH GROUP ID" },
	{ TMM_PDU_T_D_ATT_DET_GRP_ACK,	"D-ATTACH/DETACH GROUP ID ACK" },
	{ TMM_PDU_T_D_MM_STATUS,	"D-MM STATUS" },
	{ TMM_PDU_T_D_MM_PDU_NOTSUPP,	"MM PDU/FUNCTION NOT SUPPORTED" },
	{ 0, NULL }
};
const char *tetra_get_mm_pdut_name(uint8_t pdut, int uplink)
{
        /* FIXME: uplink */
        return get_value_string(mm_pdut_d_names, pdut);
}

static const struct value_string auth_sub_names[] = {
       { TMM_AUTH_ST_DEMAND,   "Demand" },
       { TMM_AUTH_ST_RESPONSE, "Response" },
       { TMM_AUTH_ST_RESULT,   "Result" },
       { TMM_AUTH_ST_REJECT,   "Reject" },
       { 0, NULL }
};

const char *tetra_get_auth_sub_type_name(uint8_t sub_type)
{
       return get_value_string(auth_sub_names, sub_type);
}

static const struct value_string otar_sub_names[] = {
       { TMM_OTAR_ST_CCK_PROVIDE,          "CCK Provide" },
       { TMM_OTAR_ST_CCK_REJECT,           "CCK Reject" },
       { TMM_OTAR_ST_SCK_PROVIDE,          "SCK Provide" },
       { TMM_OTAR_ST_SCK_REJECT,           "SCK Reject" },
       { TMM_OTAR_ST_GCK_PROVIDE,          "GCK Provide" },
       { TMM_OTAR_ST_GCK_REJECT,           "GCK Reject" },
       { TMM_OTAR_ST_KEY_ASSOCIATE_DEMAND, "Key associate Demand" },
       { TMM_OTAR_ST_OTAR_NEWCELL,         "OTAR NEWCELL" },
       { TMM_OTAR_ST_GSKO_PROVIDE,         "GSKO Provide" },
       { TMM_OTAR_ST_GSKO_REJECT,          "GSKO Reject" },
       { TMM_OTAR_ST_KEY_DELETE_DEMAND,    "Key delete demand" },
       { TMM_OTAR_ST_KEY_STATUS_DEMAND,    "Key status demand" },
       { TMM_OTAR_ST_CMG_GTSI_PROVIDE,     "CMG GTSI provide" },
       { TMM_OTAR_ST_DM_SCK_ACTIVATE,      "DM SCK Activate" },
       { TMM_OTAR_ST_RESERVED_14,          "Reserved 14" },
       { TMM_OTAR_ST_RESERVED_15,          "Reserved 15" },
       { 0, NULL }
};

const char *tetra_get_otar_sub_type_name(uint8_t sub_type)
{
       return get_value_string(otar_sub_names, sub_type);
}
