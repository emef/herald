#include <herald/subscriber.h>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

#include "sharedmem.h"

struct subscriber_t {
  subscriber_t(const int port, const callback_t callback);
  ~subscriber_t();

  subscriber_error init();

  void thread_callback();

  // members
  const int _port;
  const callback_t _callback;

  int _client_fd;
  SharedMem *_shared_mem;

  std::atomic<bool> _running;
  std::thread _callback_thread;
};

// API functions
// --------------------------------------------------

subscriber_t *subscriber_create(const int port, const callback_t callback) {
  return new subscriber_t(port, callback);
}

void subscriber_destroy(subscriber_t *subscriber) {
  delete subscriber;
}

subscriber_error subscriber_init(subscriber_t *subscriber) {
  return subscriber->init();
}

// Implementation
// --------------------------------------------------

subscriber_t::subscriber_t(const int port, const callback_t callback)
  : _port(port)
  , _callback(callback)
  , _running(false)
  , _client_fd(-1) {}

subscriber_t::~subscriber_t() {
  if (_running) {
    _running = false;
    _callback_thread.join();
  }

  if (_client_fd != -1) {
    close(_client_fd);
  }

  if (_shared_mem != nullptr) {
    sharedmem_destroy(_shared_mem);
  }
}

subscriber_error subscriber_t::init() {
  if ((_client_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    return SUB_NOSOCKET;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(_port);

  if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0)  {
    return SUB_NOSOCKET;
  }

  if (connect(_client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    return SUB_NOSOCKET;
  }

  char buffer[256];
  int bytes_read = recv(_client_fd, buffer, 256, 0);
  if (buffer[bytes_read-1] != '\n') {
    return SUB_BADRESP;
  }

  const std::string resp(buffer, bytes_read - 1);
  size_t sep_index = resp.find(' ');
  if (std::string::npos == sep_index) {
    return SUB_BADRESP;
  }

  const std::string herald_id = resp.substr(0, sep_index);
  const std::string buffer_size_str = resp.substr(sep_index + 1);
  int buffer_size;

  try {
    buffer_size = std::stoi(buffer_size_str);
  } catch (...) {
    return SUB_BADRESP;
  }

  _shared_mem = sharedmem_create(herald_id, buffer_size, false);
  if (_shared_mem == nullptr) {
    return SUB_NOSHAREDMEM;
  }

  _running = true;
  _callback_thread = std::thread(std::bind(&subscriber_t::thread_callback, this));

  return SUB_OK;
}

void subscriber_t::thread_callback() {
  pthread_mutex_t *mutex = &_shared_mem->header->mutex;
  pthread_cond_t *cond = &_shared_mem->header->cond;

  struct timeval now;
  struct timespec timeout;
  while (_running) {
    pthread_mutex_lock(mutex);

    gettimeofday(&now, nullptr);
    timeout.tv_sec = now.tv_sec + 1;
    timeout.tv_nsec = now.tv_usec * 1000;

    const long prev_generation = _shared_mem->header->generation;

    int rc = 0;
    while (prev_generation == _shared_mem->header->generation && rc == 0)
      rc = pthread_cond_timedwait(cond, mutex, &timeout);

    // timeout
    if (rc != 0) {
      pthread_mutex_unlock(mutex);
      continue;
    }

    const int new_read_idx = _shared_mem->header->write_idx;
    _shared_mem->header->read_idx = new_read_idx;

    pthread_mutex_unlock(mutex);

    const void *buffer = _shared_mem->buffers[new_read_idx];
    const size_t length = _shared_mem->header->lengths[new_read_idx];

    _callback(buffer, length);
  }
}
