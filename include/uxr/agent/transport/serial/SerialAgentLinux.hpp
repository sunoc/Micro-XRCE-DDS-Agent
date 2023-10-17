// Copyright 2017-present Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

#ifndef UXR_AGENT_TRANSPORT_SERIAL_SERIALAGENTLINUX_HPP_
#define UXR_AGENT_TRANSPORT_SERIAL_SERIALAGENTLINUX_HPP_

#include <uxr/agent/transport/Server.hpp>
#include <uxr/agent/transport/endpoint/SerialEndPoint.hpp>
#include <uxr/agent/transport/stream_framing/StreamFramingProtocol.hpp>

#include <cstdint>
#include <cstddef>
#include <sys/poll.h>
#include <sys/types.h>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <linux/rpmsg.h>
#include <queue>


/* RPMsg max payload size values*/
#define RPMSG_HEADER_LEN        16
#define MAX_RPMSG_BUFF_SIZE     (512 - RPMSG_HEADER_LEN)
#define PAYLOAD_MIN_SIZE	1
#define PAYLOAD_MAX_SIZE	(MAX_RPMSG_BUFF_SIZE - 24)
#define NUM_PAYLOADS		(PAYLOAD_MAX_SIZE/PAYLOAD_MIN_SIZE)

#define RPMSG_BUS_SYS "/sys/bus/rpmsg"

#define SHUTDOWN_MSG 0xEF56A55A

/* message printing utils */
#define UXR_PRINTF(msg, ...)  UXR_AGENT_LOG_INFO(UXR_DECORATE_GREEN(msg), " {}",  ##__VA_ARGS__)
#define UXR_WARNING(msg, ...) UXR_AGENT_LOG_INFO(UXR_DECORATE_YELLOW(msg), " {}",  ##__VA_ARGS__)
#define UXR_ERROR(msg, ...)   UXR_AGENT_LOG_ERROR(UXR_DECORATE_RED(msg), " {}", ##__VA_ARGS__)

namespace eprosima {
  namespace uxr {

    class SerialAgent : public Server<SerialEndPoint>
    {
    public:
      SerialAgent(
		  uint8_t addr,
		  Middleware::Kind middleware_kind);

#ifdef UAGENT_DISCOVERY_PROFILE
      bool has_discovery() final { return false; }
#endif

#ifdef UAGENT_P2P_PROFILE
      bool has_p2p() final { return false; }
#endif

      /* rpmsg structure */
      rpmsg_endpoint_info eptinfo = {
	"rpmsg-openamp-demo-channel", // name[32]
	0, // src
	0, // dst
      };

    private:

      virtual bool init() = 0;

      virtual bool fini() = 0;

      bool recv_message(
			InputPacket<SerialEndPoint>& input_packet,
			int timeout,
			TransportRc& transport_rc) final;

      bool send_message(
			OutputPacket<SerialEndPoint> output_packet,
			TransportRc& transport_rc) final;

      ssize_t write_data(
			 uint8_t* buf,
			 size_t len,
			 TransportRc& transport_rc);

      ssize_t read_data(
			uint8_t* buf,
			size_t len,
			int timeout,
			TransportRc& transport_rc);

    protected:
      const uint8_t addr_;
      struct pollfd poll_fd_;
      uint8_t buffer_[SERVER_BUFFER_SIZE];
      FramingIO framing_io_;
      int opt;
      int charfd;

      /* RPMsg-specific general variables */
      int ntimes = 1;
      char rpmsg_dev[NAME_MAX];
      char rpmsg_char_name[16];
      char fpath[2*NAME_MAX];

      char ept_dev_name[16];
      char ept_dev_path[32];

      //int32_t rpmsg_buffer_len;
      int32_t rpmsg_buffer_top;
      uint8_t rpmsg_buffer[MAX_RPMSG_BUFF_SIZE];
      std::queue<uint8_t> rpmsg_queue;

    };

  } // namespace uxr
} // namespace eprosima

#endif // UXR_AGENT_TRANSPORT_SERIAL_SERIALAGENTLINUX_HPP_
