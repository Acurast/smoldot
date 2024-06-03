//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_BINDINGS_H
#define SMOLDOT_ANDROID_BINDINGS_H

#include "utils.h"
#include "smoldot.h"

#include <cstdint>
#include <string>
#include <optional>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

/******** Rust -> C++ ********/

void init(uint32_t max_log_level);

uint32_t add_chain(
        uint8_t *chain_spec_buffer_ptr,
        size_t chain_spec_buffer_len,
        uint8_t *database_content_buffer_ptr,
        size_t database_content_buffer_len,
        uint32_t json_rpc_max_pending_requests,
        uint32_t json_rpc_max_subscriptions,
        uint8_t *potential_relay_chains_buffer_ptr,
        uint32_t potential_relay_chains_buffer_len
);
void remove_chain(uint32_t chain_id);

struct JsonRpcResponseInfo {
    uint8_t *ptr;
    size_t len;
};

uint32_t json_rpc_send(uint8_t *text_buffer_ptr, size_t text_buffer_len, uint32_t chain_id);
JsonRpcResponseInfo *json_rpc_responses_peek(uint32_t chain_id);
void json_rpc_responses_pop(uint32_t chain_id);

/******** C++ -> Rust ********/

void panic(uint8_t *message_ptr, size_t message_len);

void chain_initialized(uint32_t chain_id, uint8_t *error_msg_ptr, size_t error_msg_len);

void json_rpc_responses_non_empty(uint32_t chain_id);

void print(
        uint32_t level,
        uint8_t *target_ptr,
        size_t target_len,
        uint8_t *message_ptr,
        size_t message_len
);

#ifdef __cplusplus
};
#endif //__cplusplus

#endif //SMOLDOT_ANDROID_BINDINGS_H
