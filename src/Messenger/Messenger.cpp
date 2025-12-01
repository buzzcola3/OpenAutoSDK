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

#include <algorithm>
#include <boost/endian/conversion.hpp>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <Error/Error.hpp>
#include <Messenger/Messenger.hpp>
#include <Messenger/Message.hpp>
#include <Messenger/MessageId.hpp>
#include "Debug_cfg.hpp"
#include "buzz/autoapp/DebugGlassMonitor.hpp"
#include "buzz/autoapp/ProtoTrace.hpp"
#include "debugglass/widgets/graph.h"
#include "debugglass/widgets/message_monitor.h"
#include "debugglass/widgets/structure.h"
#include "debugglass/widgets/variable.h"

// To enable logging in this file, uncomment the following line
// #define MESSENGER_LOG_ENABLED

#ifndef MESSENGER_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif

namespace aasdk::messenger {
  using IoContext = boost::asio::io_context;

  namespace {
    constexpr char kMessengerWindowName[] = "Messenger";
    constexpr char kMessengerDiagnosticsTab[] = "Diagnostics";

    const char* ToString(::aasdk::messenger::MessageType type) {
      switch (type) {
        case ::aasdk::messenger::MessageType::SPECIFIC:
          return "specific";
        case ::aasdk::messenger::MessageType::CONTROL:
          return "control";
        default:
          return "unknown";
      }
    }

    std::string DescribeMessage(const ::aasdk::messenger::Message& message) {
      std::ostringstream stream;
      stream << ToString(message.getType())
             << " enc=" << (message.getEncryptionType() == ::aasdk::messenger::EncryptionType::ENCRYPTED ? "enc" : "plain")
             << " bytes=" << message.getPayload().size();

      if (message.getPayload().size() >= ::aasdk::messenger::MessageId::getSizeOf()) {
        try {
          ::aasdk::messenger::MessageId messageId(message.getPayload());
          stream << " msgId=0x" << std::hex << std::setw(4) << std::setfill('0') << messageId.getId();
        } catch (...) {
          stream << " msgId=?";
        }
      }

      return stream.str();
    }

    class DebugGlassQueueMonitor {
    public:
      void Update(std::size_t pendingPromises, std::size_t queuedMessages, std::size_t pendingSends) {
        if (!gDebugGlassMonitor.IsRunning()) {
          return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureWidgetsLocked()) {
          return;
        }

        const std::size_t totalPending = pendingPromises + queuedMessages + pendingSends;
        pending_promises_var_->SetValue(pendingPromises);
        queued_messages_var_->SetValue(queuedMessages);
        pending_sends_var_->SetValue(pendingSends);

        UpdateGraph(promises_graph_, pendingPromises);
        UpdateGraph(queued_graph_, queuedMessages);
        UpdateGraph(pending_sends_graph_, pendingSends);
      }

    private:
      bool EnsureWidgetsLocked() {
        if (pending_promises_var_ != nullptr) {
          return true;
        }

        auto& window = gDebugGlassMonitor.windows[kMessengerWindowName];
        auto* tab = window.tabs.find(kMessengerDiagnosticsTab);
        if (tab == nullptr) {
          tab = &window.tabs.Add(kMessengerDiagnosticsTab);
        }

        structure_ = &tab->AddStructure("Queue Status");
        pending_promises_var_ = &structure_->AddVariable("Pending Receive Promises");
        queued_messages_var_ = &structure_->AddVariable("Queued Messages");
        pending_sends_var_ = &structure_->AddVariable("Pending Sends");

        promises_graph_.graph = &tab->AddGraph("Pending Receive Promises (graph)");
        promises_graph_.graph->SetRange(0.0f, promises_graph_.max_range);
        queued_graph_.graph = &tab->AddGraph("Queued Messages (graph)");
        queued_graph_.graph->SetRange(0.0f, queued_graph_.max_range);
        pending_sends_graph_.graph = &tab->AddGraph("Pending Sends (graph)");
        pending_sends_graph_.graph->SetRange(0.0f, pending_sends_graph_.max_range);
        return true;
      }

