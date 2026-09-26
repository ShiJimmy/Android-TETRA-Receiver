#ifndef TETRA_MM_PDU_H
#define TETRA_MM_PDU_H

/* 16.10.39 PDU Type */
enum tetra_mm_pdu_type_d {
	TMM_PDU_T_D_OTAR		= 0x0,
	TMM_PDU_T_D_AUTH		= 0x1,
	TMM_PDU_T_D_CK_CHG_DEM		= 0x2,
	TMM_PDU_T_D_DISABLE		= 0x3,
	TMM_PDU_T_D_ENABLE		= 0x4,
	TMM_PDU_T_D_LOC_UPD_ACC		= 0x5,
	TMM_PDU_T_D_LOC_UPD_CMD		= 0x6,
	TMM_PDU_T_D_LOC_UPD_REJ		= 0x7,
	/* RES */
	TMM_PDU_T_D_LOC_UPD_PROC	= 0x9,
	TMM_PDU_T_D_ATT_DET_GRP		= 0xa,
	TMM_PDU_T_D_ATT_DET_GRP_ACK	= 0xb,
	TMM_PDU_T_D_MM_STATUS		= 0xc,
	/* RES */
	/* RES */
	TMM_PDU_T_D_MM_PDU_NOTSUPP	= 0xf
};

/* Authentication PDU subtypes */
enum tetra_auth_sub_type {
       TMM_AUTH_ST_DEMAND = 0,
       TMM_AUTH_ST_RESPONSE = 1,
       TMM_AUTH_ST_RESULT = 2,
       TMM_AUTH_ST_REJECT = 3,
};

/* OTAR PDU subtypes */
enum tetra_otar_sub_type {
       TMM_OTAR_ST_CCK_PROVIDE = 0,
       TMM_OTAR_ST_CCK_REJECT,
       TMM_OTAR_ST_SCK_PROVIDE,
       TMM_OTAR_ST_SCK_REJECT,
       TMM_OTAR_ST_GCK_PROVIDE,
       TMM_OTAR_ST_GCK_REJECT,
       TMM_OTAR_ST_KEY_ASSOCIATE_DEMAND,
       TMM_OTAR_ST_OTAR_NEWCELL,
       TMM_OTAR_ST_GSKO_PROVIDE,
       TMM_OTAR_ST_GSKO_REJECT,
       TMM_OTAR_ST_KEY_DELETE_DEMAND,
       TMM_OTAR_ST_KEY_STATUS_DEMAND,
       TMM_OTAR_ST_CMG_GTSI_PROVIDE,
       TMM_OTAR_ST_DM_SCK_ACTIVATE,
       TMM_OTAR_ST_RESERVED_14,
       TMM_OTAR_ST_RESERVED_15,
};

/* 16.10.35a Location update accept type */
enum tetra_mm_loc_upd_acc_type {
	TMM_LUPD_ACC_T_ROAMING		= 0,
	TMM_LUPD_ACC_T_TEMPORARY	= 1,
	TMM_LUPD_ACC_T_PERIODIC		= 2,
	TMM_LUPD_ACC_T_ITSI_ATT		= 3,
	TMM_LUPD_ACC_T_CALL_RESTORE	= 4,
	TMM_LUPD_ACC_T_MIGRATING	= 5,
	TMM_LUPD_ACC_T_DEMAND		= 6,
	TMM_LUPD_ACC_T_DISABLED		= 7
};

const char *tetra_get_mm_pdut_name(uint8_t pdut, int uplink);
const char *tetra_get_auth_sub_type_name(uint8_t sub_type);
const char *tetra_get_otar_sub_type_name(uint8_t sub_type);

#endif
