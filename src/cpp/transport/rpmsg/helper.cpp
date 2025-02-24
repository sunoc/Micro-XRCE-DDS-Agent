#include <string.h>
#include <stdio.h>
#include <metal/sys.h>

int init_system()
{
	struct metal_init_params metal_param = METAL_INIT_DEFAULTS;

	metal_init(&metal_param);

	return 0;
}

void cleanup_system()
{
	metal_finish();
}
