// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "conv.hpp"
#include "conv_directives.hpp"

// Functions

#include "conv_functions.hpp"

void conv::load_counter(){
    int load_index;
    {
        HLS_PROTO("load-counter-reset");
        load_index = 0;
        wait();
    }
    while(1){
        //cout << "Current load index: " << load_index << endl;
        while(!load_start){
            wait();
            weight_load_time[load_index] = 0;
        }
        while(load_start){
            wait();
            weight_load_time[load_index]++;
        }
        load_index++;
    }
}

void conv::compute_counter(){   
    int compute_index = 0;
    {
        HLS_PROTO("computes-counter-reset");
        compute_index = 0;
        wait();
    }
    while(1){
        //cout << "Current compute index: " << compute_index << endl;
        while(!compute_start){
            wait();
            kernel_compute_time[compute_index] = 0;
        }
        while(compute_start){
            wait();
            kernel_compute_time[compute_index]++;
        }
        compute_index++;
    }
}

void conv::store_counter(){
    int store_index = 0;
    {
        HLS_PROTO("store-counter-reset");
        store_index = 0;
        wait();
    }
    while(1){
        //cout << "Current store index: " << store_index << endl;
        while(!store_start){
            wait();
            result_write_time[store_index] = 0;
        }
        while(store_start){
            wait();
            result_write_time[store_index]++;
        }
        store_index++;
    }
}
// Processes
void conv::clock_cycle_counter(){

    {
        HLS_PROTO("cycle-counter-reset");

        cycle_counter = 0;
        cycle_counter_overflow = false;
        

        wait();
    }
    {
        HLS_PROTO("cycle-counter-waiting");
        while(!acc_start){
            wait();
        }
    }
    {
        HLS_PROTO("cycle-counter-waiting");    
        while(!acc_finish){
            wait();
            cycle_counter++;
            if(cycle_counter >= UINT32_MAX)
                cycle_counter_overflow = true;   
        }
    }
}
void conv::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t mem_output_addr;
    int32_t mem_input_addr;
    int32_t S;
    int32_t R;
    int32_t Q;
    int32_t P;
    int32_t M;
    int32_t C;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        mem_output_addr = config.mem_output_addr;
        mem_input_addr = config.mem_input_addr;
        S = config.S;
        R = config.R;
        Q = config.Q;
        P = config.P;
        M = config.M;
        C = config.C;
        
        load_start = false;
    }
    acc_start = true;
    //printf("Load Start at: %d\n", (int)cycle_counter);
    // Load
    {
        HLS_PROTO("load-dma");
        wait();

        bool ping = true;

        int32_t index = 0;
        bool need_shift = false;

        
        uint32_t input_length = round_up(C*(P+R-1)*(Q+S-1), DMA_WORD_PER_BEAT);
        uint32_t weight_offset = input_length;
        uint32_t weight_length = round_up(C*R*S, DMA_WORD_PER_BEAT);

        // load whole image
        dma_info_t dma_info(0, input_length / DMA_WORD_PER_BEAT, DMA_SIZE);
        sc_dt::sc_bv<DMA_WIDTH> dataBv;
        this->dma_read_ctrl.put(dma_info);

        for(int i = 0 ; i < input_length ; i += DMA_WORD_PER_BEAT){

            //printf("Load Input Start at: %d\n", (int)cycle_counter);
            HLS_BREAK_DEP(plm_in);


            dataBv = this->dma_read_chnl.get();
            wait();

            // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
            for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++) {
                //HLS_UNROLL_SIMPLE;
                wait();
                plm_in[index + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                //printf("plm_in[%d]:%d\n", index + k, (int)plm_in[index + k]);
            }
            index += DMA_WORD_PER_BEAT;
            
        }
        
        need_shift = (index > C*(P+R-1)*(Q+S-1));

        //printf("Load Input End at: %d\n", (int)cycle_counter);

        // Chunking weight loading
        for (int m = 0; m < M; m++)
        {    
            load_start = true;
            //printf("Load Weight[%d] Start at: %d\n", m, load_weight_start);
            if(need_shift == true){
                dma_info_t dma_info((weight_offset / DMA_WORD_PER_BEAT) - 1, weight_length / DMA_WORD_PER_BEAT, DMA_SIZE);
                //cout << "Set weight_offset: " << weight_offset << endl;
                weight_offset += weight_length - 2;
                this->dma_read_ctrl.put(dma_info);
            }
            else {
                dma_info_t dma_info(weight_offset / DMA_WORD_PER_BEAT, weight_length / DMA_WORD_PER_BEAT, DMA_SIZE);
                //cout << "Set weight_offset: " << weight_offset << endl;
                weight_offset += weight_length;
                this->dma_read_ctrl.put(dma_info);
            }
            index = 0;
            wait();
            

            for (uint16_t i = 0; i < weight_length; i += DMA_WORD_PER_BEAT)
            {
                HLS_BREAK_DEP(plm_weight_ping);
                HLS_BREAK_DEP(plm_weight_pong);

                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                dataBv = this->dma_read_chnl.get();
                wait();

                if(need_shift){
                    if (ping){
                        plm_weight_ping[index] = dataBv.range(63,32).to_int64();
                        //printf("plm_weight[%d]:%d\n", index, (int)plm_weight_ping[index]);
                    }
                    else{
                        plm_weight_pong[index] = dataBv.range(63,32).to_int64();
                        //printf("plm_weight[%d]:%d\n", index, (int)plm_weight_pong[index]);
                    }
                    index++;
                    need_shift = false;
                    continue;
                }

                // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                {
                    //HLS_UNROLL_SIMPLE;
                    wait();
                    if (ping){ 
                        plm_weight_ping[index+k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        //printf("plm_weight[%d]:%d\n", index, (int)plm_weight_ping[index]);
                    }
                    else{
                        plm_weight_pong[index+k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        //printf("plm_weight[%d]:%d\n", index, (int)plm_weight_pong[index]);
                    }
                }
                index += DMA_WORD_PER_BEAT;
                
            }

            need_shift = (index > C*R*S);
            
            load_start = false;
            this->load_compute_handshake();
            ping = !ping;
        }
        
    }

    // Conclude
    {
        
        this->process_done();
    }
}



