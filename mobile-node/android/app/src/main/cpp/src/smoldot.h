//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_SMOLDOT_H
#define SMOLDOT_ANDROID_SMOLDOT_H

#include "data.h"
#include "utils.h"

#include <mutex>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

namespace Smoldot {

    class State {
    private:
        State() = default;
        ~State() = default;

        static State* instance_;
        static std::mutex instance_mutex_;

        mutable std::mutex event_observers_mutex_;
        Event::Observer* event_observer_;

    public:
        State(State &other) = delete;
        void operator=(const State &) = delete;

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
