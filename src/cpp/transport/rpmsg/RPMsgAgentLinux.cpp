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
#ifdef GPIO_MONITORING
      /* BLUE: turns on PIN 1 on GPIO channel 3 */
      gpio[3].data = gpio[3].data | 0x1;
#endif
      size_t rv = 0;
      ssize_t bytes_written;
      unsigned long long udmabuf_payload;

      /* Reset index for the reading method. */
      udma_head = 0;
      udma_tail = 0;

      /* Put the data in the dma buf. */
      for (size_t i = 0; i<len; i++)
	  ((uint8_t *)udmabuf)[i] = buf[i];

      UXR_PRINTF("sending data", len);
      /* Put the length and physical addr in the rpmsg buf. */
      udmabuf_payload = udma_phys_addr + (len << 32);
      UXR_PRINTF("payload size", sizeof(udmabuf_payload));
      bytes_written = ::write(poll_fd_.fd, &udmabuf_payload, sizeof(udmabuf_payload));

      /* Test if anything was sent. */
      if (0 < bytes_written)
          rv = size_t(bytes_written);
      else
	{
	  UXR_ERROR("sending data failed with errno", strerror(errno));
          transport_rc = TransportRc::server_error;
	}
#ifdef GPIO_MONITORING
      /* BLUE: turns off PIN 1 on GPIO channel 3 */
      gpio[3].data = gpio[3].data & ~(0x1);
#endif
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
#ifdef GPIO_MONITORING
      /* GREEN: turns on PIN 2 on GPIO channel 2 */
      gpio[2].data = gpio[2].data | 0x2;
#endif
      // UXR_PRINTF("expects len", len);
      // UXR_PRINTF("udma_head", udma_head);
      // UXR_PRINTF("udma_tail", udma_tail);
      int rpmsg_buffer_len = 0;
      int attempts = 2000;

      /* Init the UDMABUF related variables. */
      size_t rpmsg_phys_addr = 0;
      ssize_t bytes_read = 0;

      if ( 0 >= timeout ){
	UXR_ERROR("Timeout: ", strerror(errno));
	transport_rc = TransportRc::timeout_error;
	return errno;
      }

      /* =======================================================================
	 This is the case where not enough data is in the buffer. */
      if ( (ssize_t)len > (udma_head - udma_tail) )
	{
	  /* If we need more data, we go and read some */
	  while ( 8 > rpmsg_queue.size() )
	    {
	      rpmsg_buffer_len = read(poll_fd_.fd,
				      rpmsg_buffer,
				      MAX_RPMSG_BUFF_SIZE);

	      /* Push the newly received data to the queue */
	      /* WARNING: for unknown reason, the value of rpmsg_buffer_len
	       * should NOT be checked before this point !!
	       * I can be used in the for loop correctly though. */
	      for ( int i = 0; i<rpmsg_buffer_len; i++ )
		rpmsg_queue.push(rpmsg_buffer[i]);

	      usleep(10);

	      attempts--;
	      if ( 0 >= attempts ) return 0;
	    }

	  /* Getting the physical address back. */
	  for ( int i = 0; i<4; i++ )
	    {
	      rpmsg_phys_addr += (rpmsg_queue.front() << i*8);
	      rpmsg_queue.pop();
	    }

	  /* Getting the data length. */
	  bytes_read = rpmsg_queue.front();
	  rpmsg_queue.pop();

	  /* Emptying the rest of the queue */
	  for ( int i = 0; i<3; i++ )
	    rpmsg_queue.pop();

	  if ( (ssize_t)len > bytes_read )
	    UXR_ERROR("Didn't received enough data...", strerror(errno));

	  if ( rpmsg_phys_addr != udma_phys_addr )
	    UXR_ERROR("Missmatched received phys addr...", strerror(errno));

	}

      /* =======================================================================
	 Actually reading datam from udmabuf and returning it */
      // UXR_PRINTF("rpmsg_phys_addr", rpmsg_phys_addr);
      // UXR_PRINTF("receiving data", bytes_read);

      /* received data move the head value of the buf. */
      udma_head += bytes_read;

      //printf("read_index: %ld\r\n", read_index);
      for ( int i = 0; i<(int)len; i++ )
	{
	  buf[i] = ((uint8_t *)udmabuf + udma_tail)[i];
	  //printf("%d: 0x%x\r\n", i, buf[i]);
	}

      //UXR_PRINTF("returning len", len);

      /* Len bytes were taken. Updating tail. */
      udma_tail += len;
#ifdef GPIO_MONITORING
      /* GREEN: turns off PIN 2 on GPIO channel 2 */
      gpio[2].data = gpio[2].data & ~(0x2);
#endif
      return len;
    }

    /***************************************************************************
     *
     * The recv_message and send_message are the parts that interract with
     * the rest of the ROS2 world and thus should be left alone.
     *
     **************************************************************************/

bool RPMsgAgent::recv_message(
        InputPacket<RPMsgEndPoint>& input_packet,
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
        UXR_PRINTF("recevied some stuff", bytes_read);
        input_packet.message.reset(new InputMessage(buffer_, static_cast<size_t>(bytes_read)));
        input_packet.source = RPMsgEndPoint(remote_addr);
        rv = true;

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
    return rv;
}

bool RPMsgAgent::send_message(
        OutputPacket<RPMsgEndPoint> output_packet,
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
        UXR_PRINTF("try and send some stuff", bytes_written);
        rv = true;

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
    return rv;
}

} // namespace uxr
} // namespace eprosima
