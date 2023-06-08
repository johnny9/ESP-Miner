#include "global_state.h"
#include "work_queue.h"
#include "serial.h"
#include "bm1397.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "ASIC_task";

static bm_job ** active_jobs;

void ASIC_task(void * pvParameters)
{

    GlobalState *GLOBAL_STATE = (GlobalState*)pvParameters;

    SERIAL_init();

    BM1397_init();

    uint8_t buf[CHUNK_SIZE];
    memset(buf, 0, 1024);

    uint8_t id = 0;

    active_jobs = malloc(sizeof(bm_job *) * 128);
    GLOBAL_STATE->valid_jobs = malloc(sizeof(uint8_t) * 128);
    for (int i = 0; i < 128; i++) {
		active_jobs[i] = NULL;
        GLOBAL_STATE->valid_jobs[i] = 0;
	}

    uint32_t prev_nonce = 0;

    int baud = BM1397_set_max_baud();
    SERIAL_set_baud(baud);

    SYSTEM_notify_mining_started(&GLOBAL_STATE->SYSTEM_MODULE);
    ESP_LOGI(TAG, "Mining!");
    while (1) {
        bm_job * next_bm_job = (bm_job *) queue_dequeue(&GLOBAL_STATE->ASIC_jobs_queue);
        struct job_packet job;
        // max job number is 128
        id = (id + 4) % 128;
        job.job_id = id;
        job.num_midstates = 1;
        memcpy(&job.starting_nonce, &next_bm_job->starting_nonce, 4);
        memcpy(&job.nbits, &next_bm_job->target, 4);
        memcpy(&job.ntime, &next_bm_job->ntime, 4);
        memcpy(&job.merkle4, next_bm_job->merkle_root + 28, 4);
        memcpy(job.midstate, next_bm_job->midstate, 32);

        if (active_jobs[job.job_id] != NULL) {
            free_bm_job(active_jobs[job.job_id]);
        }
        active_jobs[job.job_id] = next_bm_job;

        pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock);
        GLOBAL_STATE->valid_jobs[job.job_id] = 1;
        pthread_mutex_unlock(&GLOBAL_STATE->valid_jobs_lock);

        SERIAL_clear_buffer();
        BM1397_send_work(&job); //send the job to the ASIC

        //wait for a response
        int received = SERIAL_rx(buf, 9, BM1397_FULLSCAN_MS);

        if (received < 0) {
            ESP_LOGI(TAG, "Error in serial RX");
            continue;
        } else if(received == 0){
            // Didn't find a solution, restart and try again
            continue;
        }

        if(received != 9 || buf[0] != 0xAA || buf[1] != 0x55){
            ESP_LOGI(TAG, "Serial RX invalid %i", received);
            ESP_LOG_BUFFER_HEX(TAG, buf, received);
            continue;
        }

        uint8_t nonce_found = 0;
        uint32_t first_nonce = 0;

        struct nonce_response nonce;
        memcpy((void *) &nonce, buf, sizeof(struct nonce_response));

        if (GLOBAL_STATE->valid_jobs[nonce.job_id] == 0) {
            ESP_LOGI(TAG, "Invalid job nonce found");
        }

        //print_hex((uint8_t *) &nonce.nonce, 4, 4, "nonce: ");
        if (nonce_found == 0) {
            first_nonce = nonce.nonce;
            nonce_found = 1;
        } else if (nonce.nonce == first_nonce) {
            // stop if we've already seen this nonce
            break;
        }

        if (nonce.nonce == prev_nonce) {
            continue;
        } else {
            prev_nonce = nonce.nonce;
        }

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(active_jobs[nonce.job_id], nonce.nonce);

        ESP_LOGI(TAG, "Nonce difficulty %.2f of %d.", nonce_diff, active_jobs[nonce.job_id]->pool_diff);
        
        if (nonce_diff > active_jobs[nonce.job_id]->pool_diff)
        {
            SYSTEM_notify_found_nonce(&GLOBAL_STATE->SYSTEM_MODULE, active_jobs[nonce.job_id]->pool_diff);

            submit_share(GLOBAL_STATE->sock, STRATUM_USER, active_jobs[nonce.job_id]->jobid, active_jobs[nonce.job_id]->ntime,
                            active_jobs[nonce.job_id]->extranonce2, nonce.nonce);
            
        }

    }
}
