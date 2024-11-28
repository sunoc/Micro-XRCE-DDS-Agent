#ifndef _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_
#define _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/RPMsgLiteEndPoint.hpp>
#include <uxr/agent/transport/stream_framing/StreamFramingProtocol.hpp>

#include <uxr/agent/transport/rpmsg-lite/include/rpmsg_lite.h>

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
    struct pollfd poll_fd_;
    uint8_t buffer_[SERVER_BUFFER_SIZE];
    FramingIO framing_io_;
};

} // namespace uxr
} // namespace eprosima

#endif // _UXR_AGENT_TRANSPORT_RPMSGLITE_RPMSGLITEAGENTLINUX_HPP_
