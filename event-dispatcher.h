#include <mutex>
#include <atomic>
#include <httplib.h>

class EventDispatcher {
public:
    EventDispatcher();
    void wait_event(httplib::DataSink *sink);
    void send_event(const std::string &message);
private:
    std::mutex m_;
    std::condition_variable cv_;
    std::atomic_int id_{0};
    std::atomic_int cid_{-1};
    std::string message_;
};
