#ifndef _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_
#define _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_

#include <uxr/agent/transport/rpmsg-lite/RPMsgLiteAgentLinux.hpp>

#include <termios.h>

namespace eprosima {
  namespace uxr {

    class TermiosRPMsgLiteAgent : public RPMsgLiteAgent
    {
    public:
      TermiosRPMsgLiteAgent(
			char const * dev,
			int open_flags,
			termios const & termios_attrs,
			uint8_t addr,
			Middleware::Kind middleware_kind);

      ~TermiosRPMsgLiteAgent();

      void send_shutdown(struct rpmsg_lite_instance * dev,
			 struct rpmsg_lite_endpoint * ept,
			 uint32_t  	              dst);

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

#endif // _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_
