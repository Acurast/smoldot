//
// Created by Julia Samol on 17.01.2024.
//

#include "smoldot.h"

namespace Smoldot {
    State *State::instance_ = nullptr;
    std::mutex State::instance_mutex_;

    State *State::Get() {
        std::lock_guard<std::mutex> lock(instance_mutex_);

        if (instance_ == nullptr) {
            instance_ = new State();
        }

        return instance_;
    }

    void State::Reset() {
        std::lock_guard<std::mutex> lock(instance_mutex_);

        if (instance_ == nullptr) {
            instance_->RemoveEventObserver();
        }

        delete instance_;
        instance_ = nullptr;
    }

    void State::SetEventObserver(Event::Observer *observer) {
        std::lock_guard<std::mutex> lock(event_observers_mutex_);

        event_observer_ = observer;
    }

    void State::RemoveEventObserver() {
        std::lock_guard<std::mutex> lock(event_observers_mutex_);

        event_observer_ = nullptr;
    }

    void State::OnEvent(Event::Instance *event) {
        if (event_observer_ == nullptr) {
            return;
        }

        event_observer_->OnEvent(event);
        delete event;
    }
}
