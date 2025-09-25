#include <cstddef>
#include <pthread.h>
#include <uxr/agent/transport/rpmsg/RPMsgAgentLinux.hpp>
#include <uxr/agent/utils/Conversion.hpp>
#include <uxr/agent/logger/Logger.hpp>

#include <unistd.h>

namespace eprosima {
  namespace uxr {

    namespace {
      const std::string transport_rc_to_str( const TransportRc& transport_rc)
      {
	switch (transport_rc)
	  {
	  case TransportRc::connection_error:
	    {
	      return std::string("connection error");
	    }
	  case TransportRc::timeout_error:
	    {
	      return std::string("timeout error");
	    }
	  case TransportRc::server_error:
	    {
	      return std::string("server error");
	    }
	  default:
	    {
	      return std::string();
	    }
	  }
      }

    } // anonymous namespace

    RPMsgAgent::RPMsgAgent(
			   uint8_t addr,
			   Middleware::Kind middleware_kind)
      : Server<RPMsgEndPoint>{middleware_kind}
      , addr_{addr}
      , poll_fd_{}
      , buffer_{0}
    {}

    /*****************************************************************
     *
     * @brief   Trying to copy data in a faster way, by aligning with
     *          32bits blocks as much as possible.
     *
     * @param	len: data length to be copied
     *          src: pointer for data source
     *          dst: pointer for data destination
     *
     * @return	void
     *
     * @note	None.
     *
     ****************************************************************/
    void
    RPMsgAgent::aligned_copy(size_t len, uint8_t *src, uint8_t *dst)
    {
      /* Copy data byte by byte until aligned */
      while ( len && (
		      (((uintptr_t)dst) % sizeof(uint32_t)) ||
		      (((uintptr_t)src) % sizeof(uint32_t))))
	{
	  *dst = *(const uint8_t *)src;
	  dst++;
	  src++;
	  len--;
	}

      /* Copy data by 32bits. */
      for (; (uint32_t)len >= (uint32_t)sizeof(uint32_t);
	   dst += sizeof(uint32_t),
	     src += sizeof(uint32_t),
	     len -= sizeof(uint32_t))
	{
	  *(uint32_t *)dst = *(const uint32_t *)src;
	}

      /* Leftover data copied again bytes by byte. */
      for (; len != 0; dst++, src++, len--)
	{
	  *dst = *(const uint8_t *)src;
	}
    }

    /*****************************************************************
     *
     * @brief        Usage of the rpmsg_send function directly.
     *
     ****************************************************************/
    ssize_t
    RPMsgAgent::write_data(uint8_t* buf,
			   size_t len,
			   TransportRc& transport_rc)
    {
#ifdef GPIO_MONITORING
      /* turns on PIN 1 on GPIO channel 1 (brown)*/
      gpio[1].data = gpio[1].data | 0x2;
#endif
      size_t rv = 0;
      ssize_t bytes_written = 0;
      uint8_t udmabuf_payload[UDMA_ADDR_LEN];

      /* Put the data in the udmabuf, alligned by 32bits. */
      aligned_copy(len, buf, udmabuf0);

      /* Put the length and physical addr in the rpmsg buf.
	 Note that the offset udmabuff address is NOT sent. */
      for (int i = 0; i<4; i++)
	udmabuf_payload[i] = (udma0_phys_addr >> i*8) & 0x00FF;
      for (int i = 0; i<4; i++)
        udmabuf_payload[i + 4] = (len >> i * 8) & 0x00FF;

      bytes_written = rpmsg_trysend(&lept, udmabuf_payload, UDMA_ADDR_LEN);

      if ( UDMA_ADDR_LEN == bytes_written )
	rv = len;
      else
	{
	  UXR_ERROR("sending data failed with errno", strerror(errno));
          transport_rc = TransportRc::server_error;
	}

#ifdef GPIO_MONITORING
      /* turns off PIN 1 on GPIO channel 1 (brown)*/
      gpio[1].data = gpio[1].data & ~(0x2);
#endif
      return rv;
    }

