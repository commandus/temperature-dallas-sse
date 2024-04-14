#include "event-dispatcher.h"

EventDispatcher::EventDispatcher() {
}

void EventDispatcher::wait_event(
    httplib::DataSink *sink
) {
    std::unique_lock<std::mutex> lk(m_);
    int id = id_;
    cv_.wait(lk, [&] { return cid_ == id; });
    sink->write(message_.data(), message_.size());
}

void EventDispatcher::send_event(
    const std::string &message
) {
    std::lock_guard<std::mutex> lk(m_);
    cid_ = id_++;
    message_ = message;
    cv_.notify_all();
}
