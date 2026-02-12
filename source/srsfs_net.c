#include "srsfs_net.h"

#define LOG(fmt, ...) pr_info("[srsfs/srsfs_net]: " fmt, ##__VA_ARGS__)

int64_t send_package(
    const struct srsfs_request_package* reqp, struct srsfs_response_package* resp
) {
  struct sockaddr_in s_addr = {
      .sin_family = AF_INET, .sin_addr = {.s_addr = in_aton(ADDR)}, .sin_port = htons(PORT)
  };
  struct socket* sock;
  int64_t error = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
  if (error < 0) {
    return -1;
  }
  error = kernel_connect(sock, (struct sockaddr*)&s_addr, sizeof(struct sockaddr_in), 0);
  if (error != 0) {
    LOG("send_package: could not connect");
    sock_release(sock);
    return -2;
  }
  struct kvec kvec;
  kvec.iov_base = reqp;
  kvec.iov_len = sizeof(*reqp);
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  error = kernel_sendmsg(sock, &msg, &kvec, 1, kvec.iov_len);
  if (error < 0) {
    LOG("send_package: could not send message");
    goto end;
  }
  memset(&msg, 0, sizeof(msg));
  memset(&kvec, 0, sizeof(kvec));
  kvec.iov_base = resp;
  kvec.iov_len = sizeof(*resp);
  error = kernel_recvmsg(sock, &msg, &kvec, 1, kvec.iov_len, 0);
  if (error >= 0)
    error = 0;
  else
    LOG("send_package: error=%ld", error);
end:
  kernel_sock_shutdown(sock, SHUT_RDWR);
  sock_release(sock);
  return error;
}

int64_t ping(void) {
  struct srsfs_request_package reqp;
  reqp.pt = SRSFS_PING;
  struct srsfs_response_package resp;
  int64_t err = send_package(&reqp, &resp);
  if (err < 0)
    return err;
  return resp.code;
}
