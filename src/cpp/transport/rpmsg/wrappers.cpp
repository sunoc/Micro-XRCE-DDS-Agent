/*
 * These functions are C wrappers around C++ methods.
 * These functions must be called by a C function as a function pointers,
 * thus must be C functions.
 */

#include <uxr/agent/transport/rpmsg/TermiosAgentLinux.hpp>

namespace eprosima {
  namespace uxr {

    extern "C" int rpmsg_endpoint_cb_wrap(void * rpmsgclass,
					  struct rpmsg_endpoint *ept,
					  void *data, size_t len,
					  uint32_t src, void *priv)
    {
      UXR_PRINTF("Callback wrapper is reached", NULL);
      return static_cast<TermiosRPMsgAgent*>(rpmsgclass)->rpmsg_endpoint_cb(ept,
									    data,
									    len,
									    src,
									    priv);
    }

    extern "C" void rpmsg_service_unbind_wrap(void * rpmsgclass,
					      struct rpmsg_endpoint *ept)
    {
      UXR_PRINTF("Unbind wrapper is reached", NULL);
      static_cast<TermiosRPMsgAgent*>(rpmsgclass)->rpmsg_service_unbind(ept);
    }

    extern "C" void rpmsg_name_service_bind_cb_wrap(void * rpmsgclass,
						    struct rpmsg_device *rdev,
						    const char *name,
						    uint32_t dest)
    {
      UXR_PRINTF("Unbind cb wrapper is reached", NULL);
      static_cast<TermiosRPMsgAgent*>(rpmsgclass)->rpmsg_name_service_bind_cb(rdev,
									      name,
									      dest);
    }
  } // namespace uxr
} // namespace eprosima
