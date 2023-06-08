#include "stratum_api.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "utils.h"


#define BUFFER_SIZE 1024
static const char *TAG = "stratum_api";

static char *json_rpc_buffer = NULL;
static size_t json_rpc_buffer_size = 0;

static int send_uid = 1;

static void debug_stratum_tx(const char *);

void initialize_stratum_buffer()
{
    json_rpc_buffer = malloc(BUFFER_SIZE);
    json_rpc_buffer_size = BUFFER_SIZE;
    memset(json_rpc_buffer, 0, BUFFER_SIZE);
    if (json_rpc_buffer == NULL)
    {
        printf("Error: Failed to allocate memory for buffer\n");
        exit(1);
    }
}

void cleanup_stratum_buffer()
{
    free(json_rpc_buffer);
}

static void realloc_json_buffer(size_t len)
{
    size_t old, new;

    old = strlen(json_rpc_buffer);
    new = old + len + 1;

    if (new < json_rpc_buffer_size)
    {
        return;
    }

    new = new + (BUFFER_SIZE - (new % BUFFER_SIZE));
    void * new_sockbuf = realloc(json_rpc_buffer, new);

    if (new_sockbuf == NULL)
    {
        fprintf(stderr, "Error: realloc failed in recalloc_sock()\n");
        exit(EXIT_FAILURE);
    }

    json_rpc_buffer = new_sockbuf;
    memset(json_rpc_buffer + old, 0, new - old);
    json_rpc_buffer_size = new;
}

