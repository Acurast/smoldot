//
// Created by Julia Samol on 17.01.2024.
//

#include "smoldot.h"

namespace Smoldot {
    std::unique_ptr<State> State::instance_;
    std::mutex State::instance_mutex_;

    State *State::Get() {
        std::lock_guard<std::mutex> lock(instance_mutex_);

        if (!instance_) {
            instance_ = std::make_unique<State>();
        }

        return instance_.get();
    }

    void State::Reset() {
        std::lock_guard<std::mutex> lock(instance_mutex_);

        if (instance_) {
            instance_->RemoveEventObserver();
        }

        instance_.reset();
    }

    void State::SetEventObserver(Event::Observer *observer) {
        std::lock_guard<std::mutex> lock(event_observer_mutex_);

        event_observer_ = observer;
    }

    void State::RemoveEventObserver() {
        std::lock_guard<std::mutex> lock(event_observer_mutex_);

        event_observer_ = nullptr;
    }

    void State::OnEvent(Event::Instance *event) {
        std::lock_guard<std::mutex> lock(event_observer_mutex_);

        if (event_observer_ == nullptr) {
            return;
        }

        event_observer_->OnEvent(event);
        delete event;
    }
}