      struct GraphTracker {
        debugglass::Graph* graph = nullptr;
        float max_range = 32.0f;
      };

      void UpdateGraph(GraphTracker& tracker, std::size_t value) {
        if (tracker.graph == nullptr) {
          return;
        }

        const float float_value = static_cast<float>(value);
        if (float_value >= tracker.max_range) {
          tracker.max_range = std::max(tracker.max_range * 1.5f, float_value + 1.0f);
          tracker.graph->SetRange(0.0f, tracker.max_range);
        }

        tracker.graph->AddValue(float_value);
      }

      std::mutex mutex_;
      debugglass::Structure* structure_ = nullptr;
      debugglass::Variable* pending_promises_var_ = nullptr;
      debugglass::Variable* queued_messages_var_ = nullptr;
      debugglass::Variable* pending_sends_var_ = nullptr;
      GraphTracker promises_graph_;
      GraphTracker queued_graph_;
      GraphTracker pending_sends_graph_;
    };

    DebugGlassQueueMonitor& GetQueueMonitor() {
      static DebugGlassQueueMonitor monitor;
      return monitor;
    }

    class DebugGlassPendingSendInspector {
    public:
      void OnEnqueue(ChannelId channelId, const ::aasdk::messenger::Message& message) {
        if (!gDebugGlassMonitor.IsRunning()) {
          return;
        }

        auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = channels_[channelId];
        stats.entries.push_back({now, DescribeMessage(message)});
        PublishLocked(channelId, stats, now);
      }

      void OnDequeue(ChannelId channelId) {
        if (!gDebugGlassMonitor.IsRunning()) {
          return;
        }

        auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channelId);
        if (it == channels_.end()) {
          return;
        }

        if (!it->second.entries.empty()) {
          it->second.entries.pop_front();
          PublishLocked(channelId, it->second, now);
        }

        if (it->second.entries.empty()) {
          channels_.erase(it);
        }
      }

      void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        channels_.clear();
        if (!gDebugGlassMonitor.IsRunning() || !EnsureMonitorLocked()) {
          return;
        }
        monitor_->UpsertMessage("(cleared)", "pending=0");
      }

    private:
      using Clock = std::chrono::steady_clock;

      struct PendingEntry {
        Clock::time_point enqueued_at;
        std::string description;
      };

      struct ChannelStats {
        std::deque<PendingEntry> entries;
      };

      bool EnsureMonitorLocked() {
        if (monitor_ != nullptr) {
          return true;
        }
        if (!gDebugGlassMonitor.IsRunning()) {
          return false;
        }

        auto& window = gDebugGlassMonitor.windows[kMessengerWindowName];
        auto* tab = window.tabs.find(kMessengerDiagnosticsTab);
        if (tab == nullptr) {
          tab = &window.tabs.Add(kMessengerDiagnosticsTab);
        }

        monitor_ = &tab->AddMessageMonitor("Pending Sends (per channel)");
        return true;
      }

      void PublishLocked(ChannelId channelId, const ChannelStats& stats, Clock::time_point now) {
        if (!EnsureMonitorLocked()) {
          return;
        }

        const std::string label = channelIdToString(channelId);
        if (stats.entries.empty()) {
          monitor_->UpsertMessage(label, "pending=0");
          return;
        }

         const auto age = now - stats.entries.front().enqueued_at;
         const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
        std::ostringstream stream;
         stream << "pending=" << stats.entries.size() << " oldest=" << age_ms << "ms next="
           << stats.entries.front().description;
        monitor_->UpsertMessage(label, stream.str());
      }

