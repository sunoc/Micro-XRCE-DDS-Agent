#include <cstdint>
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
		    std::bind(&RPMsgAgent::write_data, this,
			      std::placeholders::_1,
			      std::placeholders::_2,
			      std::placeholders::_3),
		    std::bind(&RPMsgAgent::read_data, this,
			      std::placeholders::_1,
			      std::placeholders::_2,
			      std::placeholders::_3,
			      std::placeholders::_4))
      , opt{}
      , charfd{}
    {}

    /**************************************************************************
     *
     * @brief   Trying to copy data in a faster way, by aligning with 32bits
     *          blocks as much as possible.
     *
     * @param	len: data length to be copied
     *          src: pointer for data source
     *          dst: pointer for data destination
     *
     * @return	void
     *
     * @note	None.
     *
     **************************************************************************/
    void
    RPMsgAgent::aligned_copy(size_t len, uint8_t *src, uint8_t *dst)
    {
      /* Copy data byte by byte until aligned */
      while ( len && (
		      (((uintptr_t)dst) % sizeof(uint32_t)) ||
		      (((uintptr_t)src) % sizeof(uint32_t))))
	{
	  *dst = *src;
	  dst++;
	  src++;
	  len--;
	}

      /* Copy data by 32bits. */
      for (; (uint32_t)len >= (uint32_t)sizeof(uint32_t); dst += sizeof(uint32_t),
	     src += sizeof(uint32_t),
	     len -= sizeof(uint32_t))
	{
	  *(uint32_t *)dst = *(const uint32_t *)src;
	}

      /* Leftover data copied again bytes by byte. */
      for (; len != 0; dst++, src++, len--)
	{
	  *dst = *src;
	}
    }

    /**************************************************************************
     *
     * @brief        Basic usage of the rpmsg_send function directly.
     *
     **************************************************************************/
    ssize_t
    RPMsgAgent::write_data(
			   uint8_t* buf,
			   size_t len,
			   TransportRc& transport_rc)
    {
#ifdef GPIO_MONITORING
      /* turns on PIN 1 on GPIO channel 1 (brown)*/
      gpio[1].data = gpio[1].data | 0x2;
#endif
      size_t ret = 0;
      ssize_t bytes_written;

      bytes_written = rpmsg_send(&lept, buf, len);
      if ( 0 < bytes_written )
	ret = size_t(bytes_written);
      else
	{
	  UXR_ERROR("sending data failed with errno", strerror(errno));
          transport_rc = TransportRc::server_error;
	}

#ifdef GPIO_MONITORING
      /* turns off PIN 1 on GPIO channel 1 (brown)*/
      gpio[1].data = gpio[1].data & ~(0x2);
#endif
      return ret;
    }

    /**************************************************************************
     *
     * @brief        Access the buffer populated by the rpmsg calllback.
     *
     **************************************************************************/
    ssize_t
    RPMsgAgent::read_data(
			  uint8_t* buf,
			  size_t len,
			  int timeout,
			  TransportRc& transport_rc)
    {
      while ( rpmsg_rcv_msg_q.empty() )
	platform_poll(platform);

      struct rpmsg_rcv_msg *in_data = rpmsg_rcv_msg_q.front();
      rpmsg_rcv_msg_q.pop_front();

      if ( 0 >= timeout )
	{
	  UXR_WARNING("Read timeout: ", strerror(ETIME));
	  transport_rc = TransportRc::timeout_error;
	  return 0;
	}

      if ( in_data->len == len )
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 1 on GPIO channel 2 (green)*/
	  gpio[2].data = gpio[2].data | 0x2;
#endif
	  aligned_copy(len, in_data->data, buf);

	  /* All data has been used, can release it. */
	  rpmsg_release_rx_buffer(in_data->ept, in_data->full_payload);

#ifdef GPIO_MONITORING
	  /* turns off PIN 1 on GPIO channel 2 (green)*/
	  gpio[2].data = gpio[2].data & ~(0x2);
#endif
	}
      else if ( in_data->len > len )
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 0 on GPIO channel 3 (blue)*/
	  gpio[3].data = gpio[3].data | 0x1;
#endif
	  aligned_copy(len, in_data->data, buf);

	  /* Trunkate the first element of the queue. */
	  in_data->len   -=  len;
	  in_data->data  +=  len;

	  /* Put it back in the front of the queue. */
	  rpmsg_rcv_msg_q.push_front(in_data);

#ifdef GPIO_MONITORING
	  /* turns off PIN 0 on GPIO channel 3 (blue)*/
	  gpio[3].data = gpio[3].data & ~(0x1);
#endif
	}
      else  //if ( in_data->len < len)
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 1 on GPIO channel 3 (purple)*/
	  gpio[3].data = gpio[3].data | 0x2;
#endif
	  aligned_copy(in_data->len, in_data->data, buf);

	  /* All data has been used, can release it. */
	  rpmsg_release_rx_buffer(in_data->ept, in_data->full_payload);

#ifdef GPIO_MONITORING
	  /* turns off PIN 0 on GPIO channel 3 (purple)*/
	  gpio[3].data = gpio[3].data & ~(0x2);
#endif

	  /* Return the length we have. */
	  return in_data->len;
	}

      return len;
    }

    /**************************************************************************
     *
     * @brief        Agent methode to receive messages.
     *
     **************************************************************************/
    bool
    RPMsgAgent::recv_message(
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
      while ( (0 == bytes_read) && (0 < timeout) );

      if ( 0 < bytes_read )
	{
	  input_packet.message.reset(new InputMessage(buffer_,
						      static_cast<size_t>(bytes_read)));
	  input_packet.source = RPMsgEndPoint(remote_addr);
	  ret = true;


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
      return ret;
    }

    /**************************************************************************
     *
     * @brief        Agent methode to send messages.
     *
     **************************************************************************/
    bool
    RPMsgAgent::send_message(
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
      if ( (0 < bytes_written) &&
	   (static_cast<size_t>(bytes_written) == output_packet.message->get_len()) )
	{
	  ret = true;

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
      return ret;
    }

  } // namespace uxr
} // namespace eprosima
