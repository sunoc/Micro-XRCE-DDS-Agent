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

#ifndef UXR_AGENT_TRANSPORT_RPMSG_TERMIOSAGENTLINUX_HPP_
#define UXR_AGENT_TRANSPORT_RPMSG_TERMIOSAGENTLINUX_HPP_

#include <uxr/agent/transport/rpmsg/RPMsgAgentLinux.hpp>

#include <termios.h>

namespace eprosima {
namespace uxr {

class TermiosRPMsgAgent : public RPMsgAgent
{
public:
    TermiosRPMsgAgent(
            char const * dev,
            int open_flags,
            termios const & termios_attrs,
            uint8_t addr,
            Middleware::Kind middleware_kind);

    ~TermiosRPMsgAgent();

    int getfd() { return poll_fd_.fd; };

    void send_shutdown(int filedescriptor);

    int rpmsg_create_ept(int rpfd,
			 rpmsg_endpoint_info *ept);

    char *get_rpmsg_ept_dev_name(const char *rpmsg_name,
					const char *ept_name,
					char *ept_device_name);
  
    int bind_rpmsg_chrdev(const char *rpmsg_name);
  
    int get_rpmsg_chrdev_fd(const char *rpmsg_name,
				   char *rpmsg_ctrl_name);
  
    void set_src_dst(char *out,
			    rpmsg_endpoint_info *pep);
  
    void lookup_channel(char *out,
			rpmsg_endpoint_info *pep);

private:

    bool init() final;
    bool fini() final;
    bool handle_error(
            TransportRc transport_rc) final;

private:
    const std::string dev_;
    const int open_flags_;
    const termios termios_attrs_;
};

} // namespace uxr
} // namespace eprosima

#endif // UXR_AGENT_TRANSPORT_RPMSG_TERMIOSAGENTLINUX_HPP_