      std::mutex mutex_;
      std::unordered_map<ChannelId, ChannelStats> channels_;
      debugglass::MessageMonitor* monitor_ = nullptr;
    };

    DebugGlassPendingSendInspector& GetPendingSendInspector() {
      static DebugGlassPendingSendInspector inspector;
      return inspector;
    }

    class DebugGlassFulfilmentTracker {
    public:
      void TrackRequest(ChannelId channelId) {
        if (!gDebugGlassMonitor.IsRunning()) {
          return;
        }

        auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        pending_[channelId].push_back(now);
      }

      void TrackFulfilment(ChannelId channelId, const Message& message) {
        if (!gDebugGlassMonitor.IsRunning()) {
          return;
        }

        auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(channelId);
        if (it == pending_.end() || it->second.empty()) {
          return;
        }

        auto start = it->second.front();
        it->second.pop_front();
        if (it->second.empty()) {
          pending_.erase(it);
        }

        if (!EnsureMonitorLocked()) {
          return;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        monitor_->UpsertMessage(BuildLabelLocked(channelId, message), elapsed);
      }

      void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.clear();
      }

    private:
      using Clock = std::chrono::steady_clock;
      using PendingQueue = std::deque<Clock::time_point>;

      bool EnsureMonitorLocked() {
        if (monitor_ != nullptr) {
          return true;
        }
        if (!gDebugGlassMonitor.IsRunning()) {
          return false;
        }

        auto& window = gDebugGlassMonitor.windows[kMessengerWindowName];
        auto* tab = window.tabs.find(kMessengerDiagnosticsTab);
        if (tab == nullptr) {
          tab = &window.tabs.Add(kMessengerDiagnosticsTab);
        }

        auto* monitor = tab->FindMessageMonitor("Request -> Response (ms)");
        if (monitor == nullptr) {
          monitor_ = &tab->AddMessageMonitor("Request -> Response (ms)");
        } else {
          monitor_ = const_cast<debugglass::MessageMonitor*>(monitor);
        }

        return monitor_ != nullptr;
      }

      std::string BuildLabelLocked(ChannelId channelId, const Message& message) {
        std::ostringstream stream;
        stream << channelIdToString(channelId) << " | type " << static_cast<int>(message.getType());
        return stream.str();
      }

      std::unordered_map<ChannelId, PendingQueue> pending_;
      debugglass::MessageMonitor* monitor_ = nullptr;
      std::mutex mutex_;
    };

    DebugGlassFulfilmentTracker& GetFulfilmentTracker() {
      static DebugGlassFulfilmentTracker tracker;
      return tracker;
    }
  }

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
        auto queuedMessage = channelReceiveMessageQueue_.pop(channelId);
        if (pendingReceiveMessageCount_ > 0) {
          --pendingReceiveMessageCount_;
        }
        UpdateQueueMonitor();
        promise->resolve(std::move(queuedMessage));
      } else {
        SDK_LOG_DEBUG("[Messenger::enqueueReceive] Push promise to queue.");
        channelReceivePromiseQueue_.push(channelId, std::move(promise));
        ++pendingReceivePromiseCount_;
        UpdateQueueMonitor();
        GetFulfilmentTracker().TrackRequest(channelId);

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
    buzz::autoapp::debug::TraceMessengerMessage(
        buzz::autoapp::debug::ProtoTraceDirection::kTx,
        message->getChannelId(),
        *message);
    boost::asio::dispatch(sendStrand_,
        [this, self = this->shared_from_this(), message = std::move(message), promise = std::move(promise)]() mutable {
          auto channelId = message->getChannelId();
          channelSendPromiseQueue_.emplace_back(std::make_pair(std::move(message), std::move(promise)));
          auto& storedMessage = *channelSendPromiseQueue_.back().first;
          pendingSendChannelOrder_.push_back(channelId);
          GetPendingSendInspector().OnEnqueue(channelId, storedMessage);
          pendingSendCount_.store(channelSendPromiseQueue_.size(), std::memory_order_relaxed);
          UpdateQueueMonitor();

          if (channelSendPromiseQueue_.size() == 1) {
            this->doSend();
          }
        });
  }

  void Messenger::inStreamMessageHandler(Message::Pointer message) {
    SDK_LOG_INFO("[MESSENGER] RX <- Channel: ", channelIdToString(message->getChannelId()), ", Type: ", static_cast<int>(message->getType()), ", Size: ", message->getPayload().size());
    buzz::autoapp::debug::TraceMessengerMessage(
        buzz::autoapp::debug::ProtoTraceDirection::kRx,
        message->getChannelId(),
        *message);
    auto channelId = message->getChannelId();

    // If there's a promise on the queue, we resolve the promise with this message....
    if (channelReceivePromiseQueue_.isPending(channelId)) {
      SDK_LOG_DEBUG("[Messenger::inStreamMessageHandler] Pop and resolve message for message queue.");
      GetFulfilmentTracker().TrackFulfilment(channelId, *message);
      auto promise = channelReceivePromiseQueue_.pop(channelId);
      if (pendingReceivePromiseCount_ > 0) {
        --pendingReceivePromiseCount_;
      }
      UpdateQueueMonitor();
      promise->resolve(std::move(message));
    } else {
      SDK_LOG_DEBUG("[Messenger::inStreamMessageHandler] Pushing message to receive queue.");
      // Or we push the message to the Message Queue for when we do get a promise
      channelReceiveMessageQueue_.push(std::move(message));
      ++pendingReceiveMessageCount_;
      UpdateQueueMonitor();
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
    if (!pendingSendChannelOrder_.empty()) {
      auto channelId = pendingSendChannelOrder_.front();
      pendingSendChannelOrder_.pop_front();
      GetPendingSendInspector().OnDequeue(channelId);
    }
    pendingSendCount_.store(channelSendPromiseQueue_.size(), std::memory_order_relaxed);
    UpdateQueueMonitor();

    if (!channelSendPromiseQueue_.empty()) {
      this->doSend();
    }
  }

  void Messenger::rejectReceivePromiseQueue(const error::Error &e) {
    GetFulfilmentTracker().Reset();
    while (!channelReceivePromiseQueue_.empty()) {
      channelReceivePromiseQueue_.pop()->reject(e);
    }
    pendingReceivePromiseCount_ = 0;
    UpdateQueueMonitor();
  }

  void Messenger::rejectSendPromiseQueue(const error::Error &e) {
    while (!channelSendPromiseQueue_.empty()) {
      auto queueElement(std::move(channelSendPromiseQueue_.front()));
      channelSendPromiseQueue_.pop_front();
      queueElement.second->reject(e);
    }
    pendingSendChannelOrder_.clear();
    GetPendingSendInspector().Reset();
    pendingSendCount_.store(0, std::memory_order_relaxed);
    UpdateQueueMonitor();
  }

  void Messenger::stop() {
    boost::asio::dispatch(receiveStrand_, [this, self = this->shared_from_this()]() {
      channelReceiveMessageQueue_.clear();
      pendingReceiveMessageCount_ = 0;
      UpdateQueueMonitor();
      GetFulfilmentTracker().Reset();
    });

    boost::asio::dispatch(sendStrand_, [this, self = this->shared_from_this()]() {
      while (!channelSendPromiseQueue_.empty()) {
        auto queueElement(std::move(channelSendPromiseQueue_.front()));
        channelSendPromiseQueue_.pop_front();
        queueElement.second->reject(error::Error(error::ErrorCode::OPERATION_ABORTED));
      }
      pendingSendChannelOrder_.clear();
      GetPendingSendInspector().Reset();
      pendingSendCount_.store(0, std::memory_order_relaxed);
      UpdateQueueMonitor();
    });
  }

  void Messenger::UpdateQueueMonitor() {
    GetQueueMonitor().Update(
        pendingReceivePromiseCount_,
        pendingReceiveMessageCount_,
        pendingSendCount_.load(std::memory_order_relaxed));
  }

}

