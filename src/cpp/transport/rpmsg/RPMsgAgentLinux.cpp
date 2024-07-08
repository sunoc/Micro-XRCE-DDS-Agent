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
      , framing_io_(addr,
		    std::bind(&RPMsgAgent::write_data,
			      this, std::placeholders::_1,
			      std::placeholders::_2,
			      std::placeholders::_3),
		    std::bind(&RPMsgAgent::read_data,
			      this, std::placeholders::_1,
			      std::placeholders::_2,
			      std::placeholders::_3,
			      std::placeholders::_4))
      , opt{}
      , charfd{}
    //, rpmsg_buffer_len{0}
      , rpmsg_buffer_top{0}
    //  , rpmsg_leftover_len{0}
      , rpmsg_queue{}
    {}

    /**************************************************************************
     *
     * This function sends a message to the other ROS2 nodes.
     *
     *
     **************************************************************************/
    ssize_t
    RPMsgAgent::write_data(uint8_t* buf,
			   size_t len,
			   TransportRc& transport_rc)
    {
      size_t rv = 0;
      ssize_t bytes_written;
      unsigned long long udmabuf_payload;


      /* Put the data in the dma buf. */
      for (size_t i = 0; i<len; i++)
	{
	  ((uint8_t *)udmabuf)[i] = buf[i];
	}

      /* Put the length and physical addr in the rpmsg buf. */
      udmabuf_payload = udma_phys_addr + (len << 32);
      bytes_written = ::write(poll_fd_.fd, &udmabuf_payload, sizeof(udmabuf_payload));

      /* Test if anything was sent. */
      if (0 < bytes_written)
	{
          rv = size_t(bytes_written);
	}
      else
	{
	  UXR_ERROR("sending data failed with errno", strerror(errno));
          transport_rc = TransportRc::server_error;
	}
      return rv;
    }

    /**************************************************************************
     *
     * This function reads data from RPMsg directly.
     *
     *
     **************************************************************************/
    ssize_t
    RPMsgAgent::read_data(uint8_t* buf,
			  size_t len,
			  int timeout,
			  TransportRc& transport_rc)
    {
      size_t rpmsg_buffer_len = 0;
      unsigned long rpmsg_phys_addr;


      if ( 0 >= timeout )
	{
	  UXR_ERROR("Timeout: ", strerror(errno));
	  transport_rc = TransportRc::timeout_error;
	  return errno;
	}


      /* If we need more data, we go and read some */
      while ( len > rpmsg_buffer_len )
	{

	  /* Reading some rpmsg data; saving the length of the received block. */
	  rpmsg_buffer_len = read(poll_fd_.fd, rpmsg_buffer, MAX_RPMSG_BUFF_SIZE);

	  /* Expecting to get 64bits of data form the Client. */
	  if ( 8 != rpmsg_buffer_len )
	    {
	      UXR_ERROR("Wrong length received for a UDMABUF.", strerror(errno));
	    }
	  else
	    {

	      rpmsg_phys_addr = rpmsg_buffer[0]
		+ (rpmsg_buffer[1] << 8)
		+ (rpmsg_buffer[2] << 16)
		+ (rpmsg_buffer[3] << 24);
	      if ( udma_phys_addr != rpmsg_phys_addr)
		{
		  UXR_ERROR("Wrong phys addr received.", strerror(errno));
		  /* TODO how to manage that case ?? */
		}
	      else
		{

		  /* Test the cases for mismatches between len
		     and rpmsg_buffer[4] << 32 */

		  /* Put the data in the buf from the shared memory area. */
		  for ( int i = 0; i<(int)len; i++ )
		    {
		      buf[i] = ((uint8_t *)udmabuf)[i];
		    }
		}
	    }
	}

      return len;
    }

    /**************************************************************************
     *
     * This function is the receiving part from the Agent, getting data
     * from the "read_data function".
     *
     *
     **************************************************************************/
    bool
    RPMsgAgent::recv_message(InputPacket<RPMsgEndPoint>& input_packet,
			     int timeout,
			     TransportRc& transport_rc)
    {
      bool rv = false;
      uint8_t remote_addr = 0x00;
      ssize_t bytes_read = 0;

      do
	{
	  bytes_read = framing_io_.read_framed_msg( buffer_,
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
	  rv = true;

	  uint32_t raw_client_key;
	  if (Server<RPMsgEndPoint>::get_client_key(input_packet.source, raw_client_key))
	    {
	      UXR_AGENT_LOG_MESSAGE(
				    UXR_DECORATE_YELLOW("[==>> RMPsg <<==]"),
				    raw_client_key,
				    input_packet.message->get_buf(),
				    input_packet.message->get_len());
	    }
	}
      return rv;
    }

    /**************************************************************************
     *
     * This function sends messages to micro-ROS through DDS.
     *
     *
     **************************************************************************/
    bool
    RPMsgAgent::send_message(OutputPacket<RPMsgEndPoint> output_packet,
			     TransportRc& transport_rc)
    {

      bool rv = false;
      ssize_t bytes_written =
	framing_io_.write_framed_msg(output_packet.message->get_buf(),
				     output_packet.message->get_len(),
				     output_packet.destination.get_addr(),
				     transport_rc);
      if ((0 < bytes_written)
	  && (static_cast<size_t>(bytes_written) == output_packet.message->get_len()))
	{
	  rv = true;

	  uint32_t raw_client_key;
	  if (Server<RPMsgEndPoint>::get_client_key(output_packet.destination, raw_client_key))
	    {
	      UXR_AGENT_LOG_MESSAGE(UXR_DECORATE_YELLOW("[** <<RPMsg>> **]"),
				    raw_client_key,
				    output_packet.message->get_buf(),
				    output_packet.message->get_len());
	    }
	}
      return rv;
    }

  } // namespace uxr
} // namespace eprosima
