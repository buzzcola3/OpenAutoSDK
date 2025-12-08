#pragma once

#include <cstddef>
#include <cstdint>

namespace aasdk::messenger {
  class Message;
}

namespace aasdk::messenger::interceptor {

class MediaSinkVideoMessageHandlers {
public:
  bool handle(const ::aasdk::messenger::Message& message) const;

private:
  void handleChannelOpenRequest(const std::uint8_t* data, std::size_t size) const;
};

}
