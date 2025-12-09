//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_SMOLDOT_H
#define SMOLDOT_ANDROID_SMOLDOT_H

#include "data.h"
#include "utils.h"

#include <memory>
#include <mutex>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

namespace Smoldot {

    class State {
    private:
        static std::unique_ptr<State> instance_;
        static std::mutex instance_mutex_;

        mutable std::mutex event_observer_mutex_;
        Event::Observer* event_observer_ = nullptr;

    public:
        State() = default;
        ~State() noexcept = default;

        State(const State &other) = delete;
        State(State &&other) = delete;
        void operator=(const State &) = delete;
        void operator=(State &&) = delete;

        static State* Get();
        static void Reset();

        void SetEventObserver(Event::Observer* observer);
        void RemoveEventObserver();
        void OnEvent(Event::Instance* type);
    };
}

#ifdef __cplusplus
};
#endif //__cplusplus

#endif //SMOLDOT_ANDROID_SMOLDOT_H
