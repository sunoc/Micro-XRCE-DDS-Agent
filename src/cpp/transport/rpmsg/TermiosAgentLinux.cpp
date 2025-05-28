#include <cstdint>
#include <openamp/rpmsg.h>
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

    /* Static global variables init. */
    struct rpmsg_endpoint RPMsgAgent::lept;
    int RPMsgAgent::shutdown_req;

    /* Read message queue variables */
    std::deque<rpmsg_rcv_msg> RPMsgAgent::rpmsg_rcv_msg_q;

#ifdef GPIO_MONITORING
    int RPMsgAgent::GPIO_fd;
    GPIO_t* RPMsgAgent::gpio;
#endif

    /**************************************************************************
     *
     * @brief        RPMsg input data callback methode.
     *
     **************************************************************************/
    int
    TermiosRPMsgAgent::rpmsg_endpoint_cb(struct rpmsg_endpoint *ept,
					 void *data, size_t len,
					 uint32_t src, void *priv)
    {
#ifdef GPIO_MONITORING
      /* turns on PIN 0 on GPIO channel 2 (yellow)*/
      gpio[2].data = gpio[2].data | 0x1;
#endif
      (void)src;
      (void)priv;

      struct rpmsg_rcv_msg pl;

      rpmsg_hold_rx_buffer(ept, data);
      pl.ept  = ept;
      pl.data = (uint8_t *)data;
      pl.len  = len;
      pl.full_payload = data;

      rpmsg_rcv_msg_q.push_back(pl);

#ifdef GPIO_MONITORING
      /* turns off PIN 0 on GPIO channel 2 (yellow)*/
      gpio[2].data = gpio[2].data & ~(0x1);
#endif
      return RPMSG_SUCCESS;
    }

    /**************************************************************************
     *
     * @brief        RPMsg unbinding notification.
     *
     **************************************************************************/
    void
    TermiosRPMsgAgent::rpmsg_service_unbind(struct rpmsg_endpoint *ept)
    {
      (void)ept;
      UXR_PRINTF("unexpected Remote endpoint destroy.", NULL);
      shutdown_req = 1;
    }

    /**************************************************************************
     *
     * @brief        RPMsg input data callback methode.
     *
     **************************************************************************/
    void
    TermiosRPMsgAgent::rpmsg_name_service_bind_cb(struct rpmsg_device *rdev,
						  const char *name,
						  uint32_t dest)
    {
      UXR_PRINTF("new endpoint notification is received.", NULL);
      if ( strcmp(name, RPMSG_SERVICE_NAME) )
	UXR_ERROR("Unexpected name service:", name);
      else
	(void)rpmsg_create_ept(&lept, rdev, RPMSG_SERVICE_NAME,
			       RPMSG_ADDR_ANY, dest,
			       TermiosRPMsgAgent::rpmsg_endpoint_cb,
			       TermiosRPMsgAgent::rpmsg_service_unbind);
    }

    /**************************************************************************
     *
     * @brief        Main setup methodef for user-space RPMsg.
     *
     **************************************************************************/
    bool
    TermiosRPMsgAgent::init()
    {
      int argc = 1;
      char **argv = NULL;
      int ret, max_size, hello_ret;

      /* micro-ROS first handshake message. */
      unsigned char hello[10] = {42, 42, 42, 42, 42, 42, 42, 42, 42, 42};

#ifdef GPIO_MONITORING
      UXR_PRINTF("GPIO init....", NULL);
      GPIO_fd = open("/dev/mem", O_RDWR | O_SYNC);
      if (GPIO_fd <= 0)
	{
	  UXR_ERROR("Error opening /dev/mem.", strerror(errno));
	  return false;
	}

      gpio = static_cast<GPIO_t*>(mmap(nullptr, gpio_size, PROT_READ|PROT_WRITE, MAP_SHARED, GPIO_fd, gpio_base));
      if (!gpio)
	{
	  UXR_ERROR("Error using mmap.", strerror(errno));
	  close(GPIO_fd);
	  return false;
	}

      UXR_PRINTF("GPIO init successfully!", NULL);
#endif

      UXR_PRINTF("Start RPMsg Initialization process...", NULL);
      UXR_PRINTF("openamp lib version: ", openamp_version());
      UXR_PRINTF("libmetal lib version: ", metal_ver());

      UXR_PRINTF("Initializing the platform...", NULL);
      ret = platform_init(argc, argv, &platform);
      if ( ret )
	{
	  UXR_ERROR("Failed to initialize platform.", strerror(errno));
	  return false;
	}

      rpdev = platform_create_rpmsg_vdev(platform, 0,
					 VIRTIO_DEV_DRIVER,
					 NULL,
					 TermiosRPMsgAgent::rpmsg_name_service_bind_cb);
      if ( !rpdev )
	{
	  UXR_ERROR("Failed to create rpmsg virtio device.", strerror(errno));
	  return false;
	}

      ret = rpmsg_create_ept(&lept, rpdev, RPMSG_SERVICE_NAME,
			     RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			     TermiosRPMsgAgent::rpmsg_endpoint_cb,
			     TermiosRPMsgAgent::rpmsg_service_unbind);
      if ( ret )
	{
	  UXR_ERROR("Failed to create endpoint.", strerror(errno));
	  return false;
	}

      max_size = rpmsg_get_tx_buffer_size(&lept);
      if ( max_size <= 0 )
	{
	  UXR_ERROR("No available buffer size.", strerror(errno));
	  rpmsg_destroy_ept(&lept);
	  return false;
	}

      while ( !is_rpmsg_ept_ready(&lept) )
	platform_poll(platform);

      hello_ret = rpmsg_send(&lept, hello, 10);
      if ( 0 >= hello_ret )
	{
	  UXR_ERROR("Hello message sending failed.", strerror(errno));
	  fini();
	  return false;
	}

      UXR_PRINTF("RPMsg init is successful.", NULL);
      return true;
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
    void
    TermiosRPMsgAgent::send_shutdown(int filedescriptor)
    {
      ssize_t bytes_sent = -1;
      unsigned int umsg[8] =
	{
	  SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG,
	  SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG, SHUTDOWN_MSG
	};

      UXR_WARNING("Sending shutdown message.", NULL);

      bytes_sent = ::write(filedescriptor, &umsg, sizeof(umsg));
      if ( 0 >= bytes_sent )
	{
	  UXR_ERROR("Failed to write SHUTDOWN_MSG", strerror(errno));
	}
    }

    /**************************************************************************
     *
     * @brief        Terminates RPMsg.
     *
     **************************************************************************/
    bool
    TermiosRPMsgAgent::fini()
    {
      UXR_PRINTF("Start RPMsg Finishing process...", NULL);

      rpmsg_destroy_ept(&lept);

      platform_release_rpmsg_vdev(rpdev, platform);
      platform_cleanup(platform);

      UXR_PRINTF("Everything was freed and close cleanly.", NULL);
      return true;
    }

    /**************************************************************************
     *
     * @brief        "Reset button".
     *
     **************************************************************************/
    bool
    TermiosRPMsgAgent::handle_error(TransportRc /*transport_rc*/)
    {
      return fini() && init();
    }

  } // namespace uxr
} // namespace eprosima
