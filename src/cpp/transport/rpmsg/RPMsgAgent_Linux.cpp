// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    //, rpmsg_buffer_len{0}
      , rpmsg_buffer_top{0}
    //  , rpmsg_leftover_len{0}
      , rpmsg_queue{}
    {}

    ssize_t RPMsgAgent::write_data(
				    uint8_t* buf,
				    size_t len,
				    TransportRc& transport_rc)
    {
      size_t rv = 0;
      ssize_t bytes_written = ::write(poll_fd_.fd, buf, len);
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

    ssize_t RPMsgAgent::read_data(
				   uint8_t* buf,
				   size_t len,
				   int timeout,
				   TransportRc& transport_rc)
    {
      int rpmsg_buffer_len = 0;
      int attempts = 10;

      if ( 0 >= timeout ){
	UXR_ERROR("Timeout: ", strerror(errno));
	transport_rc = TransportRc::timeout_error;
	return errno;
      }

      /* If we need more data, we go and read some */
      while ( len > rpmsg_queue.size() ) {
	rpmsg_buffer_len = read(poll_fd_.fd, rpmsg_buffer, MAX_RPMSG_BUFF_SIZE);

	/* Push the newly received data to the queue */
	for ( int i = 0; i<rpmsg_buffer_len; i++ ) {
	  rpmsg_queue.push(rpmsg_buffer[i]);
	}

	usleep(100);

	attempts--;
	if ( 0 >= attempts ) return 0;
      }

      for ( int i = 0; i<(int)len; i++ ) {
	buf[i] = rpmsg_queue.front();
	rpmsg_queue.pop();
	//UXR_PRINTF("data put in buf:", buf[i]);
      }

      return len;
    }

    bool RPMsgAgent::recv_message(
				   InputPacket<RPMsgEndPoint>& input_packet,
				   int timeout,
				   TransportRc& transport_rc)
    {
      bool rv = false;
      uint8_t remote_addr = 0x00;
      ssize_t bytes_read = 0;

      //UXR_PRINTF("Entering method", NULL);

      do
	{
	  /*
	    Something's wrong, I can feel it.
	    There is a problem with the stream framing protocol, where
	    where the read_frames_msg is defined
	  */
	  //UXR_PRINTF("Read data loop", NULL);
	  bytes_read = framing_io_.read_framed_msg(
						   buffer_, SERVER_BUFFER_SIZE, remote_addr, timeout, transport_rc);
	  //UXR_PRINTF("Timeout:", timeout);
	  //UXR_PRINTF("bytes_read:", bytes_read);
	}
      while ((0 == bytes_read) && (0 < timeout));

      //UXR_PRINTF("BP1", NULL);

      if (0 < bytes_read)
	{
	  input_packet.message.reset(new InputMessage(buffer_, static_cast<size_t>(bytes_read)));
	  input_packet.source = RPMsgEndPoint(remote_addr);
	  rv = true;

	  //UXR_PRINTF("BP2", NULL);

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

      //UXR_PRINTF("Entering method", NULL);

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