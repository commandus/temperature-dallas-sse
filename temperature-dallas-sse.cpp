#include <atomic>
#include <chrono>
#include <condition_variable>
#include <httplib.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

class EventDispatcher {
public:
  EventDispatcher() {
  }

  void wait_event(httplib::DataSink *sink) {
    std::unique_lock<std::mutex> lk(m_);
    int id = id_;
    cv_.wait(lk, [&] { return cid_ == id; });
    sink->write(message_.data(), message_.size());
  }

  void send_event(const std::string &message) {
    std::lock_guard<std::mutex> lk(m_);
    cid_ = id_++;
    message_ = message;
    cv_.notify_all();
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  std::atomic_int id_{0};
  std::atomic_int cid_{-1};
  std::string message_;
};

const auto html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>SSE demo</title>
</head>
<body>
<script>
const ev = new EventSource("event");
ev.onmessage = function(e) {
  console.log('event', e.data);
}
</script>
</body>
</html>
)";

int main(void) {
  EventDispatcher ed;
  httplib::Server svr;

  svr.Get("/", [&](const httplib::Request & /*req*/, httplib::Response &res) {
    res.set_content(html, "text/html");
  });

  svr.Get("/send", [&](const httplib::Request & req, httplib::Response &res) {
    std::stringstream ss;
    ss << "data: ";
    for (auto it(req.params.begin()); it != req.params.end(); it++) {
      ss << it->first << "=" << it->second << ",";
    }
    ss << "\n\n";
    auto s = ss.str();
    ed.send_event(s);
    res.set_content(s, "text/plain");
    std::cout << "Send event: " << s << std::endl;
  });

  svr.Get("/event", [&](const httplib::Request & /*req*/, httplib::Response &res) {
    std::cout << "connected" << std::endl;
    res.set_chunked_content_provider("text/event-stream",
      [&](size_t /*offset*/, httplib::DataSink &sink) {
        ed.wait_event(&sink);
        return true;
      });
  });

  /*
  thread t([&] {
    int id = 0;
    while (true) {
      this_thread::sleep_for(chrono::seconds(1));
      cout << "send event: " << id << std::endl;
      std::stringstream ss;
      ss << "data: " << id << "\n\n";
      ed.send_event(ss.str());
      id++;
    }
  });
  */

  svr.listen("127.0.0.1", 1234);
}
