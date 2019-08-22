#include "sharedmem.h"

#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

SharedMem *sharedmem_create(const std::string shm_name, const int buffer_size, const bool create) {
  int oflag = O_RDWR;
  if (create) oflag |= O_CREAT;

  int shm_fd = shm_open(shm_name.c_str(), oflag, S_IRUSR | S_IWUSR);
  if (-1 == shm_fd) {
    return nullptr;
  }

  const int shm_size = sizeof(SharedMemHeader) + 3 * buffer_size;

  if (create) {
    if (0 != ftruncate(shm_fd, shm_size)) {
      shm_unlink(shm_name.c_str());
      return nullptr;
    }
  }

  uint8_t *shm = (uint8_t*) mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (MAP_FAILED == shm) {
    shm_unlink(shm_name.c_str());
    return nullptr;
  }

  SharedMem *shared_mem = new SharedMem;
  shared_mem->shm_name = shm_name;
  shared_mem->buffer_size = buffer_size;
  shared_mem->shm_fd = shm_fd;
  shared_mem->shm = shm;
  shared_mem->shm_size = shm_size;
  shared_mem->owned = create;
  shared_mem->header = (SharedMemHeader*) shm;
  shared_mem->buffers[0] = shm + sizeof(SharedMemHeader);
  shared_mem->buffers[1] = shm + (sizeof(SharedMemHeader) * 2);
  shared_mem->buffers[2] = shm + (sizeof(SharedMemHeader) * 3);

  if (create) {
    pthread_mutexattr_t mutex_attr;
    if (0 != pthread_mutexattr_init(&mutex_attr)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      return nullptr;
    }

    if (0 != pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      return nullptr;
    }

    if (0 != pthread_mutex_init(&shared_mem->header->mutex, &mutex_attr)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      return nullptr;
    }

    pthread_condattr_t cond_attr;
    if (0 != pthread_condattr_init(&cond_attr)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      pthread_mutex_destroy(&shared_mem->header->mutex);
      return nullptr;
    }

    if (0 != pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      pthread_mutex_destroy(&shared_mem->header->mutex);
      return nullptr;
    }

    if (0 != pthread_cond_init(&shared_mem->header->cond, &cond_attr)) {
      munmap(shm, shm_size);
      shm_unlink(shm_name.c_str());
      pthread_mutex_destroy(&shared_mem->header->mutex);
      return nullptr;
    }

    shared_mem->header->generation = 0;
    shared_mem->header->read_idx = 0;
    shared_mem->header->write_idx = 0;
  }

  return shared_mem;
}

void sharedmem_destroy(SharedMem *shared_mem) {
  if (shared_mem->owned) {
    pthread_mutex_destroy(&shared_mem->header->mutex);
    pthread_cond_destroy(&shared_mem->header->cond);
  }

  munmap(shared_mem->shm, shared_mem->shm_size);
  shm_unlink(shared_mem->shm_name.c_str());
}
