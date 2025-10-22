#ifndef UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
#define UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/RPMsgEndPoint.hpp>

/*
 * These C header must stay this way to keep compatibility
 * with the upstream open-amp.
 */
extern "C" {
#include <uxr/agent/transport/rpmsg/platform_info.h>
}

#include <deque>
#include <pthread.h>

#include <termios.h>

#include <cstdint>
#include <cstddef>
#include <sys/poll.h>
#include <sys/types.h>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sstream>
#include <linux/rpmsg.h>
#include <queue>
#include <sys/mman.h>

#include <openamp/virtio.h>
#include <openamp/open_amp.h>
#include <openamp/version.h>
#include <openamp/rpmsg.h>

#include <metal/alloc.h>
#include <metal/version.h>
#include <metal/irq.h>
#include <iostream>
#include <sys/stat.h>

#define GPIO_MONITORING

#ifdef GPIO_MONITORING
/* GPIO */
#define NUM_GPIO 4

#define gpio_base 0xa0000000
#define gpio_size (sizeof(GPIO_t) * NUM_GPIO)
#endif

/* RPMsg max payload size values*/
#define RPMSG_SERVICE_NAME         "rpmsg-openamp-demo-channel"
/* 8192 + 16 + 24 = 8232 */
#define RPMSG_HEADER_LEN        16
#define MAX_RPMSG_BUFF_SIZE     (8232 - RPMSG_HEADER_LEN)
#define PAYLOAD_MIN_SIZE	1
#define PAYLOAD_MAX_SIZE	(MAX_RPMSG_BUFF_SIZE - 24)
#define NUM_PAYLOADS		(PAYLOAD_MAX_SIZE/PAYLOAD_MIN_SIZE)

/* Hybrid mode cutoff size in bytes */
#define CUTOFF_SIZE 345
#define RPMSG_BUS_SYS "/sys/bus/rpmsg"
#define UDMA_ADDR_LEN           8

#define SHUTDOWN_MSG 0xEF56A55A

/* message printing utils */
#define UXR_PRINTF(msg, ...)  UXR_AGENT_LOG_INFO(UXR_DECORATE_GREEN(msg), " {}",  ##__VA_ARGS__)
#define UXR_WARNING(msg, ...) UXR_AGENT_LOG_INFO(UXR_DECORATE_YELLOW(msg), " {}",  ##__VA_ARGS__)
#define UXR_ERROR(msg, ...)   UXR_AGENT_LOG_ERROR(UXR_DECORATE_RED(msg), " {}", ##__VA_ARGS__)

/* Buffer between the cb and the read function */
struct rpmsg_rcv_msg {
  uint8_t * data;
  size_t len;
  struct rpmsg_endpoint *ept;
  void * full_payload;
};

#ifdef GPIO_MONITORING
struct alignas(0x200) GPIO_t {
  uint32_t	data;
};
#endif

namespace eprosima {
  namespace uxr {

    class RPMsgAgent : public Server<RPMsgEndPoint>
    {
    public:
      RPMsgAgent(
		  uint8_t addr,
		  Middleware::Kind middleware_kind);

#ifdef UAGENT_DISCOVERY_PROFILE
      bool has_discovery() final { return false; }
#endif

#ifdef UAGENT_P2P_PROFILE
      bool has_p2p() final { return false; }
#endif

      uint8_t * i_payload;
      void *platform;
      struct rpmsg_device *rpdev;

      /* Static variables for static class methods. */
      static struct rpmsg_endpoint lept;
      static int shutdown_req;

      static std::deque<rpmsg_rcv_msg>rpmsg_rcv_msg_q;

#ifdef GPIO_MONITORING
      static int GPIO_fd;
      static GPIO_t* gpio;
#endif

    private:

      virtual bool init() = 0;

      virtual bool fini() = 0;

      void aligned_copy(size_t len, uint8_t* src, uint8_t* dst);

      bool recv_message(
			InputPacket<RPMsgEndPoint>& input_packet,
			int timeout,
			TransportRc& transport_rc) final;

      bool send_message(
			OutputPacket<RPMsgEndPoint> output_packet,
			TransportRc& transport_rc) final;

      ssize_t write_data(
			 uint8_t* buf,
			 size_t len,
			 TransportRc& transport_rc);

      ssize_t read_data(
			uint8_t* buf,
			size_t len,
			int timeout,
			TransportRc& transport_rc);

    protected:
      const uint8_t addr_;
      struct pollfd poll_fd_;
      uint8_t buffer_[SERVER_BUFFER_SIZE];

      /* udmabuf specific variables*/
      struct pollfd udmabuf0_fd, udmabuf0_fd_addr;
      struct pollfd udmabuf1_fd, udmabuf1_fd_addr;
      uint8_t *udmabuf0, *udmabuf1;
      size_t buf_size;
      uint8_t  udma0_attr[MAX_RPMSG_BUFF_SIZE];
      uint8_t  udma1_attr[MAX_RPMSG_BUFF_SIZE];
      uint32_t  udma0_phys_addr,  udma1_phys_addr;

    };

  } // namespace uxr
} // namespace eprosima

#endif // UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
