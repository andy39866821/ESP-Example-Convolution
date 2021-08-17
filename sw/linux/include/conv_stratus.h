// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef _CONV_STRATUS_H_
#define _CONV_STRATUS_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#ifndef __user
#define __user
#endif
#endif /* __KERNEL__ */

#include <esp.h>
#include <esp_accelerator.h>

struct conv_stratus_access {
	struct esp_access esp;
	/* <<--regs-->> */
	unsigned mem_output_addr;
	unsigned mem_input_addr;
	unsigned S;
	unsigned R;
	unsigned Q;
	unsigned P;
	unsigned M;
	unsigned C;
	unsigned src_offset;
	unsigned dst_offset;
};

#define CONV_STRATUS_IOC_ACCESS	_IOW ('S', 0, struct conv_stratus_access)

#endif /* _CONV_STRATUS_H_ */
