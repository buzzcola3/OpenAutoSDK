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

#include <Channel/VendorExtension/IVendorExtensionServiceEventHandler.hpp>
#include <Channel/VendorExtension/VendorExtensionService.hpp>
#include "Debug_cfg.hpp"

// To enable logging in this file, uncomment the following line
// #define VENDOR_EXTENSION_SERVICE_LOG_ENABLED

#ifndef VENDOR_EXTENSION_SERVICE_LOG_ENABLED
#undef SDK_LOG_DEBUG
#define SDK_LOG_DEBUG(...)
#endif

/*
 * This is a Vendor Extension channel to link to a known Vendor App on the Mobile Phone.
 */

namespace aasdk::channel::vendorextension {
  using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

  VendorExtensionService::VendorExtensionService(Strand &strand,
                                                 messenger::IMessenger::Pointer messenger)
      : Channel(strand, std::move(messenger), messenger::ChannelId::VENDOR_EXTENSION) {

  }

  void VendorExtensionService::receive(IVendorExtensionServiceEventHandler::Pointer eventHandler) {

    SDK_LOG_DEBUG("[VendorExtensionService] receive()");
    auto receivePromise = messenger::ReceivePromise::defer(strand_);
    receivePromise->then(
        std::bind(&VendorExtensionService::messageHandler, this->shared_from_this(), std::placeholders::_1,
                  eventHandler),
        std::bind(&IVendorExtensionServiceEventHandler::onChannelError, eventHandler, std::placeholders::_1));

    messenger_->enqueueReceive(channelId_, std::move(receivePromise));
  }

  void VendorExtensionService::sendChannelOpenResponse(const aap_protobuf::service::control::message::ChannelOpenResponse &response,
                                                       SendPromise::Pointer promise) {
    SDK_LOG_DEBUG("[VendorExtensionService] sendChannelOpenResponse()");
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED,
                                                      messenger::MessageType::CONTROL));
    message->insertPayload(
        messenger::MessageId(
            aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
  }

  void VendorExtensionService::messageHandler(messenger::Message::Pointer message,
                                              IVendorExtensionServiceEventHandler::Pointer eventHandler) {

    SDK_LOG_DEBUG("[VendorExtensionService] messageHandler()");

    messenger::MessageId messageId(message->getPayload());
    common::DataConstBuffer payload(message->getPayload(), messageId.getSizeOf());

    switch (messageId.getId()) {
      case aap_protobuf::service::control::message::ControlMessageType::MESSAGE_CHANNEL_OPEN_REQUEST:
        this->handleChannelOpenRequest(payload, std::move(eventHandler));
        break;
      default:
        SDK_LOG_ERROR("[VendorExtensionService] Message Id not Handled: ", messageId.getId());
        this->receive(std::move(eventHandler));
        break;
    }
  }

  void VendorExtensionService::handleChannelOpenRequest(const common::DataConstBuffer &payload,
                                                        IVendorExtensionServiceEventHandler::Pointer eventHandler) {
    SDK_LOG_DEBUG("[VendorExtensionService] handleChannelOpenRequest()");
    aap_protobuf::service::control::message::ChannelOpenRequest request;
    if (request.ParseFromArray(payload.cdata, payload.size)) {
      eventHandler->onChannelOpenRequest(request);
    } else {
      eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
  }
}


