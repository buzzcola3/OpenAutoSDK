// This file is part of aasdk library project.
//
// Interceptor entry point for custom message handling during message stream
// processing. Returning true from the interceptor consumes the message and
// prevents the default channel handlers from seeing it.

#pragma once


namespace aasdk::messenger {
	class Message;
}

namespace aasdk::messenger::interceptor {

bool handleMessage(const ::aasdk::messenger::Message& message);

}
