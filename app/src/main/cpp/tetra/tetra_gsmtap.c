/* Stub <tetra_gsmtap.c> for the Android TETRA receiver. */

#include "tetra_gsmtap.h"

struct msgb *tetra_gsmtap_makemsg(struct tetra_tdma_time *tm, enum tetra_log_chan lchan,
				  uint8_t ts, uint8_t ss, int8_t signal_dbm, uint8_t snr,
				  const uint8_t *bitdata, unsigned int bitlen,
				  struct tetra_mac_state *tms)
{
	(void)tm;
	(void)lchan;
	(void)ts;
	(void)ss;
	(void)signal_dbm;
	(void)snr;
	(void)bitdata;
	(void)bitlen;
	(void)tms;
	return NULL;
}

int tetra_gsmtap_sendmsg(struct msgb *msg)
{
	(void)msg;
	return 0;
}

int tetra_gsmtap_init(const char *host, uint16_t port)
{
	(void)host;
	(void)port;
	return 0;
}