    /*****************************************************************
     *
     * @brief        Access the buffer populated by the rpmsg calllback.
     *
     ****************************************************************/
    ssize_t
    RPMsgAgent::read_data(uint8_t* buf,
			  size_t len,
			  int timeout,
			  TransportRc& transport_rc)
    {
      struct rpmsg_rcv_msg in_data;
      unsigned int metal_irq_flag;
      size_t rcv_phys_addr = 0;
      ssize_t bytes_read = 0;

      if ( 0 >= timeout )
	{
	  UXR_ERROR("Read timeout: ", strerror(ETIME));
	  transport_rc = TransportRc::timeout_error;
	  return 0;
	}

      while ( rpmsg_rcv_msg_q.empty() )
	platform_poll(platform);

      /* Disabling remoteproc interrupts when
	 accessing the queue. */
      metal_irq_flag = metal_irq_save_disable();
      in_data = rpmsg_rcv_msg_q.front();
      rpmsg_rcv_msg_q.pop_front();
      metal_irq_restore_enable(metal_irq_flag);

      /* Get the real data length from the rpmsg pl. */
      if ( in_data.len == UDMA_ADDR_LEN )
	{
	  for ( int i = 0; i<4; i++ ) /* Read 4 bytes */
	    rcv_phys_addr += ( in_data.data[i] << i*8 );
	  for ( int i = 0; i<4; i++ ) /* Read 4 bytes */
	    bytes_read += ( in_data.data[i+4] << i*8 );
	}
      else
	{
	  UXR_ERROR("Wrong udmabuf package size received.",
		    strerror(errno));
	  transport_rc = TransportRc::server_error;
	  return 0;
	}


      aligned_copy(bytes_read, udmabuf1, buf);
      rpmsg_release_rx_buffer(in_data.ept, in_data.full_payload);

      return bytes_read;
    }

    /*****************************************************************
     *
     * @brief        Agent methode to receive messages.
     *
     ****************************************************************/
    bool
    RPMsgAgent::recv_message(
			     InputPacket<RPMsgEndPoint>& input_packet,
			     int timeout,
			     TransportRc& transport_rc)
    {
      bool rv = false;
      uint8_t remote_addr = 0x00;
      ssize_t bytes_read = 0;

      do
	{
	  bytes_read = read_data(buffer_,
				 SERVER_BUFFER_SIZE,
				 timeout,
				 transport_rc);
	}
      while ((0 == bytes_read) && (0 < timeout));

      if ( 0 < bytes_read && TransportRc::ok == transport_rc )
	{
	  input_packet.message.reset(new InputMessage(buffer_,
						      static_cast<size_t>(bytes_read)));
	  input_packet.source = RPMsgEndPoint(remote_addr);
	  rv = true;

	  uint32_t raw_client_key;
	  if ( Server<RPMsgEndPoint>::get_client_key(input_packet.source,
						     raw_client_key) )
	    {
	      UXR_AGENT_LOG_MESSAGE(
				    UXR_DECORATE_YELLOW("[==>> RPMsg <<==]"),
				    raw_client_key,
				    input_packet.message->get_buf(),
				    input_packet.message->get_len());
	    }
	}
      else
	{
	  std::stringstream ss;
	  ss << UXR_COLOR_RED << "Error while receiving message: "
	     << transport_rc_to_str(transport_rc) << UXR_COLOR_RESET;
	  UXR_AGENT_LOG_ERROR(
			      ss.str(),
			      "{} agent error",
			      "RPMsg");
	}
      return rv;
    }

    /*****************************************************************
     *
     * @brief        Agent methode to send messages.
     *
     ****************************************************************/
    bool
    RPMsgAgent::send_message(
			     OutputPacket<RPMsgEndPoint> output_packet,
			     TransportRc& transport_rc)
    {
      bool rv = false;
      ssize_t bytes_written = write_data(
					 output_packet.message->get_buf(),
					 output_packet.message->get_len(),
					 transport_rc);

      if ((0 < bytes_written)
	  && (static_cast<size_t>(bytes_written)
	      == output_packet.message->get_len()) )
	{
	  rv = true;

	  uint32_t raw_client_key;
	  if (Server<RPMsgEndPoint>::get_client_key(output_packet.destination,
						    raw_client_key))
	    {
	      UXR_AGENT_LOG_MESSAGE(
				    UXR_DECORATE_YELLOW("[** <<RPMsg>> **]"),
				    raw_client_key,
				    output_packet.message->get_buf(),
				    output_packet.message->get_len());
	    }
	}
      else
	{
	  std::stringstream ss;
	  ss << UXR_COLOR_RED
	     << "Error while sending message: "
	     << transport_rc_to_str(transport_rc)
	     << ". Expected to send "
	     << output_packet.message->get_len()
	     << " bytes, but sent "
	     << bytes_written
	     << "instead"
	     << UXR_COLOR_RESET;
	  UXR_AGENT_LOG_ERROR(
			      ss.str(),
			      "{} agent error",
			      "RPmsg");
	}
      return rv;
    }

  } // namespace uxr
} // namespace eprosima
