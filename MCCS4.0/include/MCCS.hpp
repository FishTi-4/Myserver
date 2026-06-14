#ifndef MCCS_HPP
#define MCCS_HPP

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <vector>
#include <algorithm>
#include <coroutine>
#include <utility>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <liburing.h>
#include <pthread.h>
#include <liburing.h>
#include <coroutine>

#define Xsize 65535
#define QUEUE_DEPTH 2048
const int MAX_USER = (1 << 20);

atomic<int> user_count = 0;

#endif