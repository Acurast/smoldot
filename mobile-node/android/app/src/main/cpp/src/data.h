//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_DATA_H
#define SMOLDOT_ANDROID_DATA_H

#include <optional>
#include <string>

/******** Log ********/

namespace Log {

    enum Level {
        kError = 1,
        kWarn = 2,
        kInfo = 3,
        kDebug = 4,
        kTrace = 5,
    };

    Level GetLevel(uint32_t level);
}

/******** Event ********/

namespace Event {

    enum Type {
        kLog = 100,
        kPanic = 101,
        kChainInitialized = 200,
        kJsonRpcResponsesNonEmpty = 300,
    };

    class Instance {
    private:
        Type type_;

    public:
        Instance(Type type);
        virtual ~Instance() { };

        Type GetType() const;
    };

    class Log : public Instance {
    private:
        ::Log::Level level_;
        std::string target_;
        std::string message_;

    public:
        Log(::Log::Level level, std::string target, std::string message);

        ::Log::Level GetLevel() const;
        std::string &GetTarget();
        std::string &GetMessage();
    };

    class Panic : public Instance {
    private:
        std::string message_;

    public:
        Panic(std::string message);

        std::string &GetMessage();
    };

    class ChainInitialized : public Instance {
    private:
        uint32_t chain_id_;
        std::optional<std::string> error_;

    public:
        ChainInitialized(uint32_t chain_id, std::optional<std::string> error);

        uint32_t GetChainId();
        std::optional<std::string> &GetError();
    };

    class JsonRpcResponsesNonEmpty : public Instance {
    private:
        uint32_t chain_id_;

    public:
        JsonRpcResponsesNonEmpty(uint32_t chain_id);

        uint32_t GetChainId() const;
    };

    class Observer {
    public:
        virtual ~Observer() { };
        virtual void OnEvent(Instance* event) = 0;
    };
}

#endif //SMOLDOT_ANDROID_DATA_H
