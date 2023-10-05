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

#include <uxr/agent/transport/serial/SerialAgentLinux.hpp>
#include <uxr/agent/utils/Conversion.hpp>
#include <uxr/agent/logger/Logger.hpp>

#include <unistd.h>

namespace eprosima {
  namespace uxr {

    SerialAgent::SerialAgent(
			     uint8_t addr,
			     Middleware::Kind middleware_kind)
      : Server<SerialEndPoint>{middleware_kind}
      , addr_{addr}
      , poll_fd_{}
      , buffer_{0}
      , framing_io_(
		    addr,
		    std::bind(&SerialAgent::write_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
		    std::bind(&SerialAgent::read_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4))
      , opt{}
      , charfd{}
      , rpmsg_buffer_len{0}
      , rpmsg_buffer_top{0}
    {}

    ssize_t SerialAgent::write_data(
				    uint8_t* buf,
				    size_t len,
				    TransportRc& transport_rc)
    {
      // size_t rv = 0;
      // ssize_t bytes_written = ::write(poll_fd_.fd, buf, len);
      // if (0 < bytes_written)
      // {
      //     rv = size_t(bytes_written);
      // }
      // else
      // {
      //     transport_rc = TransportRc::server_error;
      // }
      // return rv;

      UXR_PRINTF("Custom RPMSg Micro XRCE-DDS Agent write_data function", NULL);					    
      
      ssize_t bytes_sent = -1;

      bytes_sent = ::write(poll_fd_.fd, buf, len);
      if (0 <= bytes_sent) {
	UXR_PRINTF("Sent payload of size", len);
	return size_t(bytes_sent);
      } else {
	UXR_ERROR("sending data failed with errno", strerror(errno));
	transport_rc = TransportRc::server_error;
	return -1;
      }
    }

    ssize_t SerialAgent::read_data(
				   uint8_t* buf,
				   size_t len,
				   int timeout,
				   TransportRc& transport_rc)
    {
      // ssize_t bytes_read = 0;
      // int poll_rv = poll(&poll_fd_, 1, timeout);
      // if(poll_fd_.revents & (POLLERR+POLLHUP))
      // {
      //     transport_rc = TransportRc::server_error;;
      // }
      // else if (0 < poll_rv)
      // {
      //     bytes_read = read(poll_fd_.fd, buf, len);
      //     if (0 > bytes_read)
      //     {
      //         transport_rc = TransportRc::server_error;
      //     }
      // }
      // else
      // {
      //     transport_rc = (poll_rv == 0) ? TransportRc::timeout_error : TransportRc::server_error;
      // }
      // return bytes_read;

      //UXR_PRINTF("Custom RPMSg Micro XRCE-DDS Agent read_data function", NULL);


      
      
      /* checks the given timeout value */
      if ( 0 >= timeout){
	UXR_ERROR("Timeout value < 0", strerror(errno));
	transport_rc = TransportRc::timeout_error;
	return errno;
      }

      //ssize_t bytes_read = 0;

      // UXR_PRINTF("Trying to poll the fd", NULL);
      // int poll_rv = poll(&poll_fd_, 1, timeout);
      // if( poll_fd_.revents & (POLLERR+POLLHUP) ) {
      // 	UXR_ERROR("Polling the fd failed", strerror(errno));
      // 	transport_rc = TransportRc::server_error;;
      // } else if ( 0 < poll_rv ) {
      // 	UXR_PRINTF("poll return value", poll_rv);
      // } else {
      // 	UXR_PRINTF("poll return value", poll_rv);
      // }

      // UXR_PRINTF("Trying to read data until something is received.", timeout);
      // //while (0 >= bytes_read && 0 < timeout ) {
      // bytes_read = read(poll_fd_.fd, buf, len);
      // while ( 0 >= bytes_read ){
      // 	usleep(1000);
      // 	bytes_read = read(poll_fd_.fd, buf, len);
      // }

	// don't use the sleep, should just block the read
	//usleep(timeout);
	// timeout--;
	//}

      /* When the rpmsg buffer is empty,
       * we try and read some data */
      if ( 0 == rpmsg_buffer_len ){
	//UXR_PRINTF("Buffer empty", rpmsg_buffer_len);

	/* Try and read the maximum size of an RPMsg*/
	do {
	  rpmsg_buffer_len = read(poll_fd_.fd, rpmsg_buffer, MAX_RPMSG_BUFF_SIZE);

	  /* If an error code is received from the read function*/
	  if ( 0 > rpmsg_buffer_len ){
	    rpmsg_buffer_len = 0;
	    return 0;
	  }
	  
	  usleep(100);
	} while ( 0 >= rpmsg_buffer_len);
      }

      /* Then, if enough data for requested len were received */
      if ( 0 < rpmsg_buffer_len ){
	UXR_PRINTF("Something was received", rpmsg_buffer_len);
	uint32_t copylen = ((size_t)rpmsg_buffer_len >= len) ? len  : rpmsg_buffer_len;
	
	for ( int i = 0; i<(int)copylen; i++ ){
	  buf[i] = rpmsg_buffer[i+rpmsg_buffer_top];
	  UXR_PRINTF("data:", buf[i]);
	}

	/* Update the rpmsg buffer variables */
	rpmsg_buffer_len -= copylen;
	/* If the buffer was emptied */
	if ( 0 == rpmsg_buffer_len ){
	  rpmsg_buffer_top = 0;

	  /* Otherwise, we keep the start of the rest of the data
	   * in the "top" variable. */
	} else {
	  rpmsg_buffer_top += copylen;
	}

	/* In this case, we always return len since the whole lenght of the
	 * expected data was read */
	return copylen;

      } else {
	UXR_PRINTF("Thou shalt not pass here", NULL);
	transport_rc = TransportRc::timeout_error;
	return rpmsg_buffer_len;
      }

      

      // /* Check if something was received */
      // // if ( 0 > timeout && 0 >= bytes_read ){
      // if ( 0 >= bytes_read ){
      // 	UXR_WARNING("Read function didn't received anything.", bytes_read);
      // 	transport_rc = (bytes_read == 0) ? TransportRc::timeout_error : TransportRc::server_error;
      // } else {
      // 	for ( int i = 0; i<(int)bytes_read; i++ ){
      // 	  UXR_PRINTF("data:", buf[i]);
      // 	}
      // }

      // UXR_PRINTF("Received payload of size: ", bytes_read);
      // return bytes_read;


      // ssize_t bytes_read = 0;
      // int poll_rv = poll(&poll_fd_, 1, timeout);
      // if( poll_fd_.revents & (POLLERR+POLLHUP) ) {
      // 	UXR_ERROR("Polling the fd failed", strerror(errno));
      // 	transport_rc = TransportRc::server_error;;
      // } else if ( 0 < poll_rv ) {
      // 	bytes_read = read(poll_fd_.fd, buf, len);

      // 	for ( int i = 0; i<(int)len; i++ ){
      // 	  UXR_PRINTF("data:", buf[i]);
      // 	}
	
      // 	if (0 > bytes_read)
      //     {
      // 	    UXR_ERROR("Failed to read data", strerror(errno));
      // 	    transport_rc = TransportRc::server_error;
      //     }
      // } else if ( 0 == poll_rv ) {
      // 	UXR_WARNING("Read function timed out.", timeout);
      // 	transport_rc = TransportRc::timeout_error;
      // } else {
      // 	UXR_ERROR("Server error", strerror(errno));
      // 	transport_rc =  TransportRc::server_error;
      // }
      // UXR_PRINTF("Received payload of size: ", bytes_read);
      // return bytes_read;
    }

    bool SerialAgent::recv_message(
				   InputPacket<SerialEndPoint>& input_packet,
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
	  input_packet.source = SerialEndPoint(remote_addr);
	  rv = true;

	  //UXR_PRINTF("BP2", NULL);
	  
	  uint32_t raw_client_key;
	  if (Server<SerialEndPoint>::get_client_key(input_packet.source, raw_client_key))
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

    bool SerialAgent::send_message(
				   OutputPacket<SerialEndPoint> output_packet,
				   TransportRc& transport_rc)
    {

      UXR_PRINTF("Entering method", NULL);
            
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
	  if (Server<SerialEndPoint>::get_client_key(output_packet.destination, raw_client_key))
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
