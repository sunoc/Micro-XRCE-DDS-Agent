#ifndef UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
#define UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/RPMsgEndPoint.hpp>
#include <uxr/agent/transport/stream_framing/StreamFramingProtocol.hpp>

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
#include <linux/rpmsg.h>
#include <queue>
#include <sys/mman.h>

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
#define RPMSG_HEADER_LEN        16
#define MAX_RPMSG_BUFF_SIZE     (512 - RPMSG_HEADER_LEN)
#define PAYLOAD_MIN_SIZE	1
#define PAYLOAD_MAX_SIZE	(MAX_RPMSG_BUFF_SIZE - 24)
#define NUM_PAYLOADS		(PAYLOAD_MAX_SIZE/PAYLOAD_MIN_SIZE)

#define RPMSG_BUS_SYS "/sys/bus/rpmsg"

#define SHUTDOWN_MSG 0xEF56A55A

/* message printing utils */
#define UXR_PRINTF(msg, ...)  UXR_AGENT_LOG_INFO(UXR_DECORATE_GREEN(msg), " {}",  ##__VA_ARGS__)
#define UXR_WARNING(msg, ...) UXR_AGENT_LOG_INFO(UXR_DECORATE_YELLOW(msg), " {}",  ##__VA_ARGS__)
#define UXR_ERROR(msg, ...)   UXR_AGENT_LOG_ERROR(UXR_DECORATE_RED(msg), " {}", ##__VA_ARGS__)

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

      /* rpmsg structure */
      rpmsg_endpoint_info eptinfo = {
	"rpmsg-openamp-demo-channel", // name[32]
	0, // src
	0, // dst
      };

    private:

      virtual bool init() = 0;

      virtual bool fini() = 0;

      ssize_t write_data(
			 uint8_t* buf,
			 size_t len,
			 TransportRc& transport_rc);

      ssize_t read_data(
			uint8_t* buf,
			size_t len,
			int timeout,
			TransportRc& transport_rc);

      bool recv_message(
			InputPacket<RPMsgEndPoint>& input_packet,
			int timeout,
			TransportRc& transport_rc) final;

      bool send_message(
			OutputPacket<RPMsgEndPoint> output_packet,
			TransportRc& transport_rc) final;

    protected:
      const uint8_t addr_;
      struct pollfd poll_fd_;
      uint8_t buffer_[SERVER_BUFFER_SIZE];
      FramingIO framing_io_;
      int opt;
      int charfd;

      /* RPMsg-specific general variables */
      int ntimes = 1;
      char rpmsg_dev[NAME_MAX];
      char rpmsg_char_name[16];
      char fpath[2*NAME_MAX];

      char ept_dev_name[16];
      char ept_dev_path[32];

      //int32_t rpmsg_buffer_len;
      int32_t rpmsg_buffer_top;
      uint8_t rpmsg_buffer[MAX_RPMSG_BUFF_SIZE];
      std::queue<uint8_t> rpmsg_queue;

      /* udmabuf specific variables*/
      struct pollfd udmabuf_fd, udmabuf_fd_addr;
      unsigned char *udmabuf;
      size_t buf_size; //, read_index;
      unsigned char  udma_attr[1024];
      unsigned long  udma_phys_addr;

#ifdef GPIO_MONITORING
      /* GPIO */
      struct alignas(0x200) GPIO_t {
	uint32_t	data;
      };

      int GPIO_fd;
      GPIO_t* gpio;
#endif

    };

  } // namespace uxr
} // namespace eprosima

#endif // UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
