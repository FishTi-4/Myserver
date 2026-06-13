#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define QUEUE_DEPTH 512
#define BUFFER_SIZE 1024
// 事件类型
enum EventType { ACCEPT, READ, WRITE };
// 缓冲区结构体
struct Buffer {
  char buf[BUFFER_SIZE];
  size_t length;
};

// 连接结构体
struct connection {
  int fd;                       // 文件描述
  struct sockaddr_in addr;         // 地址结构体
  char readbuffer[BUFFER_SIZE]; // 读缓冲区
  struct Buffer writebuffer;    // 写缓冲区
  enum EventType event;         // 事件类型
};

// 创建监听socket
int make_listener(const char *ip, int port) {
  // 1.创建socket
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock == -1) {
    perror("socket");
    return -1;
  }
  // 2.构造地址结构体
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(port);

  int opt = 1;
  if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    perror("setsockopt");
    close(listen_sock);
    return -1;
  }
  // 3.绑定
  if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(listen_sock);
    return -1;
  }
  // 4.监听
  if (listen(listen_sock, 5) == -1) {
    perror("listen");
    close(listen_sock);
    return -1;
  }
  return listen_sock;
}

// 创建并初始化io_uring实例
struct io_uring uring_create() {
  struct io_uring ring;
  if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
    perror("io_uring_queue_init");
    exit(1);
  }
  return ring;
}

// 销毁io_uring实例
void uring_destroy(struct io_uring *ring) { io_uring_queue_exit(ring); }

// 提交读请求,
//  注意：调用此函数的时候，conn已经是一个准备好的新连接,fd已经被填充
bool read_request(struct io_uring *ring, struct connection *conn) {
  // 获取sqe
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    fprintf(stderr, "io_uring_get_sqe failed\n");
    return false;
  }
  // 设置事件类型为读
  conn->event = READ;
  // 准备读请求
  io_uring_prep_read(sqe, conn->fd, conn->readbuffer, BUFFER_SIZE, 0);
  // 关联用户数据，将连接结构体传递给完成队列
  io_uring_sqe_set_data(sqe, conn);
  return true;
}
// 提交写请求
//  注意：调用此函数的时候，conn已经是一个准备好的新连接,fd已经被填充
bool write_request(struct io_uring *ring, struct connection *conn) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    fprintf(stderr, "io_uring_get_sqe failed\n");
    return false;
  }
  conn->event = WRITE;
  io_uring_prep_write(sqe, conn->fd, conn->writebuffer.buf,
                      conn->writebuffer.length, 0);
  io_uring_sqe_set_data(sqe, conn);
  return true;
}
// 提交accept请求
//  注意：调用此函数的时候，clisten_conn已经是一个准备好的新连接,fd已经被填充
bool accept_request(struct io_uring *ring, struct connection *listen_conn) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    fprintf(stderr, "io_uring_get_sqe failed\n");
    return false;
  }
  listen_conn->event = ACCEPT;
  socklen_t addrlen = sizeof(listen_conn->addr);
  io_uring_prep_accept(sqe, listen_conn->fd,(struct sockaddr*)&listen_conn->addr,&addrlen,0);
  io_uring_sqe_set_data(sqe, listen_conn);
  return true;
}
// 创建连接结构体
struct connection *create_connection(int fd) {
  struct connection *conn = malloc(sizeof(struct connection));
  if (!conn) {
    perror("malloc");
    return NULL;
  }
  // 填充fd
  conn->fd = fd;
  // 初始化读写缓冲区
  memset(conn->readbuffer, 0, BUFFER_SIZE);
  memset(conn->writebuffer.buf, 0, BUFFER_SIZE);
  conn->writebuffer.length = 0;
  return conn;
}
// 处理事件循环
void handle_events(struct io_uring *ring, int listen_sock) {
  // 创建监听连接结构体
  struct connection *listen_conn = create_connection(listen_sock);
  if (!listen_conn)
    return;
  // 提交第一次accept请求,让监听连接开始接受新的连接
  accept_request(ring, listen_conn);
  // 定义完成队列事件数组
  struct io_uring_cqe *cqes[QUEUE_DEPTH];
  // 事件循环
  while (true) {
    // 提交并等待事件
    int res = io_uring_submit_and_wait(ring, 1);
    if (res < 0) {
      fprintf(stderr, "io_uring_submit_and_wait: %s\n", strerror(-res));
      break;
    }
    // 非阻塞获取完成队列事件
    int num = io_uring_peek_batch_cqe(ring, cqes, QUEUE_DEPTH);
    // 处理每个完成事件
    for (int i = 0; i < num; i++) {
      // 获取关联的连接结构体
      struct connection *conn = (struct connection *)cqes[i]->user_data;
      // 根据事件类型处理
      if (conn->event == ACCEPT) {
        if (cqes[i]->res < 0) {
          fprintf(stderr, "accept failed: %d\n", cqes[i]->res);
        } else {
          // 新连接已接受，提交读请求
          int client_fd = cqes[i]->res;
          // 创建新连接结构体
          struct connection *client_conn = create_connection(client_fd);
          // 如果创建成功，提交读请求
          if (client_conn) {
            read_request(ring, client_conn);
          } else {
            close(client_fd);
          }
          printf("Accepted new connection: ip:%s , port:%d\n", inet_ntoa(conn->addr.sin_addr), ntohs(conn->addr.sin_port));
        }
        // 继续提交accept请求，接受下一个连接
        accept_request(ring, listen_conn);
      } else if (conn->event == READ) {
        int read_bytes = cqes[i]->res;
        if (read_bytes <= 0) {
          close(conn->fd);
          free(conn);
        } else {
          // 处理读到的数据，这里简单地回显
          printf("server read : %.*s\n", read_bytes, conn->readbuffer);
          conn->writebuffer.length = read_bytes;
          memcpy(conn->writebuffer.buf, conn->readbuffer, read_bytes);
          write_request(ring, conn);
        }
      } else if (conn->event == WRITE) {
        int write_bytes = cqes[i]->res;
        if (write_bytes <= 0) {
          close(conn->fd);
          free(conn);
        } else {
          // 继续读
          read_request(ring, conn);
        }
      } else {
        fprintf(stderr, "unknown event type\n");
      }
      // 标记完成事件已处理
      io_uring_cqe_seen(ring, cqes[i]);
    }
  }
}

// 启动服务器
void start_server(const char *ip, int port) {
  // 创建监听socket
  int server_sock = make_listener(ip, port);
  if (server_sock == -1) {
    fprintf(stderr, "Failed to create listener\n");
    return;
  }
  // 创建io_uring实例
  struct io_uring ring = uring_create();
  // 处理事件循环
  handle_events(&ring, server_sock);
  // 清理
  uring_destroy(&ring);
  close(server_sock);
}

int main() {
  start_server("127.0.0.1", 8080);
  return 0;
}