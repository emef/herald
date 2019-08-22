/** \mainpage herald - A lightweight, minimal, inter-process pubsub C/C++ library.
See the full \ref API docs.

## Publisher example (no error checking):

\code{.cpp}
#include <chrono>
#include <sstream>
#include <thread>

#include <herald/herald.h>

int main(int argc, char **argv) {
  publisher_t *pub = publisher_create(8080, 1024);
  publisher_init(pub);

  for (int i=0; ; i++) {
    std::stringstream ss;
    ss << "message " << i;

    const std::string msg = ss.str();
    publisher_publish(pub, msg.c_str(), msg.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
\endcode

## Subscriber example (no error checking):

\code{.cpp}
#include <chrono>
#include <string>
#include <thread>

#include <herald/herald.h>

int main(int argc, char **argv) {
  subscriber_t *sub = subscriber_create(8080, [](const void *msg, size_t len) {
    std::string str_data = std::string((const char *) msg, len);
    std::cout << "got callback string data in subscriber: " << str_data << std::endl;
  });

  subscriber_init(sub);

  std::this_thread::sleep_for(std::chrono::seconds(10));
}
\endcode

**/

/// \defgroup API
/// This is the low level API which provides the publisher and subscriber interfaces.

#pragma once
#include "subscriber.h"
#include "publisher.h"
