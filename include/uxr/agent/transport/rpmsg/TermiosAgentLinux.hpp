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
