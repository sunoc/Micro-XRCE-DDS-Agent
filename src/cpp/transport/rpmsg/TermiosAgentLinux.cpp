/*
 * This uses a communication system based on open-amp's "rpmsg-ping.c"
 * demo application.
 */

#include <uxr/agent/transport/rpmsg/TermiosAgentLinux.hpp>

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

      UXR_PRINTF("Sending shutdown message.", NULL);

      bytes_sent = ::write(filedescriptor, &umsg, sizeof(umsg));
      if (0 >= bytes_sent){
	UXR_ERROR("Failed to write SHUTDOWN_MSG", strerror(errno));
      }
    }

    int TermiosRPMsgAgent::rpmsg_endpoint_cb(struct rpmsg_endpoint *ept,
					     void *data, size_t len,
					     uint32_t src, void *priv)
    {
      (void)ept;
      (void)priv;
      (void)src;
      rpmsg_in_data_t in_data;
      UXR_PRINTF("Callback is reached", NULL);

      /* On reception of a shutdown we signal the application to terminate */
      if ((*(unsigned int *)data) == SHUTDOWN_MSG) {
	UXR_PRINTF("shutdown message is received.", NULL);
	shutdown_req = 1;
	return RPMSG_SUCCESS;
      }

      /* Put the data in a queue for the Agent read methode. */
      UXR_PRINTF("Received len: ", len);
      UXR_PRINTF("Buffer addr *data: ", data);
      in_data.pt = data;
      in_data.len = len;
      in_data_q.push(in_data);

      return RPMSG_SUCCESS;
    }

    void TermiosRPMsgAgent::rpmsg_service_unbind(struct rpmsg_endpoint *ept)
    {
      (void)ept;
      UXR_PRINTF("unexpected Remote endpoint destroy.", NULL);
      shutdown_req = 1;
    }

    void TermiosRPMsgAgent::rpmsg_name_service_bind_cb(struct rpmsg_device *rdev,
					 const char *name, uint32_t dest)
    {
      UXR_PRINTF("new endpoint notification is received.", NULL);
      if (strcmp(name, RPMSG_SERVICE_NAME))
	UXR_ERROR("Unexpected name service:", name);
      else
	(void)rpmsg_create_ept(&lept, rdev, RPMSG_SERVICE_NAME,
			       RPMSG_ADDR_ANY, dest,
			       (rpmsg_ept_cb)rpmsg_endpoint_cb_wrap,
			       (rpmsg_ns_unbind_cb)rpmsg_service_unbind_wrap);
    }

    bool TermiosRPMsgAgent::init()
    {
      int argc = 1;
      char **argv = NULL;
      int ret, max_size, hello_ret;

      /* micro-ROS first handshake message. */
      unsigned char hello[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

      UXR_PRINTF("Start RPMsg Initialization process...", NULL);
      UXR_PRINTF("openamp lib version: ", openamp_version());
      UXR_PRINTF("libmetal lib version: ", metal_ver());

      UXR_PRINTF("Initializing the platform...", NULL);
      ret = platform_init(argc, argv, &platform);
      if (ret)
	{
	  UXR_ERROR("Failed to initialize platform.", strerror(errno));
	  return false;
	}

      rpdev = platform_create_rpmsg_vdev(platform, 0,
					 VIRTIO_DEV_DRIVER,
					 NULL,
					 (rpmsg_ns_bind_cb)rpmsg_name_service_bind_cb_wrap);
      if (!rpdev)
	{
	  UXR_ERROR("Failed to create rpmsg virtio device.", strerror(errno));
	  return false;
	}

      ret = rpmsg_create_ept(&lept, rpdev, RPMSG_SERVICE_NAME,
			     RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			     (rpmsg_ept_cb)rpmsg_endpoint_cb_wrap,
			     (rpmsg_ns_unbind_cb)rpmsg_service_unbind_wrap);
      if (ret)
	{
	  UXR_ERROR("Failed to create endpoint.", strerror(errno));
	  return false;
	}

      max_size = rpmsg_get_tx_buffer_size(&lept);
      if (max_size <= 0)
	{
	  UXR_ERROR("No available buffer size.", strerror(errno));
	  rpmsg_destroy_ept(&lept);
	  return false;
	}

      i_payload = (uint8_t *)metal_allocate_memory(2 * sizeof(unsigned long) +
						max_size);

      if (!i_payload)
	{
	  UXR_ERROR("memory allocation failed.", strerror(errno));
	  rpmsg_destroy_ept(&lept);
	  return false;
	}

      while (!is_rpmsg_ept_ready(&lept))
	platform_poll(platform);

      hello_ret = rpmsg_trysend(&lept, hello, 10);
      if (0 >= hello_ret)
	{
	  UXR_ERROR("Hello message sending failed.", strerror(errno));
	  rpmsg_destroy_ept(&lept);
	  return false;
	}

      UXR_PRINTF("RPMsg init is successful.", NULL);
      return true;
    }

    bool TermiosRPMsgAgent::fini()
    {
      UXR_PRINTF("Start RPMsg Finishing process...", NULL);

      platform_release_rpmsg_vdev(rpdev, platform);
      platform_cleanup(platform);

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
