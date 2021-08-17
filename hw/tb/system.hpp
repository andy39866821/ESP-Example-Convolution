// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __SYSTEM_HPP__
#define __SYSTEM_HPP__

#include "conv_conf_info.hpp"
#include "conv_debug_info.hpp"
#include "conv.hpp"
#include "conv_directives.hpp"

#include "esp_templates.hpp"

const size_t MEM_SIZE = 329040 / (DMA_WIDTH/8);

#include "core/systems/esp_system.hpp"

#ifdef CADENCE
#include "conv_wrap.h"
#endif

class system_t : public esp_system<DMA_WIDTH, MEM_SIZE>
{
public:

    // ACC instance
#ifdef CADENCE
    conv_wrapper *acc;
#else
    conv *acc;
#endif

    // Constructor
    SC_HAS_PROCESS(system_t);
    system_t(sc_module_name name)
        : esp_system<DMA_WIDTH, MEM_SIZE>(name)
    {
        // ACC
#ifdef CADENCE
        acc = new conv_wrapper("conv_wrapper");
#else
        acc = new conv("conv_wrapper");
#endif
        // Binding ACC
        acc->clk(clk);
        acc->rst(acc_rst);
        acc->dma_read_ctrl(dma_read_ctrl);
        acc->dma_write_ctrl(dma_write_ctrl);
        acc->dma_read_chnl(dma_read_chnl);
        acc->dma_write_chnl(dma_write_chnl);
        acc->conf_info(conf_info);
        acc->conf_done(conf_done);
        acc->acc_done(acc_done);
        acc->debug(debug);

        /* <<--params-default-->> */
        mem_output_addr = 10000;
        mem_input_addr = 10000;
        S = 5;
        R = 5;
        Q = 28;
        P = 28;
        M = 6;
        C = 3;
    }

    // Processes

    // Configure accelerator
    void config_proc();

    // Load internal memory
    void load_memory();

    // Dump internal memory
    void dump_memory();

    // Validate accelerator results
    int validate();

    // Accelerator-specific data
    /* <<--params-->> */
    int32_t mem_output_addr;
    int32_t mem_input_addr;
    int32_t S;
    int32_t R;
    int32_t Q;
    int32_t P;
    int32_t M;
    int32_t C;

    uint32_t in_words_adj;
    uint32_t out_words_adj;
    uint32_t in_size;
    uint32_t out_size;
    int32_t *in;
    int32_t *out;
    int32_t *gold;

    // Other Functions
};

#endif // __SYSTEM_HPP__