void conv::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        this->reset_store_output();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t mem_output_addr;
    int32_t mem_input_addr;
    int32_t S;
    int32_t R;
    int32_t Q;
    int32_t P;
    int32_t M;
    int32_t C;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        store_start = false;
        // User-defined config code
        /* <<--local-params-->> */
        mem_output_addr = config.mem_output_addr;
        mem_input_addr = config.mem_input_addr;
        S = config.S;
        R = config.R;
        Q = config.Q;
        P = config.P;
        M = config.M;
        C = config.C;
    }

    // Store
    {
        HLS_PROTO("store-dma");
        wait();

        bool ping = true;
        
        uint32_t store_offset = round_up(C*(P+R-1)*(Q+S-1)+M*C*R*S, DMA_WORD_PER_BEAT) * 1;

        uint32_t offset = store_offset;

        wait();
        
        

        for (int m = 0; m < M; m++)
        {
            
            this->store_compute_handshake();

            store_start = true;
            int write_result_start = (int)cycle_counter;
            // Configure DMA transaction
            int dma_len;
            if(m == M - 1)
                dma_len =  (P*Q) / DMA_WORD_PER_BEAT + 2 + M*3;
            else
                dma_len =  (P*Q) / DMA_WORD_PER_BEAT;

            dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, dma_len, DMA_SIZE);

            offset += P*Q;

            this->dma_write_ctrl.put(dma_info);
            //cout << "Start write at " << offset << endl;

            for (uint16_t i = 0; i < P*Q; i += DMA_WORD_PER_BEAT)
            {
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                // Read from PLM
                wait();
                for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                {
                    HLS_UNROLL_SIMPLE;
                    if (ping)
                        dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_ping[i + k];
                    else
                        dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_pong[i + k];
                }
                this->dma_write_chnl.put(dataBv);
            }

            ping = !ping;
            
            store_start = false;
        }
        
    }

    {

        
        HLS_PROTO("store-counter");
        wait();
        sc_dt::sc_bv<DMA_WIDTH> dataBv1(cycle_counter);
        this->dma_write_chnl.put(dataBv1);
        wait();
        sc_dt::sc_bv<DMA_WIDTH> dataBv2(cycle_counter_overflow);
        this->dma_write_chnl.put(dataBv2);
        for(int m = 0 ; m < M ; m++){
            wait();
            sc_dt::sc_bv<DMA_WIDTH> data(weight_load_time[m]);
            this->dma_write_chnl.put(data);
        }

        for(int m = 0 ; m < M ; m++){
            wait();
            sc_dt::sc_bv<DMA_WIDTH> data(kernel_compute_time[m]);
            this->dma_write_chnl.put(data);
        }

        for(int m = 0 ; m < M ; m++){
            wait();
            sc_dt::sc_bv<DMA_WIDTH> data(result_write_time[m]);
            this->dma_write_chnl.put(data);
        }
        acc_finish = true;

    }

    // Conclude
    {
        this->accelerator_done();
        this->process_done();
        
    }
}


void conv::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        // explicit PLM ports reset if any

        // User-defined reset code
        compute_start = false;
        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t mem_output_addr;
    int32_t mem_input_addr;
    int32_t S;
    int32_t R;
    int32_t Q;
    int32_t P;
    int32_t M;
    int32_t C;
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        mem_output_addr = config.mem_output_addr;
        mem_input_addr = config.mem_input_addr;
        S = config.S;
        R = config.R;
        Q = config.Q;
        P = config.P;
        M = config.M;
        C = config.C;
    }


    // Compute
    bool ping = true;
    {
        for(int m = 0 ; m < M ; m++){
            this->compute_load_handshake();
            compute_start = true;
            {
                for (int p = 0 ; p < P ; p++){
                    wait();
                    for (int q = 0 ; q < Q ; q++){

                        wait();
                        sc_dt::sc_int<DATA_WIDTH> acc = 0;

                        int gold_index = p*Q +q;
                        // if(ping)
                        //     plm_out_ping[gold_index] = 0;
                        // else
                        //     plm_out_pong[gold_index] = 0;

                        for (int c = 0 ; c < C ; c++){
                            wait();
                            for (int r = 0 ; r < R ; r++){
                                wait();
                                for (int s = 0 ; s < S ; s++){

                                    HLS_PROTO("compute-kernel");
                                    HLS_UNROLL_LOOP(AGGRESSIVE, 5, "inner_loop");
                                    HLS_CONSTRAIN_LATENCY(1, 5, "inner");
                                    wait();
                                    int input_index = c*(P+R-1)*(Q+S-1) + (p+r)*(Q+S-1) + (q+s);
                                    int weight_index = c*R*S + r*S + s;
                                    
                                    if(ping) {
                                        wait();
                                        acc += plm_in[input_index] * plm_weight_ping[weight_index];
                                    }
                                    else{
                                        wait();
                                        acc += plm_in[input_index] * plm_weight_pong[weight_index];
                                     }
                                }
                            }
                        }
                        
                        if(ping)
                            plm_out_ping[gold_index] = acc;
                        else
                            plm_out_pong[gold_index] = acc;
                    }
                }
            }
            
    
            compute_start = false;

            this->compute_store_handshake();
            ping = !ping;
        }
        

        // Conclude
        {
            this->process_done();
        }
    }
}
