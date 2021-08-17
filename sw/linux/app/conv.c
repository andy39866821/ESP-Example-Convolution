// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include "libesp.h"
#include "cfg.h"

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned size;

/* User-defined code */
static int validate_buffer(token_t *out, token_t *gold)
{
	int i;
	int j;
	unsigned errors = 0;

	for (i = 0; i < 1; i++)
		for (j = 0; j < M*P*Q; j++)
			if (gold[i * out_words_adj + j] != out[i * out_words_adj + j]){
				printf("Error[%d]:\n", i * out_words_adj + j);
				printf("	Correct: %d\n", gold[i * out_words_adj + j]);
				printf("	Result : %d\n", out[i * out_words_adj + j]);
				errors++;
			}

	return errors;
}


/* User-defined code */
static void init_buffer(token_t *in, token_t * gold)
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
                //printf("in[%d]:%d\n", index-1, in[index-1]);
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
                    in[index++] = (token_t)num++; // range from -500 ~ 499
                    //printf("in[%d]:%d\n", index-1, in[index-1]);
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


/* User-defined code */
static void init_parameters()
{
	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = C*(P+R-1)*(Q+S-1)+M*C*R*S;
		out_words_adj = M*P*Q;
	} else {
		in_words_adj = round_up(C*(P+R-1)*(Q+S-1)+M*C*R*S, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(M*P*Q, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj * (1);
	out_len =  out_words_adj * (1);
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset = in_len;
	size = (out_offset * sizeof(token_t)) + out_size + 100 * sizeof(token_t);
}


int main(int argc, char **argv)
{
	int errors;

	token_t *gold;
	token_t *buf;

	init_parameters();

	buf = (token_t *) esp_alloc(size);
	cfg_000[0].hw_buf = buf;
    
	gold = malloc(out_size);

	init_buffer(buf, gold);

	printf("\n====== %s ======\n\n", cfg_000[0].devname);
	/* <<--print-params-->> */
	printf("  .mem_output_addr = %d\n", mem_output_addr);
	printf("  .mem_input_addr = %d\n", mem_input_addr);
	printf("  .S = %d\n", S);
	printf("  .R = %d\n", R);
	printf("  .Q = %d\n", Q);
	printf("  .P = %d\n", P);
	printf("  .M = %d\n", M);
	printf("  .C = %d\n", C);
	printf("\n  ** START **\n");

	esp_run(cfg_000, NACC);

	printf("\n  ** DONE **\n");

	printf("Hardware clock cycle counter : %d\n", buf[in_len+out_len + 0]);
	printf("Hardware clock cycle overflow: %d\n", buf[in_len+out_len + 1]);

    int weight_load_total_time = 0;
    int result_write_total_time = 0;
    int kernel_compute_total_time = 0;
    printf("-------Weight Load time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = in_len + out_len + 4 + m*2;
		int t = buf[data_offset];
        printf("Weight Load[%d]: %d\n", m, t);
        weight_load_total_time += t;
    }
    printf("Total weight load time: %d\n", weight_load_total_time);

    printf("-------Kernel Compute time-------\n");
    for(int m = 0 ; m < M ; m++){
        int data_offset = in_len + out_len + 4 + 2*M + m*2;
		int t = buf[data_offset];
        printf("Kernel Compute[%d]: %d\n", m, t);
        kernel_compute_total_time += t;
    }
    printf("Total kernel compute time: %d\n", kernel_compute_total_time);

    printf("-------Result Write time-------\n");
    for(int m = 0 ; m < M ; m++){        
        int data_offset = in_len + out_len + 4 + 2*2*M + m*2;
		int t = buf[data_offset];
        printf("Result Write[%d]: %d\n", m, t);
        result_write_total_time += t;
    }
	errors = validate_buffer(&buf[out_offset], gold);

	free(gold);
	esp_free(buf);

	if (!errors)
		printf("+ Test PASSED\n");
	else
		printf("+ Test FAILED\n");

	printf("\n====== %s ======\n\n", cfg_000[0].devname);
	
		
	return errors;
}
