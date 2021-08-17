// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include <sstream>
#include "system.hpp"

// Process
void system_t::config_proc()
{

    // Reset
    {
        conf_done.write(false);
        conf_info.write(conf_info_t());
        wait();
    }

    ESP_REPORT_INFO("reset done");

    // Config
    load_memory();
    {
        conf_info_t config;
        // Custom configuration
        /* <<--params-->> */
        config.mem_output_addr = mem_output_addr;
        config.mem_input_addr = mem_input_addr;
        config.S = S;
        config.R = R;
        config.Q = Q;
        config.P = P;
        config.M = M;
        config.C = C;

        wait(); conf_info.write(config);
        conf_done.write(true);
    }

    ESP_REPORT_INFO("config done");

    // Compute
    {
        // Print information about begin time
        sc_time begin_time = sc_time_stamp();
        ESP_REPORT_TIME(begin_time, "BEGIN - conv");

        // Wait the termination of the accelerator
        do { wait(); } while (!acc_done.read());
        debug_info_t debug_code = debug.read();

        // Print information about end time
        sc_time end_time = sc_time_stamp();
        ESP_REPORT_TIME(end_time, "END - conv");

        esc_log_latency(sc_object::basename(), clock_cycle(end_time - begin_time));
        wait(); conf_done.write(false);
    }

    // Validate
    {
        dump_memory(); // store the output in more suitable data structure if needed
        // check the results with the golden model
        if (validate())
        {
            ESP_REPORT_ERROR("validation failed!");
        } else
        {
            ESP_REPORT_INFO("validation passed!");
        }
    }

    // Conclude
    {
        sc_stop();
    }
}

// Functions
void system_t::load_memory()
{
    // Optional usage check
#ifdef CADENCE
    if (esc_argc() != 1)
    {
        ESP_REPORT_INFO("usage: %s\n", esc_argv()[0]);
        sc_stop();
    }
#endif

    // Input data and golden output (aligned to DMA_WIDTH makes your life easier)
#if (DMA_WORD_PER_BEAT == 0)
    in_words_adj = C*(P+R-1)*(Q+S-1)+M*C*R*S;
    out_words_adj = M*P*Q;
#else
    in_words_adj = round_up(C*(P+R-1)*(Q+S-1)+M*C*R*S, DMA_WORD_PER_BEAT);
    out_words_adj = round_up(M*P*Q, DMA_WORD_PER_BEAT);
    //printf("in_words_adj:%d\n", in_words_adj);
    //printf("out_words_adj:%d\n", out_words_adj);
#endif

    in_size = in_words_adj * (1);
    out_size = out_words_adj * (1);

    in = new int32_t[in_size ];
    int weight_base = C * (P + R - 1) * (Q + S - 1);

    int num = 0;
    int index = 0;
    // input
    for (int c = 0 ; c < C ; c++){
        for(int j = 0 ; j < (P + R - 1) ; j++){
            for(int k = 0 ; k < (Q + S - 1) ; k++){
                in[index++] = rand()%1000-500; // range from -50 ~ 49
                //in[index++] = num++; // range from -50 ~ 49
            }
        }
    }
    
    // weight
    
    for (int m = 0 ; m < M ; m++){
        //num = 0;
        for (int c = 0 ; c < C ; c++){
            for (int r = 0 ; r < R ; r++){
                for (int s = 0 ; s < S ; s++){
                    in[index++] = rand()%1000-500; // range from -500 ~ 499
                    //printf("weight[%d]:%d\n", index-1-weight_base, in[index-1]);
                    //in[index++] = (num++)%500; // range from -50 ~ 49
                }
            }
        }
    }

    gold = new int32_t[out_size];
    for (int m = 0 ; m < M ; m++){
        for (int p = 0 ; p < P ; p++){
            for (int q = 0 ; q < Q ; q++){
                int gold_index = m*P*Q + p*Q +q;
                gold[gold_index] = 0;
                for (int c = 0 ; c < C ; c++){
                    for (int r = 0 ; r < R ; r++){
                        for (int s = 0 ; s < S ; s++){
                            int input_index = c*(P+R-1)*(Q+S-1) + (p+r)*(Q+S-1) + (q+s);
                            int weight_index = weight_base + m*C*R*S + c*R*S + r*S + s;
                            // if(m == 0 && p == 0){
                            //     printf("acc[0][%d] += %d * %d\n",q, in[input_index], in[weight_index]);
                            // }
                            gold[gold_index] += in[input_index] * in[weight_index];
                        }
                    }
                }
            }
        }
    }
    
                            
                

    // Memory initialization:
#if (DMA_WORD_PER_BEAT == 0)
    for (int i = 0; i < in_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            mem[DMA_BEAT_PER_WORD * i + j] = data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH);
    }
