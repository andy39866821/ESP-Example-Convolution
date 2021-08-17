/* Copyright (c) 2011-2021 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>
#include <fixed_point.h>

typedef int32_t token_t;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
        return (sizeof(void *) / _st);
}


#define SLD_CONV 0x098
#define DEV_NAME "sld,conv_stratus"

/* <<--params-->> */
const int32_t mem_output_addr = 10000;
const int32_t mem_input_addr = 10000;
const int32_t S = 5;
const int32_t R = 5;
const int32_t Q = 28;
const int32_t P = 28;
const int32_t M = 6;
const int32_t C = 3;

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 20
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK(_sz) ((_sz % CHUNK_SIZE == 0) ?		\
			(_sz / CHUNK_SIZE) :		\
			(_sz / CHUNK_SIZE) + 1)

/* User defined registers */
/* <<--regs-->> */
#define CONV_MEM_OUTPUT_ADDR_REG 0x5c
#define CONV_MEM_INPUT_ADDR_REG 0x58
#define CONV_S_REG 0x54
#define CONV_R_REG 0x50
#define CONV_Q_REG 0x4c
#define CONV_P_REG 0x48
#define CONV_M_REG 0x44
#define CONV_C_REG 0x40


static int validate_buf(token_t *out, token_t *gold)
{
	int i;
	int j;
	unsigned errors = 0;

	for (i = 0; i < 1; i++)
		for (j = 0; j < M*P*Q; j++)
			if (gold[i * out_words_adj + j] != out[i * out_words_adj + j]){
				
				errors++; 
			}
			else {
				//printf("[Correct] %d: %d\n", i * out_words_adj + j,out[i * out_words_adj + j]);
			}

	return errors;
}


static void init_buf (token_t *in, token_t * gold)
{
    int num = 0;
    int index = 0;
    int weight_base = C * (P + R - 1) * (Q + S - 1);
    // input
    for (int c = 0 ; c < C ; c++){
        for(int j = 0 ; j < (P + R - 1) ; j++){
            for(int k = 0 ; k < (Q + S - 1) ; k++){
                //in[index++] = (token_t)(rand()%1000-500); // range from -50 ~ 49
                in[index++] = (token_t)num++; // range from -50 ~ 49
            }
        }
    }
    
    // weight
    
    for (int m = 0 ; m < M ; m++){
        num = 0;
        for (int c = 0 ; c < C ; c++){
            for (int r = 0 ; r < R ; r++){
                for (int s = 0 ; s < S ; s++){
                    //in[index++] = (token_t)(rand()%1000-500); // range from -500 ~ 499
                	in[index++] = (token_t)num++; // range from -50 ~ 49
                    //printf("in[%d]:%d\n", index, in[index-1]);
                }
            }
        }
    }


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
                            gold[gold_index] += in[input_index] * in[weight_index];
                        }
                    }
                }
            }
        }
    }
}


int main(int argc, char * argv[])
{
	int i;
	int n;
	int ndev;
	struct esp_device *espdevs;
	struct esp_device *dev;
	unsigned done;
	unsigned **ptable;
	token_t *mem;
	token_t *gold;
	unsigned errors = 0;
	unsigned coherence;

	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = C*(P+R-1)*(Q+S-1)+M*C*R*S;
		out_words_adj = M*P*Q;
	} else {
		in_words_adj = round_up(C*(P+R-1)*(Q+S-1)+M*C*R*S, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(M*P*Q, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj * (1);
	out_len = out_words_adj * (1);
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset  = in_len;
	mem_size = (out_offset * sizeof(token_t)) + out_size + 100* sizeof(token_t);


	// Search for the device
	printf("Scanning device tree... \n");

	ndev = probe(&espdevs, VENDOR_SLD, SLD_CONV, DEV_NAME);
	if (ndev == 0) {
		printf("conv not found\n");
		return 0;
	}

	for (n = 0; n < ndev; n++) {

		printf("**************** %s.%d ****************\n", DEV_NAME, n);

		dev = &espdevs[n];

		// Check DMA capabilities
		if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
			printf("  -> scatter-gather DMA is disabled. Abort.\n");
			return 0;
		}

		if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK(mem_size)) {
			printf("  -> Not enough TLB entries available. Abort.\n");
			return 0;
		}

		// Allocate memory
		gold = aligned_malloc(out_size);
		mem = aligned_malloc(mem_size);
		printf("  memory buffer base-address = %p\n", mem);

		// Alocate and populate page table
		ptable = aligned_malloc(NCHUNK(mem_size) * sizeof(unsigned *));
		for (i = 0; i < NCHUNK(mem_size); i++)
			ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(token_t))];

		printf("  ptable = %p\n", ptable);
		printf("  nchunk = %lu\n", NCHUNK(mem_size));

