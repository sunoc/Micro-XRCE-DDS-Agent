/*
 * Adapted from open-amp's
 * machine/zynqmp/platform_info.h, merged
 * with platform_info_common.h
 *
 */

#ifndef PLATFORM_INFO_H
#define PLATFORM_INFO_H

#include <openamp/remoteproc.h>
#include <openamp/virtio.h>
#include <openamp/rpmsg.h>

struct vring_ipi_info {
  /* Socket file path */
  const char *path;
  int fd;
  atomic_flag sync;
};

struct remoteproc_priv {
  const char *shm_file;
  int shm_size;
  struct metal_io_region *shm_old_io;
  struct metal_io_region shm_new_io;
  struct remoteproc_mem shm;
  struct vring_ipi_info ipi;
};

#define POLL_STOP 0x1U

/**
 * platform_init - initialize the platform
 *
 * Initialize the platform.
 *
 * @argc: number of arguments
 * @argv: array of the input arguments
 * @platform: pointer to store the platform data pointer
 *
 * return 0 for success or negative value for failure
 */
int platform_init(int argc, char *argv[], void **platform);

/**
 * platform_create_rpmsg_vdev - create rpmsg vdev
 *
 * Create rpmsg virtio device, and return the rpmsg virtio
 * device pointer.
 *
 * @platform: pointer to the private data
 * @vdev_index: index of the virtio device, there can more than one vdev
 *              on the platform.
 * @role: virtio driver or virtio device of the vdev
 * @rst_cb: virtio device reset callback
 * @ns_bind_cb: rpmsg name service bind callback
 *
 * return pointer to the rpmsg virtio device
 */
struct rpmsg_device *
platform_create_rpmsg_vdev(void *platform, unsigned int vdev_index,
			   unsigned int role,
			   void (*rst_cb)(struct virtio_device *vdev),
			   rpmsg_ns_bind_cb ns_bind_cb);

/**
 * platform_poll - platform poll function
 *
 * @platform: pointer to the platform
 *
 * return negative value for errors, otherwise 0.
 */
int platform_poll(void *platform);

/**
 * platform_release_rpmsg_vdev - release rpmsg virtio device
 *
 * @rpdev: pointer to the rpmsg device
 */
void platform_release_rpmsg_vdev(struct rpmsg_device *rpdev, void *platform);

/**
 * platform_cleanup - clean up the platform resource
 *
 * @platform: pointer to the platform
 */
void platform_cleanup(void *platform);


#endif /* PLATFORM_INFO_H */
