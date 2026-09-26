/* Stub <tuntap.h> for the Android TETRA receiver.
 *
 * The upstream osmo-tetra can bridge decoded LLC SDUs onto a Linux TUN/TAP
 * device.  The Android receiver does not do this; tun_alloc() always fails so
 * the tuntap write path in tetra_llc.c is skipped.
 */
#ifndef TUNTAP_H
#define TUNTAP_H

int tun_alloc(char *dev);

#endif
