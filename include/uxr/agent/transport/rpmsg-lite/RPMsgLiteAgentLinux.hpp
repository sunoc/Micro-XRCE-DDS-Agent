#ifndef _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_
#define _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/RPMsgLiteEndPoint.hpp>
#include <uxr/agent/transport/stream_framing/StreamFramingProtocol.hpp>

#include <uxr/agent/transport/rpmsg-lite/include/rpmsg_lite.h>
#include <uxr/agent/transport/rpmsg-lite/include/rpmsg_queue.h>
#include <uxr/agent/transport/rpmsg-lite/include/rpmsg_ns.h>

#define SHUTDOWN_MSG 0xEF56A55A

#define IPI_ADDR 0xFF990000
#define IPI_LEN  0xE0

/* message printing utils */
#define UXR_PRINTF(msg, ...)  UXR_AGENT_LOG_INFO(UXR_DECORATE_GREEN(msg), " {}",  ##__VA_ARGS__)
#define UXR_WARNING(msg, ...) UXR_AGENT_LOG_INFO(UXR_DECORATE_YELLOW(msg), " {}",  ##__VA_ARGS__)
#define UXR_ERROR(msg, ...)   UXR_AGENT_LOG_ERROR(UXR_DECORATE_RED(msg), " {}", ##__VA_ARGS__)

namespace eprosima {
namespace uxr {

class RPMsgLiteAgent : public Server<RPMsgLiteEndPoint>
{
public:
  RPMsgLiteAgent(
		 uint8_t addr,
		 Middleware::Kind middleware_kind);

#ifdef UAGENT_DISCOVERY_PROFILE
  bool has_discovery() final { return false; }
#endif

#ifdef UAGENT_P2P_PROFILE
  bool has_p2p() final { return false; }
#endif

  /* RPMsg-Lite related variables. */
  struct rpmsg_lite_instance *  	rpmsg_lite_dev;
  rpmsg_queue_handle                    rpmsg_lite_q;
  struct rpmsg_lite_endpoint*           rpmsg_lite_ept;


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
		    InputPacket<RPMsgLiteEndPoint>& input_packet,
		    int timeout,
		    TransportRc& transport_rc) final;

  bool send_message(
		    OutputPacket<RPMsgLiteEndPoint> output_packet,
		    TransportRc& transport_rc) final;


protected:
    const uint8_t addr_;
    uint8_t buffer_[SERVER_BUFFER_SIZE];
    FramingIO framing_io_;
};

} // namespace uxr
} // namespace eprosima

#endif // _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_
