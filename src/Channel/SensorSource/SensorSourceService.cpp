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

#include <aap_protobuf/service/sensorsource/SensorMessageId.pb.h>
#include <Channel/SensorSource/ISensorSourceServiceEventHandler.hpp>
#include <Channel/SensorSource/SensorSourceService.hpp>
#include "Debug_cfg.hpp"

// To enable logging in this file, uncomment the following line
// #define SENSOR_SOURCE_SERVICE_LOG_ENABLED

#ifndef SENSOR_SOURCE_SERVICE_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif


namespace aasdk::channel::sensorsource {
  using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

  SensorSourceService::SensorSourceService(Strand &strand, messenger::IMessenger::Pointer messenger)
      : Channel(strand, std::move(messenger), messenger::ChannelId::SENSOR) {

  }

  void SensorSourceService::receive(ISensorSourceServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[SensorSourceService] receive()");
    auto receivePromise = messenger::ReceivePromise::defer(strand_);
    receivePromise->then(
        std::bind(&SensorSourceService::messageHandler, this->shared_from_this(), std::placeholders::_1, eventHandler),
        std::bind(&ISensorSourceServiceEventHandler::onChannelError, eventHandler, std::placeholders::_1));

    messenger_->enqueueReceive(channelId_, std::move(receivePromise));
  }

  void SensorSourceService::sendChannelOpenResponse(const aap_protobuf::service::control::message::ChannelOpenResponse &response,
                                              SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[SensorSourceService] sendChannelOpenResponse()");
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::CONTROL));
    message->insertPayload(
        messenger::MessageId(
            aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void
  SensorSourceService::messageHandler(messenger::Message::Pointer message, ISensorSourceServiceEventHandler::Pointer eventHandler) {

    SDK_LOG_DEBUG("[SensorSourceService] messageHandler()");

    messenger::MessageId messageId(message->getPayload());
    common::DataConstBuffer payload(message->getPayload(), messageId.getSizeOf());

    switch (messageId.getId()) {
      case aap_protobuf::service::sensorsource::SensorMessageId::SENSOR_MESSAGE_REQUEST:
        this->handleSensorStartRequest(payload, std::move(eventHandler));
        break;
      case aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_REQUEST:
        this->handleChannelOpenRequest(payload, std::move(eventHandler));
        break;
      default:
        SDK_LOG_ERROR("[SensorSourceService] Message Id not Handled: ", messageId.getId());
        this->receive(std::move(eventHandler));
        break;
    }
  }

  void
  SensorSourceService::sendSensorEventIndication(const aap_protobuf::service::sensorsource::message::SensorBatch &indication,
                                           SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[SensorSourceService] sendSensorEventIndication()");
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::SPECIFIC));
    message->insertPayload(
        messenger::MessageId(aap_protobuf::service::sensorsource::SensorMessageId::SENSOR_MESSAGE_BATCH).getData());
    message->insertPayload(indication);

    this->send(std::move(message), std::move(promise));
  }

  void
  SensorSourceService::sendSensorStartResponse(
      const aap_protobuf::service::sensorsource::message::SensorStartResponseMessage &response,
      SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[SensorSourceService] sendSensorStartResponse()");
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::SPECIFIC));
    message->insertPayload(
        messenger::MessageId(aap_protobuf::service::sensorsource::SensorMessageId::SENSOR_MESSAGE_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void SensorSourceService::handleSensorStartRequest(const common::DataConstBuffer &payload,
                                               ISensorSourceServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[SensorSourceService] handleSensorStartRequest()");
    aap_protobuf::service::sensorsource::message::SensorRequest request;
    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onSensorStartRequest(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }

  void SensorSourceService::handleChannelOpenRequest(const common::DataConstBuffer &payload,
                                               ISensorSourceServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[SensorSourceService] handleChannelOpenRequest()");
    aap_protobuf::service::control::message::ChannelOpenRequest request;
    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onChannelOpenRequest(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }

}


