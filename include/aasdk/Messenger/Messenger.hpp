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
// along with aasdk. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <boost/asio.hpp>
#include <atomic>
#include <cstddef>
#include <deque>
#include <list>
#include <Messenger/IMessenger.hpp>
#include <Messenger/IMessageInStream.hpp>
#include <Messenger/IMessageOutStream.hpp>
#include <Messenger/ChannelReceiveMessageQueue.hpp>
#include <Messenger/ChannelReceivePromiseQueue.hpp>


namespace aasdk {
  namespace messenger {
    using IoContext = boost::asio::io_context;
    using Strand = boost::asio::strand<IoContext::executor_type>;

    class Messenger : public IMessenger, public std::enable_shared_from_this<Messenger>, boost::noncopyable {
    public:
      Messenger(IoContext &ioContext, IMessageInStream::Pointer messageInStream,
                IMessageOutStream::Pointer messageOutStream);

      void enqueueReceive(ChannelId channelId, ReceivePromise::Pointer promise) override;

      void enqueueSend(Message::Pointer message, SendPromise::Pointer promise) override;

      void stop() override;

    private:
      using std::enable_shared_from_this<Messenger>::shared_from_this;
      typedef std::list<std::pair<Message::Pointer, SendPromise::Pointer>> ChannelSendQueue;

      void doSend();

      void inStreamMessageHandler(Message::Pointer message);

      void outStreamMessageHandler(ChannelSendQueue::iterator queueElement);

      void rejectReceivePromiseQueue(const error::Error &e);

      void rejectSendPromiseQueue(const error::Error &e);
      void UpdateQueueMonitor();

      Strand receiveStrand_;
      Strand sendStrand_;
      IMessageInStream::Pointer messageInStream_;
      IMessageOutStream::Pointer messageOutStream_;

      ChannelReceivePromiseQueue channelReceivePromiseQueue_;
      ChannelReceiveMessageQueue channelReceiveMessageQueue_;
      ChannelSendQueue channelSendPromiseQueue_;
      std::size_t pendingReceivePromiseCount_ = 0;
      std::size_t pendingReceiveMessageCount_ = 0;
      std::atomic<std::size_t> pendingSendCount_{0};
      std::deque<ChannelId> pendingSendChannelOrder_;

    };

  }
}