char * receive_jsonrpc_line(int sockfd)
{
    if (json_rpc_buffer == NULL) {
        initialize_stratum_buffer();
    }
    char *line, *tok = NULL;
    char recv_buffer[BUFFER_SIZE];
    int nbytes;
    size_t buflen = 0;

    if (!strstr(json_rpc_buffer, "\n"))
    {
        do
        {
            memset(recv_buffer, 0, BUFFER_SIZE);
            nbytes = recv(sockfd, recv_buffer, BUFFER_SIZE - 1, 0);
            if (nbytes == -1)
            {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            realloc_json_buffer(nbytes);
            strncat(json_rpc_buffer, recv_buffer, nbytes);
        } while (!strstr(json_rpc_buffer, "\n"));
    }
    buflen = strlen(json_rpc_buffer);
    tok = strtok(json_rpc_buffer, "\n");
    line = strdup(tok);
    int len = strlen(line);
    if (buflen > len + 1)
        memmove(json_rpc_buffer, json_rpc_buffer + len + 1, buflen - len + 1);
    else
        strcpy(json_rpc_buffer, "");
    return line;
}

stratum_method parse_stratum_method(const char * stratum_json)
{
    cJSON * json = cJSON_Parse(stratum_json);
    cJSON * method_json = cJSON_GetObjectItem(json, "method");
    stratum_method result = STRATUM_UNKNOWN;
    if (method_json != NULL && cJSON_IsString(method_json)) {
        if (strcmp("mining.notify", method_json->valuestring) == 0) {
            result = MINING_NOTIFY;
        } else if (strcmp("mining.set_difficulty", method_json->valuestring) == 0) {
            result = MINING_SET_DIFFICULTY;
        } else if (strcmp("mining.set_version_mask", method_json->valuestring) == 0) {
            result = MINING_SET_VERSION_MASK;
        }
    } else {
        //parse results
        cJSON * result_json = cJSON_GetObjectItem(json, "result");
        if (result_json != NULL && cJSON_IsBool(result_json)) {
            result = STRATUM_RESULT;
        }
    }

    cJSON_Delete(json);
    return result;
}

uint32_t parse_mining_set_difficulty_message(const char * stratum_json)
{
    cJSON * json = cJSON_Parse(stratum_json);
    cJSON * method = cJSON_GetObjectItem(json, "method");
    if (method != NULL && cJSON_IsString(method)) {
        assert(strcmp("mining.set_difficulty", method->valuestring) == 0);
    }

    cJSON * params = cJSON_GetObjectItem(json, "params");
    uint32_t difficulty = cJSON_GetArrayItem(params, 0)->valueint;

    cJSON_Delete(json);
    return difficulty;
}

uint32_t parse_mining_set_version_mask_message(const char * stratum_json)
{
    cJSON * json = cJSON_Parse(stratum_json);
    cJSON * method = cJSON_GetObjectItem(json, "method");
    if (method != NULL && cJSON_IsString(method)) {
        assert(strcmp("mining.set_version_mask", method->valuestring) == 0);
    }

    cJSON * params = cJSON_GetObjectItem(json, "params");
    uint32_t version_mask = strtoul(cJSON_GetArrayItem(params, 0)->valuestring, NULL, 16);

    cJSON_Delete(json);
    return version_mask;
}

bool parse_stratum_result_message(const char * stratum_json, int16_t * parsed_id)
{
    cJSON * json = cJSON_Parse(stratum_json);
    cJSON * result_json = cJSON_GetObjectItem(json, "result");
    cJSON * id_json = cJSON_GetObjectItem(json, "id");

    if (id_json != NULL && cJSON_IsNumber(id_json)) {
        *parsed_id = id_json->valueint;
    } else {
        *parsed_id = -1;
    }

    bool result = false;
    if (result_json != NULL && cJSON_IsTrue(result_json)) {
        result = true;
    }

    cJSON_Delete(json);
    return result;
}

mining_notify * parse_mining_notify_message(const char * stratum_json, uint32_t difficulty)
{
    cJSON * json = cJSON_Parse(stratum_json);
    cJSON * method = cJSON_GetObjectItem(json, "method");
    if (method != NULL && cJSON_IsString(method)) {
        assert(strcmp("mining.notify", method->valuestring) == 0);
    }

    mining_notify * new_work = malloc(sizeof(mining_notify));
    new_work->difficulty = difficulty;
    cJSON * params = cJSON_GetObjectItem(json, "params");
    new_work->job_id = strdup(cJSON_GetArrayItem(params, 0)->valuestring);
    new_work->prev_block_hash = strdup(cJSON_GetArrayItem(params, 1)->valuestring);
    new_work->coinbase_1 = strdup(cJSON_GetArrayItem(params, 2)->valuestring);
    new_work->coinbase_2 = strdup(cJSON_GetArrayItem(params, 3)->valuestring);

    cJSON * merkle_branch = cJSON_GetArrayItem(params, 4);
    new_work->n_merkle_branches = cJSON_GetArraySize(merkle_branch);
    if (new_work->n_merkle_branches > MAX_MERKLE_BRANCHES) {
        printf("Too many Merkle branches.\n");
        abort();
    }
    new_work->merkle_branches = malloc(HASH_SIZE * new_work->n_merkle_branches);
    for (size_t i = 0; i < new_work->n_merkle_branches; i++) {
        hex2bin(cJSON_GetArrayItem(merkle_branch, i)->valuestring, new_work->merkle_branches + HASH_SIZE * i, HASH_SIZE * 2);
    }

    new_work->version = strtoul(cJSON_GetArrayItem(params, 5)->valuestring, NULL, 16);
    new_work->target = strtoul(cJSON_GetArrayItem(params, 6)->valuestring, NULL, 16);
    new_work->ntime = strtoul(cJSON_GetArrayItem(params, 7)->valuestring, NULL, 16);

    cJSON_Delete(json);
    return new_work;
}

void free_mining_notify(mining_notify * params)
{
    free(params->job_id);
    free(params->prev_block_hash);
    free(params->coinbase_1);
    free(params->coinbase_2);
    free(params->merkle_branches);
    free(params);
}

int parse_stratum_subscribe_result_message(const char * result_json_str,
                                           char ** extranonce,
                                           int * extranonce2_len)
{
    cJSON * root = cJSON_Parse(result_json_str);
    if (root == NULL) {
       ESP_LOGE(TAG, "Unable to parse %s", result_json_str);
       return -1;
    }
    cJSON * result = cJSON_GetObjectItem(root, "result");
    if (result == NULL) {
       ESP_LOGE(TAG, "Unable to parse subscribe result %s", result_json_str);
       return -1;
    }

    cJSON * extranonce2_len_json = cJSON_GetArrayItem(result, 2);
    if (extranonce2_len_json == NULL) {
       ESP_LOGE(TAG, "Unable to parse extranonce2_len: %s", result->valuestring);
       return -1;
    }
    *extranonce2_len = extranonce2_len_json->valueint;

    cJSON * extranonce_json = cJSON_GetArrayItem(result, 1);
    if (extranonce_json == NULL) {
       ESP_LOGE(TAG, "Unable parse extranonce: %s", result->valuestring);
       return -1;
    }
    *extranonce = malloc(strlen(extranonce_json->valuestring) + 1);
    strcpy(*extranonce, extranonce_json->valuestring);

    cJSON_Delete(root);

    return 0;
}

int subscribe_to_stratum(int socket, char ** extranonce, int * extranonce2_len)
{
    // Subscribe
    char subscribe_msg[BUFFER_SIZE];
    sprintf(subscribe_msg, "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": [\"bitaxe v2.2\"]}\n", send_uid++);
    debug_stratum_tx(subscribe_msg);
    write(socket, subscribe_msg, strlen(subscribe_msg));
    char * line;
    line = receive_jsonrpc_line(socket);

    ESP_LOGI(TAG, "Received result %s", line);

    parse_stratum_subscribe_result_message(line, extranonce, extranonce2_len);

    free(line);

    return 1;
}

int suggest_difficulty(int socket, uint32_t difficulty)
{
    char difficulty_msg[BUFFER_SIZE];
    sprintf(difficulty_msg, "{\"id\": %d, \"method\": \"mining.suggest_difficulty\", \"params\": [%d]}\n", send_uid++, difficulty);
    debug_stratum_tx(difficulty_msg);
    write(socket, difficulty_msg, strlen(difficulty_msg));

    /* TODO: fix race condition with first mining.notify message
    char * line;
    line = receive_jsonrpc_line(socket);

    ESP_LOGI(TAG, "Received result %s", line);

    free(line);
    */

    return 1;
}

int auth_to_stratum(int socket, const char * username)
{
    char authorize_msg[BUFFER_SIZE];
    sprintf(authorize_msg, "{\"id\": %d, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"x\"]}\n",
            send_uid++, username);
    debug_stratum_tx(authorize_msg);

    write(socket, authorize_msg, strlen(authorize_msg));

    return 1;
}

void submit_share(int socket, const char * username, const char * jobid,
                 const uint32_t ntime, const char * extranonce_2, const uint32_t nonce)
{
    char submit_msg[BUFFER_SIZE];
    sprintf(submit_msg, "{\"id\": %d, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%08x\", \"%08x\"]}\n",
            send_uid++, username, jobid, extranonce_2, ntime, nonce);
    debug_stratum_tx(submit_msg);
    write(socket, submit_msg, strlen(submit_msg));

}

int should_abandon_work(const char * mining_notify_json_str)
{
    cJSON * root = cJSON_Parse(mining_notify_json_str);
    cJSON * params = cJSON_GetObjectItem(root, "params");
    int value = cJSON_IsTrue(cJSON_GetArrayItem(params, 8));
    cJSON_Delete(root);
    return value;
}

void configure_version_rolling(int socket)
{
    // Configure
    char configure_msg[BUFFER_SIZE * 2];
    sprintf(configure_msg, "{\"id\": %d, \"method\": \"mining.configure\", \"params\": [[\"version-rolling\"], {\"version-rolling.mask\": \"ffffffff\"}]}\n", send_uid++);
    ESP_LOGI(TAG, "tx: %s", configure_msg);
    write(socket, configure_msg, strlen(configure_msg));

    return;
}

static void debug_stratum_tx(const char * msg) {
    ESP_LOGI(TAG, "tx: %s", msg);
}