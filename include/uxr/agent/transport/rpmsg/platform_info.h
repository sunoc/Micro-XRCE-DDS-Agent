/*
 * Adapted from open-amp's
 * machine/zynqmp/platform_info.h, merged
 * with platform_info_common.h
 *
 */

#ifndef PLATFORM_INFO_H_
#define PLATFORM_INFO_H_

#include <openamp/remoteproc.h>
#include <openamp/virtio.h>
#include <openamp/rpmsg.h>

#include <uxr/agent/transport/rpmsg/platform_info_common.h>

#if defined __cplusplus
extern "C" {
#endif

struct remoteproc_priv {
	const char *ipi_name; /**< IPI device name */
	const char *ipi_bus_name; /**< IPI bus name */
	const char *rsc_name; /**< rsc device name */
	const char *rsc_bus_name; /**< rsc bus name */
	const char *shm_name; /**< shared memory device name */
	const char *shm_bus_name; /**< shared memory bus name */
	struct metal_device *ipi_dev; /**< pointer to IPI device */
	struct metal_io_region *ipi_io; /**< pointer to IPI i/o region */
	struct metal_device *shm_dev; /**< pointer to shared memory device */
	struct metal_io_region *shm_io; /**< pointer to sh mem i/o region */

	struct remoteproc_mem shm_mem; /**< shared memory */
	unsigned int ipi_chn_mask; /**< IPI channel mask */
	atomic_int ipi_nokick;
};

#define RPU_CPU_ID          0 /* RPU remote CPU Index. We only talk to
			       * one CPU in the example. We set the CPU
			       * index to 0.
			       */
#define IPI_CHN_BITMASK	    0x00000100
#define IPI_DEV_NAME	    "ff340000.ipi"

#define DEV_BUS_NAME        "platform" /* device bus name. "platform" bus
                                        * is used in Linux kernel for generic
					* devices */

#define SHM_DEV_NAME        "3ed20000.shm" /* shared device name */
#define RSC_MEM_PA          0x3ED20000UL
#define RSC_MEM_SIZE        0x2000UL
#define VRING_MEM_PA        0x3ED40000UL
#define VRING_MEM_SIZE      0x8000UL
#define SHARED_BUF_PA       0x3ED48000UL
#define SHARED_BUF_SIZE     0x40000UL

#define OCM_RAM_BASE_ADDR   0xFF340000UL

#define _rproc_wait() metal_cpu_yield()

#if defined __cplusplus
}
#endif

#endif /* PLATFORM_INFO_H_ */
