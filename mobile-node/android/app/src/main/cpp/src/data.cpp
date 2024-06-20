//
// Created by Julia Samol on 17.01.2024.
//

#include "data.h"

/******** Log ********/

namespace Log {
    Level GetLevel(uint32_t level) {
        switch (level) {
            case kError:
                return kError;
            case kWarn:
                return kWarn;
            case kInfo:
                return kInfo;
            case kDebug:
                return kDebug;
            case kTrace:
                return kTrace;
            default:
                return kError;
        }
    }
}

/******** Event ********/

namespace Event {

    Instance::Instance(Type type) {
        type_ = type;
    }

    Type Instance::GetType() const {
        return type_;
    }

    /******** Event::Log ********/

    Log::Log(::Log::Level level, std::string target, std::string message) : Instance(
            kLog) {
        level_ = level;
        target_ = std::move(target);
        message_ = std::move(message);
    }

    ::Log::Level Log::GetLevel() const {
        return level_;
    }

    std::string &Log::GetTarget() {
        return target_;
    }

    std::string &Log::GetMessage() {
        return message_;
    }

    /******** Event::Panic ********/

    Panic::Panic(std::string message) : Instance(kPanic) {
        message_ = std::move(message);
    }

    std::string &Panic::GetMessage() {
        return message_;
    }

    /******** Event::ChainInitialized ********/

    ChainInitialized::ChainInitialized(uint32_t chain_id, std::optional<std::string> error) : Instance(kChainInitialized) {
        chain_id_ = chain_id;
        error_ = std::move(error);
    }

    uint32_t ChainInitialized::GetChainId() {
        return chain_id_;
    }

    std::optional<std::string> &ChainInitialized::GetError() {
        return error_;
    }

    /******** Event::JsonRpcResponsesNonEmpty ********/

    JsonRpcResponsesNonEmpty::JsonRpcResponsesNonEmpty(uint32_t chain_id) : Instance(
            kJsonRpcResponsesNonEmpty) {
        chain_id_ = chain_id;
    }

    uint32_t JsonRpcResponsesNonEmpty::GetChainId() const {
        return chain_id_;
    }
}