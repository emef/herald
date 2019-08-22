#include <fcntl.h>
#include <iostream>
#include <semaphore.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

constexpr size_t NUM_ELEMENTS = 1000;

struct SharedMem {
  int read_idx;
  int write_idx;
  float data[NUM_ELEMENTS];
};

int open_shmem(const char *shm_name) {
  return shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
}


int usage() {
  std::cerr << "usage: client|server SHM_NAME" << std::endl;
  return 1;
}

int server(const char *shm_name) {
  std::cout << "[server] started" << std::endl;
  int shm_fd = open_shmem(shm_name);
  ftruncate(shm_fd, sizeof(SharedMem));

  SharedMem *shared_mem = (SharedMem*) mmap(
    nullptr, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

  shared_mem->read_idx = 0;
  shared_mem->write_idx = 1;

  sem_unlink(shm_name);
  sem_t *sem = sem_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, 0);
  if (sem == nullptr) {
    std::cerr << "sem_open fail: ";
    if (errno == EACCES) std::cerr << "EACCESS";
    if (errno == EEXIST) std::cerr << "EEXIST";
    if (errno == EINVAL) std::cerr << "EINVAL";
    if (errno == EMFILE) std::cerr << "EMFILE";
    if (errno == ENAMETOOLONG) std::cerr << "ENAMETOOLONG";
    if (errno == ENFILE) std::cerr << "ENFILE";
    if (errno == ENOENT) std::cerr << "ENOENT";
    if (errno == ENOMEM) std::cerr << "ENOMEM";
    std::cerr << std::endl;
    return 1;
  }

  int sem_value;
  sem_getvalue(sem, &sem_value);
  std::cout << "sem value: " << sem_value << std::endl;

  sem_post(sem);

  sem_getvalue(sem, &sem_value);
  std::cout << "sem value: " << sem_value << std::endl;

  sem_post(sem);

  sem_getvalue(sem, &sem_value);
  std::cout << "sem value: " << sem_value << std::endl;

  std::cout << "[server] initialized shared memory" << std::endl;

  return 0;
}

int client(const char* shm_name) {
  std::cout << "[client] started" << std::endl;
  int shm_fd = open_shmem(shm_name);
  SharedMem *shared_mem = (SharedMem*) mmap(
    nullptr, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

  std::cout << "[client] read_idx = " << shared_mem->read_idx
            << ", write_idx = " << shared_mem->write_idx << std::endl;

  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    return usage();
  }

  if (strcmp(argv[1], "server") == 0) return server(argv[2]);
  else if (strcmp(argv[1], "client") == 0) return client(argv[2]);
  else return usage();
}
