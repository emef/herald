#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include <herald/publisher.h>
#include <herald/subscriber.h>

int main() {
  publisher_t *publisher = publisher_create(8080, 1024);
  if (PUB_OK != publisher_init(publisher)) {
    std::cerr << "error initializing publisher" << std::endl;
    return -1;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  subscriber_t *subscriber = subscriber_create(8080, [](const void *msg, size_t len) {
      std::string str_data = std::string((const char *) msg, len);
      std::cout << "got callback string data in subscriber: " << str_data << std::endl;
  });

  if (SUB_OK != subscriber_init(subscriber)) {
    std::cerr << "error initializing subscriber" << std::endl;
    return -1;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  for (int i=0; i<10; i++) {
    std::stringstream ss;
    ss << "message " << i;
    const std::string msg = ss.str();
    if (PUB_OK != publisher_publish(publisher, msg.c_str(), msg.size())) {
      std::cerr << "error publishing" << std::endl;
      return -1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  subscriber_destroy(subscriber);
  publisher_destroy(publisher);
}
