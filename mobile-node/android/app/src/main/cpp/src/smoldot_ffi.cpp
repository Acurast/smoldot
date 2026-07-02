//
// Created by Julia Samol on 17.01.2024.
//

#include "smoldot_ffi.h"

/******** C++ -> Rust ********/

void panic(uint8_t *message_ptr, size_t message_len) {
    auto message = GetString(message_ptr, message_len);
    Smoldot::State::Get()->OnEvent(new Event::Panic(message));
}

void chain_initialized(uint32_t chain_id, uint8_t *error_msg_ptr, size_t error_msg_len) {
    auto error_msg = GetOptString(error_msg_ptr, error_msg_len);
    Smoldot::State::Get()->OnEvent(new Event::ChainInitialized(chain_id, error_msg));
}

void json_rpc_responses_non_empty(uint32_t chain_id) {
    Smoldot::State::Get()->OnEvent(new Event::JsonRpcResponsesNonEmpty(chain_id));
}

void print(
        uint32_t level,
        uint8_t *target_ptr,
        size_t target_len,
        uint8_t *message_ptr,
        size_t message_len
) {
    auto logLevel = Log::GetLevel(level);
    auto target = GetString(target_ptr, target_len);
    auto message = GetString(message_ptr, message_len);

    Smoldot::State::Get()->OnEvent(new Event::Log(logLevel, target, message));
}