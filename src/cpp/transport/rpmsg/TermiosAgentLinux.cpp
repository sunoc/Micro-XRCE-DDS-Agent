#include <uxr/agent/transport/rpmsg/TermiosAgentLinux.hpp>

#include <fcntl.h>
#include <unistd.h>

namespace eprosima {
namespace uxr {

TermiosRPMsgAgent::TermiosRPMsgAgent(
        char const* dev,
        int open_flags,
        termios const& termios_attrs,
        uint8_t addr,
        Middleware::Kind middleware_kind)
    : RPMsgAgent(addr, middleware_kind)
    , dev_{dev}
    , open_flags_{open_flags}
    , termios_attrs_{termios_attrs}
{
}

TermiosRPMsgAgent::~TermiosRPMsgAgent()
{
    try
    {
        stop();
    }
    catch (std::exception& e)
    {
        UXR_AGENT_LOG_CRITICAL(
            UXR_DECORATE_RED("error stopping server"),
            "exception: {}",
            e.what());
    }
}


/*******************************************************************************
*
* @brief        This function goal is to send a shutdown package
*               to the micro-ROS client in order to stop it.
*
* @param	fd: the file descriptor for where to send the message
*
* @return	void
*
* @note		None.
*
*******************************************************************************/
void TermiosRPMsgAgent::send_shutdown(int filedescriptor)
{
  ssize_t bytes_sent = -1;
  unsigned int umsg[8] = {
      SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG,
      SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG
  };

  bytes_sent = ::write(filedescriptor, &umsg, sizeof(umsg));
  if (0 >= bytes_sent){
    UXR_ERROR("Failed to write SHUTDOWN_MSG", strerror(errno));
  }
}

bool TermiosRPMsgAgent::init()
{
    UXR_PRINTF("RPMsg XRCE-DDS INIT", NULL);

    int ret;

    UXR_PRINTF("RPMsg init is successful.", NULL);
    return true;
}

bool TermiosRPMsgAgent::fini()
{

  UXR_PRINTF("Custom RPMSg Micro XRCE-DDS Agent FINI function.", NULL);

  UXR_PRINTF("Everything was freed and close cleanly. Returning true.", NULL);
  return true;
}

bool TermiosRPMsgAgent::handle_error(
        TransportRc /*transport_rc*/)
{
    return fini() && init();
}

} // namespace uxr
} // namespace eprosima
