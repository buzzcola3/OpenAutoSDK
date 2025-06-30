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

#include <Transport/Transport.hpp>
#include "Debug_cfg.hpp"

// To disable logging in this file, comment out the following line
// #define TRANSPORT_LOG_ENABLED

#ifndef TRANSPORT_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif


namespace aasdk {
  namespace transport {

    Transport::Transport(IoContext &ioContext)
        : receiveStrand_(ioContext.get_executor()), sendStrand_(ioContext.get_executor()) {}

    void Transport::receive(size_t size, ReceivePromise::Pointer promise) {
      SDK_LOG_DEBUG("receive()");
      boost::asio::dispatch(receiveStrand_, [this, self = this->shared_from_this(), size, promise = std::move(promise)]() mutable {
        receiveQueue_.emplace_back(std::make_pair(size, std::move(promise)));

        if (receiveQueue_.size() == 1) {
          try {
            SDK_LOG_DEBUG("Distribute received data.");
            this->distributeReceivedData();
          }
          catch (const error::Error &e) {
            // Due to the design of the messaging system, we don't really need to raise an error - debug it is
            SDK_LOG_DEBUG("Reject receive promise.");
            this->rejectReceivePromises(e);
          }
        }
      });
    }

    void Transport::receiveHandler(size_t bytesTransferred) {
      try {
        SDK_LOG_DEBUG("receiveHandler()");
        receivedDataSink_.commit(bytesTransferred);
        this->distributeReceivedData();
      }
      catch (const error::Error &e) {
        // Due to the design of the messaging system, we don't really need to raise an error - debug it is
        SDK_LOG_DEBUG("Rejecting promise.");
        this->rejectReceivePromises(e);
      }
    }

    void Transport::distributeReceivedData() {
      SDK_LOG_DEBUG("distributeReceivedData()");
      for (auto queueElement = receiveQueue_.begin(); queueElement != receiveQueue_.end();) {
        if (receivedDataSink_.getAvailableSize() < queueElement->first) {
          SDK_LOG_DEBUG("Receiving from buffer.");
          auto buffer = receivedDataSink_.fill();
          this->enqueueReceive(std::move(buffer));

          break;
        } else {
          auto data(receivedDataSink_.consume(queueElement->first));
          SDK_LOG_DEBUG("Resolve and clear message.");
          queueElement->second->resolve(std::move(data));
          queueElement = receiveQueue_.erase(queueElement);
        }
      }
    }

    void Transport::rejectReceivePromises(const error::Error &e) {
      for (auto &queueElement: receiveQueue_) {
        queueElement.second->reject(e);
      }

      receiveQueue_.clear();
    }

    void Transport::send(common::Data data, SendPromise::Pointer promise) {
      boost::asio::dispatch(sendStrand_,
          [this, self = this->shared_from_this(), data = std::move(data), promise = std::move(promise)]() mutable {
            sendQueue_.emplace_back(std::make_pair(std::move(data), std::move(promise)));

            if (sendQueue_.size() == 1) {
              this->enqueueSend(sendQueue_.begin());
            }
          });
    }

  }
}
