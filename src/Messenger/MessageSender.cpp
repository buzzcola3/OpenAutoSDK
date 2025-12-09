#include <Messenger/MessageSender.hpp>

#include <utility>

#include <Channel/IChannel.hpp>
#include <Common/Log.hpp>

namespace aasdk::messenger {

MessageSender::MessageSender(IMessenger::Pointer messenger, boost::asio::io_service& ioService)
    : messenger_(std::move(messenger))
    , ioService_(ioService) {
}

void MessageSender::sendRaw(ChannelId channelId,
                            EncryptionType encryptionType,
                            MessageType messageType,
                            uint16_t messageId,
                            const common::DataConstBuffer& buffer) const {
  if (!this->canSend()) {
    AASDK_LOG(error) << "[MessageSender] Cannot send, messenger not configured.";
    return;
  }

  auto message = std::make_shared<Message>(channelId, encryptionType, messageType);
  message->insertPayload(MessageId(messageId).getData());
  message->insertPayload(buffer);
  this->dispatch(std::move(message));
}

void MessageSender::sendProtobuf(ChannelId channelId,
                                 EncryptionType encryptionType,
                                 MessageType messageType,
                                 uint16_t messageId,
                                 const google::protobuf::Message& payload) const {
  if (!this->canSend()) {
    AASDK_LOG(error) << "[MessageSender] Cannot send, messenger not configured.";
    return;
  }

  auto message = std::make_shared<Message>(channelId, encryptionType, messageType);
  message->insertPayload(MessageId(messageId).getData());
  message->insertPayload(payload);
  this->dispatch(std::move(message));
}

void MessageSender::dispatch(Message::Pointer message) const {
  auto promise = SendPromise::defer(ioService_);
  const auto channelLabel = channelIdToString(message->getChannelId());

  promise->then([]() {}, [channelLabel](const error::Error& e) {
    AASDK_LOG(error) << "[MessageSender] Failed to send on channel " << channelLabel << ": " << e.what();
  });

  messenger_->enqueueSend(std::move(message), std::move(promise));
}

bool MessageSender::canSend() const {
  return messenger_ != nullptr;
}

void MessageSender::registerChannel(::aasdk::channel::IChannel& channel) {
  std::lock_guard<std::mutex> lock(channelMutex_);
  channels_[channel.getId()] = &channel;
}

void MessageSender::unregisterChannel(ChannelId channelId) {
  std::lock_guard<std::mutex> lock(channelMutex_);
  channels_.erase(channelId);
}

::aasdk::channel::IChannel* MessageSender::getChannel(ChannelId channelId) const {
  std::lock_guard<std::mutex> lock(channelMutex_);
  auto iter = channels_.find(channelId);
  if (iter == channels_.end()) {
    return nullptr;
  }
  return iter->second;
}

}
