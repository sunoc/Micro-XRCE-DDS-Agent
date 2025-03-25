#ifndef UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
#define UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/RPMsgEndPoint.hpp>
#include <uxr/agent/transport/stream_framing/StreamFramingProtocol.hpp>

/*
 * These C header must stay this way to keep compatibility
 * with the upstream open-amp.
 */
extern "C" {
#include <uxr/agent/transport/rpmsg/platform_info.h>
}

#include <iostream>
#include <string>
#include <queue>

#include <cstdint>
#include <cstddef>
#include <sys/poll.h>

#include <openamp/virtio.h>
#include <openamp/open_amp.h>
#include <openamp/version.h>
#include <metal/alloc.h>
#include <metal/version.h>

/* RPMsg max payload size values*/
#define RPMSG_SERVICE_NAME         "rpmsg-openamp-demo-channel"
#define SHUTDOWN_MSG 0xEF56A55A

/* message printing utils */
#define UXR_PRINTF(msg, ...)  UXR_AGENT_LOG_INFO(UXR_DECORATE_GREEN(msg), " {}",  ##__VA_ARGS__)
#define UXR_WARNING(msg, ...) UXR_AGENT_LOG_INFO(UXR_DECORATE_YELLOW(msg), " {}",  ##__VA_ARGS__)
#define UXR_ERROR(msg, ...)   UXR_AGENT_LOG_ERROR(UXR_DECORATE_RED(msg), " {}", ##__VA_ARGS__)

/* Buffer between the cb and the read function */
struct rpmsg_in_data_t
{
  uint8_t *pt;
  size_t len;
};

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
      static unsigned long int * i_raw_data_ptr;
      static int shutdown_req;
      static std::queue<rpmsg_in_data_t> in_data_q;

    private:

      virtual bool init() = 0;

      virtual bool fini() = 0;

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
      FramingIO framing_io_;
      int opt;
      int charfd;

    };

  } // namespace uxr
} // namespace eprosima

#endif // UXR_AGENT_TRANSPORT_RPMSG_RPMSGAGENTLINUX_HPP_
