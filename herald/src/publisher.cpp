#include <herald/publisher.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <queue>
#include <random>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "sharedmem.h"

struct client_t {
  client_t(const std::string herald_id, const int buffer_size)
    : _herald_id(herald_id)
    , _buffer_size(buffer_size) {
  }

  bool init() {
    SharedMem *shared_mem = sharedmem_create(_herald_id, _buffer_size, true);
    if (shared_mem != nullptr) {
      _shared_mem = shared_mem;
      return true;
    } else {
      return false;
    }
  }

  void write(const void *data, const size_t length) {
    const int read_idx = _shared_mem->header->read_idx;
    const int write_idx = _shared_mem->header->write_idx;
    const int new_write_idx =
      (read_idx != 0 && write_idx != 0) ? 0 :
      (read_idx != 1 && write_idx != 1) ? 1 :
      2;

    uint8_t *write_buffer = _shared_mem->buffers[new_write_idx];
    memcpy(write_buffer, data, length);
    _shared_mem->header->lengths[new_write_idx] = length;

    pthread_mutex_lock(&_shared_mem->header->mutex);

    _shared_mem->header->write_idx = new_write_idx;
    _shared_mem->header->generation++;
    pthread_cond_signal(&_shared_mem->header->cond);

    pthread_mutex_unlock(&_shared_mem->header->mutex);
  }

  ~client_t() {
    if (_shared_mem != nullptr) {
      sharedmem_destroy(_shared_mem);
    }
  }

  const std::string _herald_id;
  const int _buffer_size;
  SharedMem *_shared_mem;
};

struct publisher_t {
  publisher_t(const int port, const size_t buffer_size);
  ~publisher_t();

  publisher_error init();
  publisher_error publish(const void *data, const size_t length);

  std::string get_next_id();

  // Thread functions
  void thread_server();
  void thread_publish();

  // Members
  const int _port;
  const int _buffer_size;

  std::atomic<bool> _running;
  std::thread _server_thread;
  std::thread _publish_thread;

  std::mutex _publish_mutex;
  std::condition_variable _publish_cv;
  std::queue<std::pair<const void*, size_t>> _publish_pending;
  std::vector<char> _id_alphabet;
  std::random_device _rnd_device;

  int _server_fd;
  std::mutex _client_mutex;
  std::unordered_map<int, std::shared_ptr<client_t>> _clients;
};

// API functions
// --------------------------------------------------

publisher_t *publisher_create(const int port, const size_t buffer_size) {
  return new publisher_t(port, buffer_size);
}

publisher_error publisher_init(publisher_t *publisher) {
  return publisher->init();
}

publisher_error publisher_publish(publisher_t *publisher, const void* data, const size_t length) {
  publisher->publish(data, length);
}

void publisher_destroy(publisher_t *publisher) {
  delete publisher;
}

// Implementation
// --------------------------------------------------

publisher_t::publisher_t(const int port, const size_t buffer_size)
  : _port(port)
  , _buffer_size(buffer_size)
  , _running(false)
  , _server_fd(-1) {

  for (char c = 'A'; c <= 'Z'; c++) _id_alphabet.push_back(c);
  for (char c = 'a'; c <= 'z'; c++) _id_alphabet.push_back(c);
  for (char c = '0'; c <= '9'; c++) _id_alphabet.push_back(c);
}

publisher_t::~publisher_t() {
  if (_running) {
    _running = false;
    _server_thread.join();
    _publish_thread.join();
  }

  if (_server_fd != -1) {
    close(_server_fd);
  }
}

publisher_error publisher_t::init() {
  if ((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    return PUB_NOSOCKET;
  }

  int opt = 1;
  if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    return PUB_NOSOCKET;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(_port);

  if (bind(_server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    return PUB_NOSOCKET;
  }

  if (listen(_server_fd, 3) < 0) {
    return PUB_NOSOCKET;
  }

  _running = true;
  _server_thread = std::thread(std::bind(&publisher_t::thread_server, this));
  _publish_thread = std::thread(std::bind(&publisher_t::thread_publish, this));
  return PUB_OK;
}

publisher_error publisher_t::publish(const void* data, const size_t length) {
  if (!_running) {
    return PUB_NOTRUNNING;
  }

  if (length > _buffer_size) {
    return PUB_TOOLARGE;
  }

  {
    std::unique_lock<std::mutex> lock(_publish_mutex);
    _publish_pending.push(std::make_pair(data, length));
    _publish_cv.notify_one();
  }

  return PUB_OK;
}

std::string publisher_t::get_next_id() {
  char id[32];
  std::uniform_int_distribution<int> dist(0, _id_alphabet.size() - 1);
  for (int i=0; i<32; i++) {
    id[i] = _id_alphabet[dist(_rnd_device)];
  }
  return std::string(id, 32);
}

void publisher_t::thread_server() {
  std::vector<struct pollfd> poll_set(1);
  poll_set[0].fd = _server_fd;
  poll_set[0].events = POLLIN;

  while (_running) {
    poll(poll_set.data(), poll_set.size(), 1000);

    std::vector<int> disconnected;

    const int num_fds = poll_set.size();
    for (int i=0; i<num_fds; i++) {
      const auto polled_fd = poll_set[i];

      if(!(polled_fd.revents & POLLIN)) {
        continue;
      }

      if (polled_fd.fd == _server_fd) {
        // handle new connection
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int new_socket;
        if ((new_socket = accept(_server_fd, (struct sockaddr *)&address,
                                 (socklen_t*)&addrlen)) < 0) {
          std::cerr << "could not open new connection" << std::endl;
          continue;
        }

        const std::string herald_id = get_next_id();

        std::shared_ptr<client_t> new_client = std::make_shared<client_t>(herald_id, _buffer_size);
        if (!new_client->init()) {
          std::cerr << "error initializing client in publisher" << std::endl;
          close(new_socket);
          continue;
        }

        {
          std::unique_lock<std::mutex> client_lock(_client_mutex);
          _clients[new_socket] = new_client;
        }

        const std::string open_resp = herald_id + " " + std::to_string(_buffer_size) + "\n";
        send(new_socket, open_resp.c_str(), open_resp.size(), 0);

        poll_set.emplace_back();
        poll_set.back().fd = new_socket;
        poll_set.back().events = POLLIN;

      } else {
        // handle client request: should never send data so we will always
        // just close the socket here.
        close(polled_fd.fd);
        _clients.erase(polled_fd.fd);
        disconnected.push_back(i);
      }
    }

    for (const int i : disconnected) {
      poll_set[i] = poll_set.back();
      poll_set.pop_back();
    }
  }
}

void publisher_t::thread_publish() {
  while (_running) {
    std::pair<const void*, size_t> publish_req;
    {
      std::unique_lock<std::mutex> lock(_publish_mutex);
      bool have_request = _publish_cv.wait_for(
        lock, std::chrono::milliseconds(100), [this] { return !_publish_pending.empty();});

      if (!have_request) {
        continue;
      }

      publish_req = _publish_pending.front();
      _publish_pending.pop();
    }

    {
      std::unique_lock<std::mutex> client_lock(_client_mutex);
      for (const auto client : _clients) {
        client.second->write(publish_req.first, publish_req.second);
      }
    }
  }
}
