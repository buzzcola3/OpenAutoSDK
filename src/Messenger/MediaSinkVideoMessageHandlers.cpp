#include <Messenger/MediaSinkVideoMessageHandlers.hpp>

#include <Messenger/Message.hpp>
#include <Messenger/MessageId.hpp>
#include <Messenger/MessageSender.hpp>
#include <Messenger/MessageType.hpp>
#include <Common/Log.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <aap_protobuf/service/control/message/ChannelOpenRequest.pb.h>
#include <aap_protobuf/service/control/ControlMessageType.pb.h>
#include <aap_protobuf/service/control/message/ChannelOpenResponse.pb.h>
#include <aap_protobuf/shared/MessageStatus.pb.h>
#include <aap_protobuf/service/media/shared/message/Setup.pb.h>
#include <aap_protobuf/service/media/shared/message/Start.pb.h>
#include <aap_protobuf/service/media/shared/message/Stop.pb.h>
#include <aap_protobuf/service/media/sink/MediaMessageId.pb.h>
#include <aap_protobuf/service/media/video/message/VideoFocusRequestNotification.pb.h>

namespace {

using Control = aap_protobuf::service::control::message::ControlMessageType;
using Media = aap_protobuf::service::media::sink::MediaMessageId;

template<typename Proto>
void decodeAndLogPayload(const std::uint8_t* data, std::size_t size, const char* label) {
  if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    AASDK_LOG(error) << "[MediaSinkVideoMessageHandlers] " << label
                     << " payload too large for ParseFromArray, bytes=" << size;
    return;
  }

  Proto message;
  if (!message.ParseFromArray(data, static_cast<int>(size))) {
    AASDK_LOG(error) << "[MediaSinkVideoMessageHandlers] Failed to parse " << label
                     << " payload, bytes=" << size;
    return;
  }

  AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] " << label << ": "
                   << message.ShortDebugString();
}

}

namespace aasdk::messenger::interceptor {

bool MediaSinkVideoMessageHandlers::handle(const ::aasdk::messenger::Message& message) const {
  const auto& rawPayload = message.getPayload();
  AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] media video message received, size="
                   << rawPayload.size();

  if (rawPayload.size() <= ::aasdk::messenger::MessageId::getSizeOf()) {
    AASDK_LOG(error) << "[MediaSinkVideoMessageHandlers] media video payload too small";
    return false;
  }

  ::aasdk::messenger::MessageId messageId(rawPayload);
  const auto payloadSize = rawPayload.size() - ::aasdk::messenger::MessageId::getSizeOf();
  const auto* payloadData = rawPayload.data() + ::aasdk::messenger::MessageId::getSizeOf();

  bool handled = false;
  switch (messageId.getId()) {
    case Control::MESSAGE_CHANNEL_OPEN_REQUEST:
      handled = handleChannelOpenRequest(message, payloadData, payloadSize);
      break;
    case Media::MEDIA_MESSAGE_SETUP:
      decodeAndLogPayload<aap_protobuf::service::media::shared::message::Setup>(
          payloadData, payloadSize, "MediaSetup");
      break;
    case Media::MEDIA_MESSAGE_START:
      decodeAndLogPayload<aap_protobuf::service::media::shared::message::Start>(
          payloadData, payloadSize, "MediaStart");
      break;
    case Media::MEDIA_MESSAGE_STOP:
      decodeAndLogPayload<aap_protobuf::service::media::shared::message::Stop>(
          payloadData, payloadSize, "MediaStop");
      break;
    case Media::MEDIA_MESSAGE_VIDEO_FOCUS_REQUEST:
      decodeAndLogPayload<aap_protobuf::service::media::video::message::VideoFocusRequestNotification>(
          payloadData, payloadSize, "VideoFocusRequest");
      break;
    case Media::MEDIA_MESSAGE_CODEC_CONFIG:
      AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] codec configuration blob size="
                       << payloadSize << " bytes";
      break;
    case Media::MEDIA_MESSAGE_DATA:
      AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] media data frame size="
                       << payloadSize << " bytes";
      break;
    default:
      AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] media video message id="
                       << messageId.getId() << " not explicitly decoded.";
      break;
  }

  return handled;
}

bool MediaSinkVideoMessageHandlers::handleChannelOpenRequest(const ::aasdk::messenger::Message& message,
                                                             const std::uint8_t* data,
                                                             std::size_t size) const {
  aap_protobuf::service::control::message::ChannelOpenRequest request;
  if (!request.ParseFromArray(data, static_cast<int>(size))) {
    AASDK_LOG(error) << "[MediaSinkVideoMessageHandlers] Failed to parse ChannelOpenRequest payload";
    return false;
  }

  AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] ChannelOpenRequest: "
                   << request.ShortDebugString();

  aap_protobuf::service::control::message::ChannelOpenResponse response;
  response.set_status(aap_protobuf::shared::MessageStatus::STATUS_SUCCESS);

  AASDK_LOG(debug) << "[MediaSinkVideoMessageHandlers] Constructed ChannelOpenResponse: "
                   << response.ShortDebugString();

  if (sender_ != nullptr) {
    sender_->sendProtobuf(message.getChannelId(),
                          message.getEncryptionType(),
                          ::aasdk::messenger::MessageType::CONTROL,
                          Control::MESSAGE_CHANNEL_OPEN_RESPONSE,
                          response);
    return true;
  } else {
    AASDK_LOG(error) << "[MediaSinkVideoMessageHandlers] MessageSender not configured; cannot send response.";
    return false;
  }
}

void MediaSinkVideoMessageHandlers::setMessageSender(
    std::shared_ptr<::aasdk::messenger::MessageSender> sender) {
  sender_ = std::move(sender);
}

}
