// Default interceptor implementation; override to short-circuit message
// delivery for specific channel IDs when required.

#include <Messenger/MessageInStreamInterceptor.hpp>
#include <Messenger/MediaSinkVideoMessageHandlers.hpp>
#include <Messenger/Message.hpp>
#include <Messenger/ChannelId.hpp>

namespace aasdk::messenger::interceptor {

namespace {

const MediaSinkVideoMessageHandlers MEDIA_SINK_VIDEO_HANDLERS;

}

bool handleMessage(const ::aasdk::messenger::Message& message) {
  switch (message.getChannelId()) {
    case ::aasdk::messenger::ChannelId::MEDIA_SINK_VIDEO:
      return MEDIA_SINK_VIDEO_HANDLERS.handle(message);
    default:
      return false;
  }
}

}
