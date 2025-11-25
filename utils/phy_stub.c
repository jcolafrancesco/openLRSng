// Minimal PhyDriver example that forwards frames over UDP to a host tool
// such as a GNU Radio flow graph. This file is not compiled by default;
// copy it into openLRSng/ or update your build to include it.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
  int (*init)(void);
  int (*send_frame)(const uint8_t *frame, size_t len, bool blocking);
  int (*poll_frame)(uint8_t *frame, size_t max_len);
  void (*shutdown)(void);
} PhyDriver;

#ifndef PHY_MAX_FRAME
#define PHY_MAX_FRAME 21
#endif

#ifndef PHY_UDP_PORT
#define PHY_UDP_PORT 46000
#endif

static int sock_fd = -1;
static struct sockaddr_in host_addr;

static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return -errno;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return -errno;
  }
  return 0;
}

static int phy_udp_init(void) {
  sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    return -errno;
  }

  memset(&host_addr, 0, sizeof(host_addr));
  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(PHY_UDP_PORT);
  host_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  return set_nonblocking(sock_fd);
}

static int phy_udp_send(const uint8_t *frame, size_t len, bool blocking) {
  if (!frame || len == 0 || len > PHY_MAX_FRAME) {
    return -EMSGSIZE;
  }

  int flags = blocking ? 0 : MSG_DONTWAIT;
  ssize_t written = sendto(sock_fd, frame, len, flags, (struct sockaddr *)&host_addr, sizeof(host_addr));

  if (written < 0) {
    return -errno;
  }

  return (int)written;
}

static int phy_udp_poll(uint8_t *frame, size_t max_len) {
  if (!frame || max_len == 0) {
    return -EINVAL;
  }

  ssize_t n = recvfrom(sock_fd, frame, max_len, MSG_DONTWAIT, NULL, NULL);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    return -errno;
  }

  return (int)n;
}

static void phy_udp_shutdown(void) {
  if (sock_fd >= 0) {
    close(sock_fd);
    sock_fd = -1;
  }
}

PhyDriver external_phy_driver = {
  .init = phy_udp_init,
  .send_frame = phy_udp_send,
  .poll_frame = phy_udp_poll,
  .shutdown = phy_udp_shutdown,
};

