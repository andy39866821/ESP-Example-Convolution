// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef __ESP_CFG_000_H__
#define __ESP_CFG_000_H__

#include "libesp.h"
#include "conv_stratus.h"

typedef int32_t token_t;

/* <<--params-def-->> */
#define _MEM_OUTPUT_ADDR 10000
#define _MEM_INPUT_ADDR 10000
#define _S 5
#define _R 5
#define _Q 28
#define _P 28
#define _M 6
#define _C 3

/* <<--params-->> */
const int32_t mem_output_addr = _MEM_OUTPUT_ADDR;
const int32_t mem_input_addr = _MEM_INPUT_ADDR;
const int32_t S = _S;
const int32_t R = _R;
const int32_t Q = _Q;
const int32_t P = _P;
const int32_t M = _M;
const int32_t C = _C;

#define NACC 1

struct conv_stratus_access conv_cfg_000[] = {
	{
		/* <<--descriptor-->> */
		.mem_output_addr = _MEM_OUTPUT_ADDR,
		.mem_input_addr = _MEM_INPUT_ADDR,
		.S = _S,
		.R = _R,
		.Q = _Q,
		.P = _P,
		.M = _M,
		.C = _C,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_NONE,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

esp_thread_info_t cfg_000[] = {
	{
		.run = true,
		.devname = "conv_stratus.0",
		.ioctl_req = CONV_STRATUS_IOC_ACCESS,
		.esp_desc = &(conv_cfg_000[0].esp),
	}
};

#endif /* __ESP_CFG_000_H__ */
