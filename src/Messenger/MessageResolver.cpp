// This file is part of aasdk library project.
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

#include <Messenger/MessageResolver.hpp>

namespace aasdk::messenger {

  bool MessageResolver::resolve(const Message::Pointer& message, const FallbackHandler& fallback) const {
    if (message == nullptr) {
      return false;
    }

    switch (message->getChannelId()) {
      case ChannelId::CONTROL: {
        // Dummy branch for CONTROL traffic; currently passes straight through.
        if (fallback != nullptr) {
          fallback(message);
        }
        return true;
      }

      // Add channel-specific handling here. Return true once the promise is resolved or deferred.
      default:
        return false;
    }
  }

}
