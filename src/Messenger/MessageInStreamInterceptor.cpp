// Default interceptor implementation; override to short-circuit message
// delivery for specific channel IDs when required.

#include <Messenger/MessageInStreamInterceptor.hpp>
#include <Messenger/Message.hpp>
#include <Messenger/ChannelId.hpp>
#include <Common/Log.hpp>

namespace aasdk::messenger {
  class Message;
  enum class ChannelId;
}

namespace aasdk::messenger::interceptor {

namespace {

bool handleMediaVideo(const ::aasdk::messenger::Message& message) {
  AASDK_LOG(debug) << "[MessageInStreamInterceptor] media video message stub, size="
                   << message.getPayload().size();
  return false;
}

}

bool handleMessage(const ::aasdk::messenger::Message& message) {
  switch (message.getChannelId()) {
    case ::aasdk::messenger::ChannelId::MEDIA_SINK_VIDEO:
      return handleMediaVideo(message);
    default:
      return false;
  }
}

}