#else
    for (int i = 0; i < in_size / DMA_WORD_PER_BEAT; i++)  {
        sc_dt::sc_bv<DMA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            data_bv.range((j+1) * DATA_WIDTH - 1, j * DATA_WIDTH) = in[i * DMA_WORD_PER_BEAT + j];
        mem[i] = data_bv;
    }
#endif

    ESP_REPORT_INFO("load memory completed");
}

void system_t::dump_memory()
{
    // Get results from memory
    out = new int32_t[out_size];
    uint32_t offset = in_size;

#if (DMA_WORD_PER_BEAT == 0)
    offset = offset * DMA_BEAT_PER_WORD;
    for (int i = 0; i < out_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv;

        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH) = mem[offset + DMA_BEAT_PER_WORD * i + j];

        out[i] = data_bv.to_int64();
    }
#else
    offset = offset / DMA_WORD_PER_BEAT;
    for (int i = 0; i < out_size / DMA_WORD_PER_BEAT; i++)
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            out[i * DMA_WORD_PER_BEAT + j] = mem[offset + i].range((j + 1) * DATA_WIDTH - 1, j * DATA_WIDTH).to_int64();
#endif


    sc_dt::sc_bv<DATA_WIDTH> data_bv;
    data_bv = mem[(in_size+out_size)/2 + 0];
    uint64_t counter = data_bv.to_int64();
    cout << "Hardware clock cycle counter: " << counter << endl;
    
    data_bv = mem[(in_size+out_size)/2 + 1];
    uint64_t overflow = data_bv.to_int64();
    cout << "Hardware clock cycle overflow: " << overflow << endl;
    

    int weight_load_total_time = 0;
    int result_write_total_time = 0;
    int kernel_compute_total_time = 0;
    printf("-------Weight Load time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = (in_size + out_size + 4 + m*2)/2;
        sc_dt::sc_bv<64> data;
        data.range(31, 0) = mem[data_offset + 0];
        data.range(63, 32) = mem[data_offset + 1];

        int64_t t = data.to_int64();
        printf("Weight Load[%d]: %d\n", m, t);
        weight_load_total_time += t;
    }
    printf("Total weight load time: %d\n", weight_load_total_time);

    printf("-------Kernel Compute time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = (in_size + out_size + 4 + M*2 + m*2)/2;
        sc_dt::sc_bv<64> data;
        data.range(31, 0) = mem[data_offset + 0];
        data.range(63, 32) = mem[data_offset + 1];

        int64_t t = data.to_int64();
        printf("Kernel Compute[%d]: %d\n", m, t);
        kernel_compute_total_time += t;
    }
    printf("Total kernel compute time: %d\n", kernel_compute_total_time);

    printf("-------Result Write time-------\n");
    for(int m = 0 ; m < M ; m++){        
        int data_offset = (in_size + out_size + 4 + M*2*2 + m*2)/2;
        sc_dt::sc_bv<64> data;
        data.range(31, 0) = mem[data_offset + 0];
        data.range(63, 32) = mem[data_offset + 1];

        int64_t t = data.to_int64();
        printf("Result Write[%d]: %d\n", m, t);
        result_write_total_time += t;
    }
    printf("Total result write time: %d\n", result_write_total_time);
    ESP_REPORT_INFO("dump memory completed");
}

int system_t::validate()
{
    // Check for mismatches
    uint32_t errors = 0;

    for (int i = 0; i < 1; i++)
        for (int j = 0; j < M*P*Q; j++)
            if (gold[i * out_words_adj + j] != out[i * out_words_adj + j]){
                errors++;
                // cout << "[ERROR] " << i * out_words_adj + j << endl;
                // cout << "   Correct: " << gold[i * out_words_adj + j] << endl;
                // cout << "   Result : " << out[i * out_words_adj + j] << endl;
            }

    delete [] in;
    delete [] out;
    delete [] gold;

    return errors;
}
