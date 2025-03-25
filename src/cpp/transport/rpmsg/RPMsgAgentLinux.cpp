#include <uxr/agent/transport/rpmsg/RPMsgAgentLinux.hpp>
#include <uxr/agent/utils/Conversion.hpp>
#include <uxr/agent/logger/Logger.hpp>

#include <unistd.h>

namespace eprosima {
  namespace uxr {

    RPMsgAgent::RPMsgAgent(
			     uint8_t addr,
			     Middleware::Kind middleware_kind)
      : Server<RPMsgEndPoint>{middleware_kind}
      , addr_{addr}
      , poll_fd_{}
      , buffer_{0}
      , framing_io_(
		    addr,
		    std::bind(&RPMsgAgent::write_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
		    std::bind(&RPMsgAgent::read_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4))
      , opt{}
      , charfd{}
    {}

    /**************************************************************************
     *
     * @brief        Basic usage of the rpmsg_send function directly.
     *
     **************************************************************************/
    ssize_t RPMsgAgent::write_data(
				    uint8_t* buf,
				    size_t len,
				    TransportRc& transport_rc)
    {
      size_t ret = 0;
      ssize_t bytes_written;

      UXR_PRINTF("Entered the write function!", NULL);
      bytes_written = rpmsg_trysend(&lept, buf, len);
      if (0 < bytes_written)
	{
          ret = size_t(bytes_written);
	}
      else
	{
	  UXR_ERROR("sending data failed with errno", strerror(errno));
          transport_rc = TransportRc::server_error;
	}
      return ret;
    }

    /**************************************************************************
     *
     * @brief        Access the buffer populated by the rpmsg calllback.
     *
     **************************************************************************/
    ssize_t RPMsgAgent::read_data(
				   uint8_t* buf,
				   size_t len,
				   int timeout,
				   TransportRc& transport_rc)
    {
      rpmsg_in_data_t in_data;
      int waittime = 10; /* milliseconds */

      if ( 0 >= timeout )
	{
	  UXR_ERROR("Timeout: ", strerror(ETIME));
	  transport_rc = TransportRc::timeout_error;
	  return 0;
	}

      if (in_data_q.empty())
	{
	  std::this_thread::sleep_for(std::chrono::milliseconds(waittime));
	  return RPMsgAgent::read_data(buf,
				       len,
				       timeout - waittime,
				       transport_rc);
	}

      in_data = in_data_q.front();
      in_data_q.pop();

      // UXR_PRINTF("Got a request for some bytes.", len);
      // UXR_PRINTF("Got a some bytes in the queue", in_data.len);
      // UXR_PRINTF("Got a some pointer in the queue", (void *)in_data.pt);

      for (size_t i = 0; i<in_data.len; i++)
	{
	  //UXR_PRINTF("in_data.pt[i]", in_data.pt[i]);
	  buf[i] = in_data.pt[i];
	}

      return (ssize_t)in_data.len;
    }

    bool RPMsgAgent::recv_message(
				   InputPacket<RPMsgEndPoint>& input_packet,
				   int timeout,
				   TransportRc& transport_rc)
    {
      bool ret = false;
      uint8_t remote_addr = 0x00;
      ssize_t bytes_read = 0;

      do
	{
	  bytes_read = framing_io_.read_framed_msg(
						   buffer_,
						   SERVER_BUFFER_SIZE,
						   remote_addr,
						   timeout,
						   transport_rc);
	}
      while ((0 == bytes_read) && (0 < timeout));

      if (0 < bytes_read)
	{
	  input_packet.message.reset(new InputMessage(buffer_, static_cast<size_t>(bytes_read)));
	  input_packet.source = RPMsgEndPoint(remote_addr);
	  ret = true;


	  uint32_t raw_client_key;
	  if (Server<RPMsgEndPoint>::get_client_key(input_packet.source, raw_client_key))
	    {
	      UXR_AGENT_LOG_MESSAGE(
				    UXR_DECORATE_YELLOW("[==>> SER <<==]"),
				    raw_client_key,
				    input_packet.message->get_buf(),
				    input_packet.message->get_len());
	    }
	}
      return ret;
    }

    bool RPMsgAgent::send_message(
				   OutputPacket<RPMsgEndPoint> output_packet,
				   TransportRc& transport_rc)
    {
      bool ret = false;
      ssize_t bytes_written =
	framing_io_.write_framed_msg(
				     output_packet.message->get_buf(),
				     output_packet.message->get_len(),
				     output_packet.destination.get_addr(),
				     transport_rc);
      if ((0 < bytes_written) && (
				  static_cast<size_t>(bytes_written) == output_packet.message->get_len()))
	{
	  ret = true;

	  uint32_t raw_client_key;
	  if (Server<RPMsgEndPoint>::get_client_key(output_packet.destination, raw_client_key))
	    {
	      UXR_AGENT_LOG_MESSAGE(
				    UXR_DECORATE_YELLOW("[** <<SER>> **]"),
				    raw_client_key,
				    output_packet.message->get_buf(),
				    output_packet.message->get_len());
	    }
	}
      return ret;
    }

  } // namespace uxr
} // namespace eprosima
