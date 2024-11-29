#include <uxr/agent/transport/rpmsg-lite/RPMsgLiteAgentLinux.hpp>
#include <uxr/agent/utils/Conversion.hpp>
#include <uxr/agent/logger/Logger.hpp>

#include <unistd.h>

namespace eprosima {
namespace uxr {

RPMsgLiteAgent::RPMsgLiteAgent(
        uint8_t addr,
        Middleware::Kind middleware_kind)
    : Server<RPMsgLiteEndPoint>{middleware_kind}
    , addr_{addr}
    , buffer_{0}
    , framing_io_(
          addr,
          std::bind(&RPMsgLiteAgent::write_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          std::bind(&RPMsgLiteAgent::read_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4))
{}

ssize_t RPMsgLiteAgent::write_data(
				   uint8_t* buf,
				   size_t len,
				   TransportRc& transport_rc)
{
  int32_t ret;
  uint8_t* tx_buffer = (uint8_t*)rpmsg_lite_alloc_tx_buffer(rpmsg_lite_dev, &len, 0);
  if ( RL_NULL == tx_buffer )
    UXR_ERROR("Failed to allocate TX buffer", strerror(errno));

  /* Dirty way to put data in the tx buffer... */
  for (int i = 0; i<len; i++)
    tx_buffer[i] = buf[i];

  ret = rpmsg_lite_send_nocopy(dev, ept, dst, tx_buffer, len);
  if (0 != ret)
    {
      UXR_ERROR("Failed to write data", strerror(errno));
    }
  else
    return len;
}

ssize_t RPMsgLiteAgent::read_data(
				  uint8_t* buf,
				  size_t len,
				  int timeout,
				  TransportRc& transport_rc)
{
  int32_t ret = rpmsg_queue_recv_nocopy(dev, q, ept, dst, buf, len);
  if (0 != ret)
    {
      UXR_ERROR("Failed to write data", strerror(errno));
    }
  else
    return len;

    
    ssize_t bytes_read = 0;
    //int poll_rv = poll(&poll_fd_, 1, timeout);
    if(poll_fd_.revents & (POLLERR+POLLHUP))
    {
        transport_rc = TransportRc::server_error;;
    }
    else if (0 < poll_rv)
    {
        bytes_read = read(poll_fd_.fd, buf, len);
        if (0 > bytes_read)
        {
            transport_rc = TransportRc::server_error;
        }
    }
    else
    {
        transport_rc = (poll_rv == 0) ? TransportRc::timeout_error : TransportRc::server_error;
    }
    return bytes_read;
}

bool RPMsgLiteAgent::recv_message(
        InputPacket<RPMsgLiteEndPoint>& input_packet,
        int timeout,
        TransportRc& transport_rc)
{
    bool rv = false;
    uint8_t remote_addr = 0x00;
    ssize_t bytes_read = 0;

    do
    {
        bytes_read = framing_io_.read_framed_msg(
            buffer_, SERVER_BUFFER_SIZE, remote_addr, timeout, transport_rc);
    }
    while ((0 == bytes_read) && (0 < timeout));

    if (0 < bytes_read)
    {
        input_packet.message.reset(new InputMessage(buffer_, static_cast<size_t>(bytes_read)));
        input_packet.source = RPMsgLiteEndPoint(remote_addr);
        rv = true;

        uint32_t raw_client_key;
        if (Server<RPMsgLiteEndPoint>::get_client_key(input_packet.source, raw_client_key))
        {
            UXR_AGENT_LOG_MESSAGE(
                UXR_DECORATE_YELLOW("[==>> SER <<==]"),
                raw_client_key,
                input_packet.message->get_buf(),
                input_packet.message->get_len());
        }
    }
    return rv;
}

bool RPMsgLiteAgent::send_message(
        OutputPacket<RPMsgLiteEndPoint> output_packet,
        TransportRc& transport_rc)
{
    bool rv = false;
    ssize_t bytes_written =
            framing_io_.write_framed_msg(
                output_packet.message->get_buf(),
                output_packet.message->get_len(),
                output_packet.destination.get_addr(),
                transport_rc);
    if ((0 < bytes_written) && (
         static_cast<size_t>(bytes_written) == output_packet.message->get_len()))
    {
        rv = true;

        uint32_t raw_client_key;
        if (Server<RPMsgLiteEndPoint>::get_client_key(output_packet.destination, raw_client_key))
        {
            UXR_AGENT_LOG_MESSAGE(
                UXR_DECORATE_YELLOW("[** <<SER>> **]"),
                raw_client_key,
                output_packet.message->get_buf(),
                output_packet.message->get_len());
        }
    }
    return rv;
}

} // namespace uxr
} // namespace eprosima
