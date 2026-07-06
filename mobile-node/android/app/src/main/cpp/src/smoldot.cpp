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

    void State::SetEventObserver(std::shared_ptr<Event::Observer> observer) {
        std::lock_guard<std::mutex> lock(event_observer_mutex_);

        event_observer_ = std::move(observer);
    }

    void State::RemoveEventObserver() {
        std::lock_guard<std::mutex> lock(event_observer_mutex_);

        event_observer_ = nullptr;
    }

    void State::OnEvent(Event::Instance *event) {
        switch (event->GetType()) {
            case Event::Type::kPanic: {
                auto panic_event = dynamic_cast<Event::Panic*>(event);
                if (panic_event != nullptr) {
                    std::lock_guard<std::mutex> lock(last_panic_message_mutex_);
                    last_panic_message_ = panic_event->GetMessage();
                }

                break;
            }
            default:
                break;
        }

        // Take a strong reference to the observer under the lock, then release the lock before
        // dispatching. This keeps the observer alive for the duration of the (potentially
        // re-entrant) JNI up-call without holding `event_observer_mutex_` across it.
        std::shared_ptr<Event::Observer> observer;
        {
            std::lock_guard<std::mutex> lock(event_observer_mutex_);
            observer = event_observer_;
        }

        if (observer == nullptr) {
            delete event;
            return;
        }

        observer->OnEvent(event);
        delete event;
    }

    std::string State::GetLastPanicMessage() const {
        std::lock_guard<std::mutex> lock(last_panic_message_mutex_);

        return last_panic_message_;
    }
}
