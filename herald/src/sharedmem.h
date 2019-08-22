#pragma once

#include <pthread.h>
#include <string>
#include <sys/types.h>

struct SharedMemHeader {
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  long generation;
  int read_idx;
  int write_idx;

  int lengths[3];
};

struct SharedMem {
  std::string shm_name;
  int buffer_size;

  // Shared memory fd and mmap region.
  int shm_fd;
  void *shm;
  int shm_size;
  bool owned;

  // Pointers into shared memory region
  SharedMemHeader *header;
  uint8_t *buffers[3];
};

SharedMem *sharedmem_create(const std::string shm_name, const int buffer_size, const bool create);

void sharedmem_destroy(SharedMem *shared_mem);
