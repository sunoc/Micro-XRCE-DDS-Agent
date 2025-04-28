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
      rpmsg_in_data_t in_data, in_data_trunk;
      std::queue<rpmsg_in_data_t> in_data_q_copy;

      if ( 0 >= timeout )
	{
	  UXR_WARNING("Read timeout: ", strerror(ETIME));
	  transport_rc = TransportRc::timeout_error;
	  return 0;
	}

      while ( in_data_q.empty() )
	platform_poll(platform);

      in_data = in_data_q.front();

      UXR_PRINTF("len", len);
      UXR_PRINTF("in_data.len", in_data.len);
      printf("-----------------\r\n");
      printf("buf: %x\r\n", buf);
      buf[0] = 42;
      printf("buf[0]: %x\r\n", buf[0]);
      printf("-----------------\r\n");
      printf("in_data.pt: %x\r\n", in_data.pt);
      //in_data.pt[0] = 0x81;
      printf("in_data.pt[0]: %x\r\n", in_data.pt[0]);
      printf("-----------------\r\n");

      /* trivial case */
      if ( in_data.len == len )
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 1 on GPIO channel 2 (green)*/
	  gpio[2].data = gpio[2].data | 0x2;
#endif
	  for ( size_t i = 0; i<len; ++i )
	    {
	      UXR_PRINTF("i", i);
	      buf[i] = in_data.pt[i];
	    }

	  in_data_q.pop();
#ifdef GPIO_MONITORING
	  /* turns off PIN 1 on GPIO channel 2 (green)*/
	  gpio[2].data = gpio[2].data & ~(0x2);
#endif
	}
      /* received package "too big" */
      else if ( in_data.len > len )
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 0 on GPIO channel 3 (blue)*/
	  gpio[3].data = gpio[3].data | 0x1;
#endif
	  for ( size_t i = 0; i<len; ++i )
	    {
	      buf[i] = in_data.pt[i];
	    }

	  /* Trunkate the first element of the queue. */
	  in_data_trunk.len =  in_data.len - len;
	  in_data_trunk.pt =  in_data.pt + len;

	  /* Put the new first element in the queue copy. */
	  while ( !in_data_q_copy.empty() )
	    in_data_q_copy.pop();
	  in_data_q_copy.push(in_data_trunk);
	  in_data_q.pop();

	  /* Put the rest of the queue in the copy. */
	  for ( int i = 0; i<(int)in_data_q.size(); i++ )
	    {
	      in_data_q_copy.push(in_data_q.front());
	      in_data_q.pop();
	    }

	  /* Put back the modified queue in the main one. */
	  while ( !in_data_q.empty() )
	    in_data_q.pop();

	  while ( !in_data_q_copy.empty() )
	    {
	      in_data_q.push(in_data_q_copy.front());
	      in_data_q_copy.pop();
	    }

#ifdef GPIO_MONITORING
	  /* turns off PIN 0 on GPIO channel 3 (blue)*/
	  gpio[3].data = gpio[3].data & ~(0x1);
#endif

	}
      /* not enough data received in the first package. */
      else  //if ( in_data.len < len)
	{
#ifdef GPIO_MONITORING
	  /* turns on PIN 1 on GPIO channel 3 (purple)*/
	  gpio[3].data = gpio[3].data | 0x2;
#endif

	  // Read all the available data
	  // WARNING THIS LOOP IS INSANELY SLOW !!!
	  for ( size_t i = 0; i<in_data.len; ++i )
	    {
	      //std::this_thread::sleep_for(std::chrono::microseconds(1));
	      UXR_PRINTF("i", i);
	      buf[i] = in_data.pt[i];
	    }
#ifdef GPIO_MONITORING
	      /* turns off PIN 0 on GPIO channel 3 (purple)*/
	      gpio[3].data = gpio[3].data & ~(0x2);
#endif
	  // pop the "used" data
	  in_data_q.pop();

	  // if there is no more data, return what we got.
	  if ( in_data_q.empty() )
	    return in_data.len;
	  else
	    {

	      // Recursively call the read function
	      // to get more data
	      return read_data(
			       buf+in_data.len,
			       len-in_data.len,
			       timeout,
			       transport_rc) + in_data.len;
	    }
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
