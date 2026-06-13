#pragma once

#include "MCCS.h"

#include <liburing.h>

using namespace std;

constexpr int QUEUE_DEPTH = (1 << 10);

struct io_uring uring_create() {
  struct io_uring ring;
  if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
    perror("io_uring_queue_init");
    exit(1);
  }
  return ring;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        exit(1);
    }


    struct io_uring ring = {};
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0)
    {
        cerr << "Queue init failed" << endl;
        io_uring_queue_exit(&ring);
        exit(1);
    }

    

}