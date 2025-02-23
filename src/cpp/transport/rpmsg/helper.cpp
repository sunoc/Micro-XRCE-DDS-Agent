#include <string.h>
#include <stdio.h>
#include <metal/sys.h>
#include <metal/shmem.h>
#include <metal/device.h>
#include <metal/io.h>


int init_system()
{
	struct metal_init_params metal_param = METAL_INIT_DEFAULTS;

	metal_param.log_level = LOG_DEBUG;
	metal_init(&metal_param);

	return 0;
}

void cleanup_system()
{
	metal_finish();
}
