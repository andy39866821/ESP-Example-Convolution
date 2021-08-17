// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_HPP__
#define __CONV_HPP__

#include "conv_conf_info.hpp"
#include "conv_debug_info.hpp"

#include "esp_templates.hpp"

#include "conv_directives.hpp"

#define __round_mask(x, y) ((y)-1)
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
/* <<--defines-->> */
#define DATA_WIDTH 32
#define DMA_SIZE SIZE_WORD
#define PLM_OUT_WORD 1200
#define PLM_IN_WORD 4000
#define PLM_WEIGHT_WORD 1200

class conv : public esp_accelerator_3P<DMA_WIDTH>
{
public:
    // Constructor
    SC_HAS_PROCESS(conv);
    conv(const sc_module_name& name)
    : esp_accelerator_3P<DMA_WIDTH>(name)
        , cfg("config")
    {
        // Signal binding
        cfg.bind_with(*this);

        // Map arrays to memories
        /* <<--plm-bind-->> */
        HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);
        HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);
        HLS_MAP_plm(plm_in, PLM_IN_NAME);
        HLS_MAP_plm(plm_weight_pong, PLM_WEIGHT_NAME);
        HLS_MAP_plm(plm_weight_ping, PLM_WEIGHT_NAME);

        
        SC_CTHREAD(clock_cycle_counter, this->clk.pos());
        this->reset_signal_is(this->rst, false);
        
        SC_CTHREAD(load_counter, this->clk.pos());
        this->reset_signal_is(this->rst, false);
        SC_CTHREAD(compute_counter, this->clk.pos());
        this->reset_signal_is(this->rst, false);
        SC_CTHREAD(store_counter, this->clk.pos());
        this->reset_signal_is(this->rst, false);
    }

    // Processes

    // Load the input data
    void load_input();

    // Computation
    void compute_kernel();

    // Store the output data
    void store_output();

    // Store the output data
    void clock_cycle_counter();
    
    void load_counter();

    void compute_counter();

    void store_counter();

    // Configure conv
    esp_config_proc cfg;

    // Functions
    bool acc_start, acc_finish;
    uint32_t cycle_counter;
    bool cycle_counter_overflow;
    bool load_start, compute_start, store_start;
    int weight_load_time[50];
    int result_write_time[50];
    int kernel_compute_time[50];

    // Private local memories
    sc_dt::sc_int<DATA_WIDTH> plm_in[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_weight_ping[PLM_WEIGHT_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_weight_pong[PLM_WEIGHT_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_ping[PLM_OUT_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_pong[PLM_OUT_WORD];

};


#endif /* __CONV_HPP__ */
