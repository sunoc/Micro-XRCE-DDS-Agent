#include <uxr/agent/transport/rpmsg-lite/TermiosAgentLinux.hpp>

#include <fcntl.h>
#include <unistd.h>

namespace eprosima {
  namespace uxr {

    TermiosRPMsgLiteAgent::TermiosRPMsgLiteAgent(
						 char const* dev,
						 int open_flags,
						 termios const& termios_attrs,
						 uint8_t addr,
						 Middleware::Kind middleware_kind)
      : RPMsgLiteAgent(addr, middleware_kind)
      , dev_{dev}
      , open_flags_{open_flags}
      , termios_attrs_{termios_attrs}
    {
    }

    TermiosRPMsgLiteAgent::~TermiosRPMsgLiteAgent()
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


    /***************************************************************************
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
     **************************************************************************/
    void TermiosRPMsgLiteAgent::send_shutdown(struct rpmsg_lite_instance * dev,
					      struct rpmsg_lite_endpoint * ept,
					      uint32_t  	           dst);
    {
      int32_t ret;
      unsigned int umsg[8] = {
	SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG,
	SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG
      };

      ret = rpmsg_lite_send_nocopy(dev, ept, dst, &umsg, sizeof(umsg));
      if (0 != ret){
	UXR_ERROR("Failed to send SHUTDOWN_MSG", strerror(errno));
      }
    }


    /***************************************************************************
     *
     * @brief        Starts the RPMsg-Lite system.
     *
     * @param	out:
     *
     * @param	pep:
     *
     * @return	None.
     *
     * @note		None.
     *
     **************************************************************************/
    bool TermiosRPMsgLiteAgent::init()
    {
      UXR_PRINTF("RPMSG-LITE INIT", NULL);

      /* Main initialization. */
      rpmsg_lite_dev = rpmsg_lite_master_init();
      if ( RL_NULL == rpmsg_lite_devt )
      	UXR_ERROR("Unable to init RPMsg-Lite Master.", strerror(errno));



      /* Create RPMsg-lite end-point. */
      rpmsg_lite_ept = rpmsg_lite_create_ept(rpmsg_lite_dev);
      if ( RL_NULL == rpmsg_lite_ept )
      	UXR_ERROR("Unable to create RPMsg-Lite endpoint.", strerror(errno));

      return true;
    }

    /***************************************************************************
     *
     * @brief        Ends the RPMsg-Lite system.
     *
     * @param	None.
     *
     * @return	None.
     *
     * @note		None.
     *
     **************************************************************************/
    bool TermiosRPMsgLiteAgent::fini()
    {

      UXR_PRINTF("RPMSG-LITE FINI", NULL);


      send_shutdown(rpmsg_lite_dev, rpmsg_lite_ept, 0);

      /* Get rid of the endpoint. */
      rpmsg_lite_destroy_ept(rpmsg_lite_dev,
			     rpmsg_lite_ept);

      UXR_PRINTF("Everything was freed and close cleanly.", NULL);
      return true;
    }

    bool TermiosRPMsgLiteAgent::handle_error(
					     TransportRc /*transport_rc*/)
    {
      return fini() && init();
    }

  } // namespace uxr
} // namespace eprosima
