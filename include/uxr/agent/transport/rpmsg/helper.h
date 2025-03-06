#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdint.h>
#include <metal/sys.h>
#include <openamp/remoteproc.h>
#include <openamp/virtio.h>
#include <openamp/rpmsg.h>

#ifdef __cplusplus
extern "C" {
#endif


  int rpmsg_endpoint_cb_wrap(void * rpmsgclass,
			     struct rpmsg_endpoint *ept, void *data, size_t len,
			     uint32_t src, void *priv);

  void rpmsg_service_unbind_wrap(void * rpmsgclass,
				 struct rpmsg_endpoint *ept);

  void rpmsg_name_service_bind_cb_wrap(void * rpmsgclass,
				       struct rpmsg_device *rdev,
				       const char *name, uint32_t dest);

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