#ifndef __riscv
		for (coherence = ACC_COH_NONE; coherence <= ACC_COH_FULL; coherence++) {
#else
		{
			/* TODO: Restore full test once ESP caches are integrated */
			coherence = ACC_COH_NONE;
#endif
			printf("  --------------------\n");
			printf("  Generate input...\n");
			init_buf(mem, gold);

			// Pass common configuration parameters

			iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
			iowrite32(dev, COHERENCE_REG, coherence);

#ifndef __sparc
			iowrite32(dev, PT_ADDRESS_REG, (unsigned long long) ptable);
#else
			iowrite32(dev, PT_ADDRESS_REG, (unsigned) ptable);
#endif
			iowrite32(dev, PT_NCHUNK_REG, NCHUNK(mem_size));
			iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT);

			// Use the following if input and output data are not allocated at the default offsets
			iowrite32(dev, SRC_OFFSET_REG, 0x0);
			iowrite32(dev, DST_OFFSET_REG, 0x0);

			// Pass accelerator-specific configuration parameters
			/* <<--regs-config-->> */
		iowrite32(dev, CONV_MEM_OUTPUT_ADDR_REG, mem_output_addr);
		iowrite32(dev, CONV_MEM_INPUT_ADDR_REG, mem_input_addr);
		iowrite32(dev, CONV_S_REG, S);
		iowrite32(dev, CONV_R_REG, R);
		iowrite32(dev, CONV_Q_REG, Q);
		iowrite32(dev, CONV_P_REG, P);
		iowrite32(dev, CONV_M_REG, M);
		iowrite32(dev, CONV_C_REG, C);

			// Flush (customize coherence model here)
			esp_flush(coherence);

			// Start accelerators
			printf("  Start...\n");
			iowrite32(dev, CMD_REG, CMD_MASK_START);

			// Wait for completion
			done = 0;
			while (!done) {
				done = ioread32(dev, STATUS_REG);
				done &= STATUS_MASK_DONE;
			}
			iowrite32(dev, CMD_REG, 0x0);

			printf("  Done\n");
			printf("  validating...\n");

			/* Validation */
			errors = validate_buf(&mem[out_offset], gold);
			if (errors)
				printf("  ... FAIL\n");
			else
				printf("  ... PASS\n");
		}

		
		
	
	printf("Hardware clock cycle counter : %d\n", mem[in_len+out_len + 0]);
	printf("Hardware clock cycle overflow: %d\n", mem[in_len+out_len + 2]);
	
    int weight_load_total_time = 0;
    int result_write_total_time = 0;
    int kernel_compute_total_time = 0;
    printf("-------Weight Load time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = in_len + out_len + 4 + m*2;
		int t = mem[data_offset];
        printf("Weight Load[%d]: %d\n", m, t);
        weight_load_total_time += t;
    }
    printf("Total weight load time: %d\n", weight_load_total_time);

    printf("-------Kernel Compute time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = in_len + out_len + 4 + 2*M + m*2;
		int t = mem[data_offset];
        printf("Kernel Compute[%d]: %d\n", m, t);
        kernel_compute_total_time += t;
    }
    printf("Total kernel compute time: %d\n", kernel_compute_total_time);

    printf("-------Result Write time-------\n");
    for(int m = 0 ; m < M ; m++){        
        int data_offset = in_len + out_len + 4 + 2*2*M + m*2;
		int t = mem[data_offset];
        printf("Result Write[%d]: %d\n", m, t);
        result_write_total_time += t;
    }
    printf("Total result write time: %d\n", result_write_total_time);
		
    
		aligned_free(ptable);
		aligned_free(mem);
		aligned_free(gold);
	}

	return 0;
}
