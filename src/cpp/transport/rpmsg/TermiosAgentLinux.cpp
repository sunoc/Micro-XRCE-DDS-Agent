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

    /* To make the linker happy, for some reason... */
    struct rpmsg_endpoint RPMsgAgent::lept;
    int RPMsgAgent::shutdown_req = 0;

    /**************************************************************************
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

    int TermiosRPMsgAgent::rpmsg_endpoint_cb(struct rpmsg_endpoint *ept,
					     void *data, size_t len,
					     uint32_t src, void *priv)
    {
      (void)priv;
      (void)src;

      /* On reception of a shutdown we signal the application to terminate */
      if ((*(unsigned int *)data) == SHUTDOWN_MSG) {
	UXR_PRINTF("shutdown message is received.", NULL);
	shutdown_req = 1;
	return RPMSG_SUCCESS;
      }

      /* Put the data in a buffer for the Agent read methode */
      return RPMSG_SUCCESS;
    }

    void TermiosRPMsgAgent::rpmsg_service_unbind(struct rpmsg_endpoint *ept)
    {
      (void)ept;
      UXR_PRINTF("unexpected Remote endpoint destroy.", NULL);
      shutdown_req = 1;
    }

    bool TermiosRPMsgAgent::init()
    {
      UXR_PRINTF("RPMsg XRCE-DDS INIT", NULL);

      void *platform;
      int argc = 0;
      char **argv = NULL;
      struct rpmsg_device *rpdev;
      int ret;

      UXR_PRINTF("openamp lib version: ", openamp_version());
      UXR_PRINTF("Major: ", openamp_version_major());
      UXR_PRINTF("Minor: ", openamp_version_minor());
      UXR_PRINTF("Patch: ", openamp_version_patch());

      UXR_PRINTF("libmetal lib version: ", metal_ver());
      UXR_PRINTF("Major: ", metal_ver_major());
      UXR_PRINTF("Minor: ", metal_ver_minor());
      UXR_PRINTF("Patch: ", metal_ver_patch());

      UXR_PRINTF("Initializing the platform...", NULL);
      ret = platform_init(argc, argv, &platform);
      if (ret) {
	UXR_ERROR("Failed to initialize platform.", strerror(errno));
	ret = -1;
      } else {
	UXR_PRINTF("Creating vdev...", NULL);
	rpdev = platform_create_rpmsg_vdev(platform, 0,
					   VIRTIO_DEV_DEVICE,
					   NULL, NULL);
	if (!rpdev) {
	  UXR_ERROR("Failed to create rpmsg virtio device.", strerror(errno));
	  ret = -1;
	} else {
	  	UXR_PRINTF("Creating ept...", NULL);
	  ret = rpmsg_create_ept(&lept, rpdev, RPMSG_SERVICE_NAME,
				 RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
				 rpmsg_endpoint_cb,
				 rpmsg_service_unbind);
	  if (ret) {
	    UXR_ERROR("Failed to create endpoint.", strerror(errno));
	    return -1;
	  }

	  UXR_PRINTF("Successfully created rpmsg endpoint.", NULL);
	  UXR_PRINTF("RPMsg TX buffer size: ", rpmsg_get_tx_buffer_size(&lept));
	  UXR_PRINTF("RPMsg RX buffer size: ", rpmsg_get_rx_buffer_size(&lept));
	}
      }

      UXR_PRINTF("RPMsg init is successful.", NULL);
      return true;
    }

    bool TermiosRPMsgAgent::fini()
    {

      UXR_PRINTF("Custom RPMSg Micro XRCE-DDS Agent FINI function.", NULL);

      UXR_PRINTF("Everything was freed and close cleanly.", NULL);
      return true;
    }

    bool TermiosRPMsgAgent::handle_error(
					 TransportRc /*transport_rc*/)
    {
      return fini() && init();
    }

  } // namespace uxr
} // namespace eprosima
