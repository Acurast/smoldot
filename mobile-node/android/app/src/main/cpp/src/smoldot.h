//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_SMOLDOT_H
#define SMOLDOT_ANDROID_SMOLDOT_H

#include "data.h"
#include "utils.h"

#include <memory>
#include <mutex>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

namespace Smoldot {

    class State {
    private:
        static std::unique_ptr<State> instance_;
        static std::mutex instance_mutex_;

        mutable std::mutex event_observer_mutex_;
        std::shared_ptr<Event::Observer> event_observer_ = nullptr;

        mutable std::mutex last_panic_message_mutex_;
        std::string last_panic_message_;

    public:
        State() = default;
        ~State() noexcept = default;

        State(const State &other) = delete;
        State(State &&other) = delete;
        void operator=(const State &) = delete;
        void operator=(State &&) = delete;

        static State* Get();
        static void Reset();

        void SetEventObserver(std::shared_ptr<Event::Observer> observer);
        void RemoveEventObserver();
        void OnEvent(Event::Instance* type);

        // Returns the message of the last panic event, or an empty string if no panic occurred.
        std::string GetLastPanicMessage() const;
    };
}

#ifdef __cplusplus
};
#endif //__cplusplus

#endif //SMOLDOT_ANDROID_SMOLDOT_H
