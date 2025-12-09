#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <google/protobuf/message.h>
#include <boost/asio/io_service.hpp>
#include <cstdint>

#include <Messenger/IMessenger.hpp>
#include <Messenger/ChannelId.hpp>
#include <Messenger/EncryptionType.hpp>
#include <Messenger/Message.hpp>
#include <Messenger/MessageId.hpp>
#include <Messenger/MessageType.hpp>
#include <Messenger/Promise.hpp>
#include <Common/Data.hpp>
#include <Error/Error.hpp>

namespace aasdk {
namespace channel {
class IChannel;
}
}

namespace aasdk::messenger {

class MessageSender {
public:
  MessageSender(IMessenger::Pointer messenger, boost::asio::io_service& ioService);

  void sendRaw(ChannelId channelId,
               EncryptionType encryptionType,
               MessageType messageType,
               uint16_t messageId,
               const common::DataConstBuffer& buffer) const;

  void sendProtobuf(ChannelId channelId,
                    EncryptionType encryptionType,
                    MessageType messageType,
                    uint16_t messageId,
                    const google::protobuf::Message& payload) const;

  void registerChannel(::aasdk::channel::IChannel& channel);
  void unregisterChannel(ChannelId channelId);
  ::aasdk::channel::IChannel* getChannel(ChannelId channelId) const;

private:
  void dispatch(Message::Pointer message) const;
  bool canSend() const;

  IMessenger::Pointer messenger_;
  boost::asio::io_service& ioService_;
  mutable std::mutex channelMutex_;
  std::unordered_map<ChannelId, ::aasdk::channel::IChannel*> channels_;
};

}
