// This file is part of aasdk library project.
// Copyright (C) 2018 f1x.studio (Michal Szwaj)
// Copyright (C) 2024 CubeOne (Simon Dean - simon.dean@cubeone.co.uk)
//
// aasdk is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// aasdk is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with aasdk. If not, see <http://www.gnu.org/licenses/>.#include <boost/endian/conversion.hpp>
#include <Error/Error.hpp>
#include <Messenger/Messenger.hpp>
#include "Debug_cfg.hpp"

// To enable logging in this file, uncomment the following line
// #define MESSENGER_LOG_ENABLED

#ifndef MESSENGER_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif

namespace aasdk::messenger {
  using IoContext = boost::asio::io_context;

  Messenger::Messenger(IoContext &ioContext, IMessageInStream::Pointer messageInStream,
                       IMessageOutStream::Pointer messageOutStream)
      : receiveStrand_(ioContext.get_executor()), sendStrand_(ioContext.get_executor()), messageInStream_(std::move(messageInStream)),
        messageOutStream_(std::move(messageOutStream)) {

  }

  void Messenger::enqueueReceive(ChannelId channelId, ReceivePromise::Pointer promise) {
    SDK_LOG_DEBUG("[Messenger::enqueueReceive] Called on channel ", channelIdToString(channelId));

    // enqueueReceive is called from the service channel.
    boost::asio::dispatch(receiveStrand_, [this, self = this->shared_from_this(), channelId, promise = std::move(promise)]() mutable {
      //If there's any messages on the service, resolve. The service will call enqueueReceive again.
      if (!channelReceiveMessageQueue_.empty(channelId)) {
        SDK_LOG_DEBUG("[Messenger::enqueueReceive] Message queue not empty, resolving message first.");
        promise->resolve(std::move(channelReceiveMessageQueue_.pop(channelId)));
      } else {
        SDK_LOG_DEBUG("[Messenger::enqueueReceive] Push promise to queue.");
        channelReceivePromiseQueue_.push(channelId, std::move(promise));

        if (channelReceivePromiseQueue_.size() == 1) {
          SDK_LOG_DEBUG("[Messenger::enqueueReceive] Processing promise.");
          auto inStreamPromise = ReceivePromise::defer(receiveStrand_);
          inStreamPromise->then(
              std::bind(&Messenger::inStreamMessageHandler, this->shared_from_this(), std::placeholders::_1),
              std::bind(&Messenger::rejectReceivePromiseQueue, this->shared_from_this(), std::placeholders::_1));
          messageInStream_->startReceive(std::move(inStreamPromise));
        }
      }
    });
  }

  void Messenger::enqueueSend(Message::Pointer message, SendPromise::Pointer promise) {
    SDK_LOG_INFO("[MESSENGER] TX -> Channel: ", channelIdToString(message->getChannelId()), ", Type: ", static_cast<int>(message->getType()), " Size: ", message->getPayload().size());
    boost::asio::dispatch(sendStrand_,
        [this, self = this->shared_from_this(), message = std::move(message), promise = std::move(promise)]() mutable {
          channelSendPromiseQueue_.emplace_back(std::make_pair(std::move(message), std::move(promise)));

          if (channelSendPromiseQueue_.size() == 1) {
            this->doSend();
          }
        });
  }

  void Messenger::inStreamMessageHandler(Message::Pointer message) {
    SDK_LOG_INFO("[MESSENGER] RX <- Channel: ", channelIdToString(message->getChannelId()), ", Type: ", static_cast<int>(message->getType()), ", Size: ", message->getPayload().size());
    auto channelId = message->getChannelId();

    // If there's a promise on the queue, we resolve the promise with this message....
    if (channelReceivePromiseQueue_.isPending(channelId)) {
      SDK_LOG_DEBUG("[Messenger::inStreamMessageHandler] Pop and resolve message for message queue.");
      channelReceivePromiseQueue_.pop(channelId)->resolve(std::move(message));
    } else {
      SDK_LOG_DEBUG("[Messenger::inStreamMessageHandler] Pushing message to receive queue.");
      // Or we push the message to the Message Queue for when we do get a promise
      channelReceiveMessageQueue_.push(std::move(message));
    }

    if (!channelReceivePromiseQueue_.empty()) {
      SDK_LOG_DEBUG("[Messenger::inStreamMessageHandler] Initiate queue for receiving.");
      auto inStreamPromise = ReceivePromise::defer(receiveStrand_);
      inStreamPromise->then(
          std::bind(&Messenger::inStreamMessageHandler, this->shared_from_this(), std::placeholders::_1),
          std::bind(&Messenger::rejectReceivePromiseQueue, this->shared_from_this(), std::placeholders::_1));
      messageInStream_->startReceive(std::move(inStreamPromise));
    }
  }

  void Messenger::doSend() {
    auto queueElementIter = channelSendPromiseQueue_.begin();
    auto outStreamPromise = SendPromise::defer(sendStrand_);
    outStreamPromise->then(std::bind(&Messenger::outStreamMessageHandler, this->shared_from_this(), queueElementIter),
                           std::bind(&Messenger::rejectSendPromiseQueue, this->shared_from_this(),
                                     std::placeholders::_1));

    messageOutStream_->stream(std::move(queueElementIter->first), std::move(outStreamPromise));
  }

  void Messenger::outStreamMessageHandler(ChannelSendQueue::iterator queueElement) {
    queueElement->second->resolve();
    channelSendPromiseQueue_.erase(queueElement);

    if (!channelSendPromiseQueue_.empty()) {
      this->doSend();
    }
  }

  void Messenger::rejectReceivePromiseQueue(const error::Error &e) {
    while (!channelReceivePromiseQueue_.empty()) {
      channelReceivePromiseQueue_.pop()->reject(e);
    }
  }

  void Messenger::rejectSendPromiseQueue(const error::Error &e) {
    while (!channelSendPromiseQueue_.empty()) {
      auto queueElement(std::move(channelSendPromiseQueue_.front()));
      channelSendPromiseQueue_.pop_front();
      queueElement.second->reject(e);
    }
  }

  void Messenger::stop() {
    boost::asio::dispatch(receiveStrand_, [this, self = this->shared_from_this()]() {
      channelReceiveMessageQueue_.clear();
    });
  }

}

