/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 *  Copyright 2005 Spiro Trikaliotis
*/

/* $Id: libtrans.h,v 1.2 2006-05-23 12:01:05 wmsr Exp $ */

#ifndef LIBTRANS_H
#define LIBTRANS_H

#include "opencbm.h"
#include <stdio.h>

#define LIBOCT_STATE_DEBUG  /* enable state logging and debugging */

#ifdef __cplusplus
extern "C" {                /* allow linkage to C++ programs      */
#endif

typedef
enum opencbm_transfer_e
{
    opencbm_transfer_serial1,
    opencbm_transfer_serial2,
    opencbm_transfer_parallel
} opencbm_transfer_t;

int
libopencbmtransfer_set_transfer(opencbm_transfer_t type);

int
libopencbmtransfer_install(CBM_FILE HandleDevice, __u_char DeviceAddress);

int
libopencbmtransfer_execute_command(CBM_FILE HandleDevice, __u_char DeviceAddress,
                                   unsigned int ExecutionAddress);

int
libopencbmtransfer_read_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                            __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length);

int
libopencbmtransfer_write_mem(CBM_FILE HandleDevice, __u_char DeviceAddress,
                            __u_char Buffer[], unsigned int MemoryAddress, unsigned int Length);

int
libopencbmtransfer_remove(CBM_FILE HandleDevice, __u_char DeviceAddress);

#ifdef LIBOCT_STATE_DEBUG
extern void libopencbmtransfer_printStateDebugCounters(FILE *channel);
#endif

#ifdef __cplusplus
}
#endif
#endif /* #ifndef LIBTRANS_H */
