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

#include <aap_protobuf/service/bluetooth/BluetoothMessageId.pb.h>
#include "Channel/Bluetooth/IBluetoothServiceEventHandler.hpp"
#include "Channel/Bluetooth/BluetoothService.hpp"
#include "Debug_cfg.hpp"

// To enable logging in this file, uncomment the following line
// #define BLUETOOTH_SERVICE_LOG_ENABLED

#ifndef BLUETOOTH_SERVICE_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif

namespace aasdk::channel::bluetooth {

  using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

  BluetoothService::BluetoothService(Strand &strand,
                                     messenger::IMessenger::Pointer messenger)
      : Channel(strand, std::move(messenger), messenger::ChannelId::BLUETOOTH) {

  }

  void BluetoothService::receive(IBluetoothServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[BluetoothService] receive()");

    auto receivePromise = messenger::ReceivePromise::defer(strand_);
    receivePromise->then(
        std::bind(&BluetoothService::messageHandler, this->shared_from_this(), std::placeholders::_1,
                  eventHandler),
        std::bind(&IBluetoothServiceEventHandler::onChannelError, eventHandler, std::placeholders::_1));

    messenger_->enqueueReceive(channelId_, std::move(receivePromise));
  }

  void BluetoothService::sendChannelOpenResponse(const aap_protobuf::service::control::message::ChannelOpenResponse &response,
                                                 SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[BluetoothService] sendChannelOpenResponse()");

    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::CONTROL));
    message->insertPayload(
        messenger::MessageId(
            aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void BluetoothService::sendBluetoothPairingResponse(
      const aap_protobuf::service::bluetooth::message::BluetoothPairingResponse &response,
      SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[BluetoothService] sendBluetoothPairingResponse()");

    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::SPECIFIC));
    message->insertPayload(
        messenger::MessageId(
            aap_protobuf::service::bluetooth::BluetoothMessageId::BLUETOOTH_MESSAGE_PAIRING_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void BluetoothService::sendBluetoothAuthenticationData(
      const aap_protobuf::service::bluetooth::message::BluetoothAuthenticationData &response,
      SendPromise::Pointer promise) {

    SDK_LOG_DEBUG("[BluetoothService] sendBluetoothAuthenticationData()");

    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::SPECIFIC));
    message->insertPayload(
        messenger::MessageId(
            aap_protobuf::service::bluetooth::BluetoothMessageId::BLUETOOTH_MESSAGE_AUTHENTICATION_DATA).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void BluetoothService::messageHandler(messenger::Message::Pointer message,
                                        IBluetoothServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[BluetoothService] messageHandler()");

    messenger::MessageId messageId(message->getPayload());
    common::DataConstBuffer payload(message->getPayload(), messageId.getSizeOf());

    switch (messageId.getId()) {
      case aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_REQUEST:
        this->handleChannelOpenRequest(payload, std::move(eventHandler));
        break;
      case aap_protobuf::service::bluetooth::BluetoothMessageId::BLUETOOTH_MESSAGE_PAIRING_REQUEST:
        this->handleBluetoothPairingRequest(payload, std::move(eventHandler));
        break;
      case aap_protobuf::service::bluetooth::BluetoothMessageId::BLUETOOTH_MESSAGE_AUTHENTICATION_RESULT:
        this->handleBluetoothAuthenticationResult(payload, std::move(eventHandler));
        break;
      default:
        SDK_LOG_ERROR("[BluetoothService] Message Id not Handled: ", messageId.getId());
        this->receive(std::move(eventHandler));
        break;
    }
  }

  void BluetoothService::handleChannelOpenRequest(const common::DataConstBuffer &payload,
                                                  IBluetoothServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[BluetoothService] handleChannelOpenRequest()");

    aap_protobuf::service::control::message::ChannelOpenRequest request;
    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onChannelOpenRequest(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }

  void BluetoothService::handleBluetoothAuthenticationResult(const common::DataConstBuffer &payload,
                                                                  IBluetoothServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[BluetoothService] handleBluetoothAuthenticationResult()");

    aap_protobuf::service::bluetooth::message::BluetoothAuthenticationResult request;

    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onBluetoothAuthenticationResult(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }

  void BluetoothService::handleBluetoothPairingRequest(const common::DataConstBuffer &payload,
                                                       IBluetoothServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[BluetoothService] handleBluetoothPairingRequest()");

    aap_protobuf::service::bluetooth::message::BluetoothPairingRequest request;
    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onBluetoothPairingRequest(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }

}


