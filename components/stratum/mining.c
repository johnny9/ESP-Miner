#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "mining.h"
#include "utils.h"
#include "../../main/pretty.h"

void free_bm_job(bm_job * job)
{
    free(job->jobid);
    free(job->extranonce2);
    free(job);
}

char * construct_coinbase_tx(const char * coinbase_1, const char * coinbase_2,
                             const char * extranonce, const char * extranonce_2)
{
    int coinbase_tx_len = strlen(coinbase_1) + strlen(coinbase_2)
        + strlen(extranonce) + strlen(extranonce_2) + 1;

    char * coinbase_tx = malloc(coinbase_tx_len);
    strcpy(coinbase_tx, coinbase_1);
    strcat(coinbase_tx, extranonce);
    strcat(coinbase_tx, extranonce_2);
    strcat(coinbase_tx, coinbase_2);
    coinbase_tx[coinbase_tx_len - 1] = '\0';

    return coinbase_tx;
}

char * calculate_merkle_root_hash(const char * coinbase_tx, const uint8_t merkle_branches[][32], const int num_merkle_branches)
{
    size_t coinbase_tx_bin_len = strlen(coinbase_tx) / 2;
    uint8_t * coinbase_tx_bin = malloc(coinbase_tx_bin_len);
    hex2bin(coinbase_tx, coinbase_tx_bin, coinbase_tx_bin_len);

    uint8_t both_merkles[64];
    uint8_t * new_root = double_sha256_bin(coinbase_tx_bin, coinbase_tx_bin_len);
    free(coinbase_tx_bin);
    memcpy(both_merkles, new_root, 32);
    free(new_root);
    for (int i = 0; i < num_merkle_branches; i++)
    {
        memcpy(both_merkles + 32, merkle_branches[i], 32);
        uint8_t * new_root = double_sha256_bin(both_merkles, 64);
        memcpy(both_merkles, new_root, 32);
        free(new_root);
    }

    char * merkle_root_hash = malloc(65);
    bin2hex(both_merkles, 32, merkle_root_hash, 65);
    return merkle_root_hash;
}

bm_job construct_bm_job(uint32_t version, const char * prev_block_hash, const char * merkle_root,
                        uint32_t ntime, uint32_t target)
{
    bm_job new_job;
    new_job.starting_nonce = 0;
    new_job.target = target;
    new_job.ntime = ntime;

    uint8_t merkle_root_bin[32];
    uint8_t prev_block_bin[32];
    hex2bin(merkle_root, merkle_root_bin, 32);
    hex2bin(prev_block_hash, prev_block_bin, 32);

    uint8_t midstate_data[64];

    //print the header
    //printf("header: %08x%s%s%08x%08x000000000000008000000000000000000000000000000000000000000000000000000000\n", version, prev_block_hash, merkle_root, ntime, target);

    memcpy(midstate_data, &version, 4); //copy version
    swap_endian_words(prev_block_hash, midstate_data + 4); //copy prev_block_hash
    memcpy(midstate_data + 36, merkle_root_bin, 28); //copy merkle_root
    // printf("midstate_data: ");
    // prettyHex(midstate_data, 64);
    // printf("\n");

    midstate_sha256_bin(midstate_data, 64, new_job.midstate); //make the midstate hash
    reverse_bytes(new_job.midstate, 32); //reverse the midstate bytes for the BM job packet

    memcpy(&new_job.merkle_root_end, merkle_root_bin + 28, 4);

    return new_job;
}

char * extranonce_2_generate(uint32_t extranonce_2, uint32_t length)
{
    char * extranonce_2_str = malloc(length * 2 + 1);
    bin2hex((uint8_t *) &extranonce_2, length, extranonce_2_str, length * 2 + 1);
    return extranonce_2_str;
}