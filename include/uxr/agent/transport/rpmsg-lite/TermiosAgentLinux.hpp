#ifndef _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_
#define _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_

#include <uxr/agent/transport/rpmsg/RPMsgAgentLinux.hpp>

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

#endif // _UXR_AGENT_TRANSPORT_RPMSGLITE_TERMIOSAGENTLINUX_HPP_
