#include "tup/monitor.h"

int monitor_supported(void)
{
	return -1;
}

int monitor(int argc, char **argv)
{
	if(argc) {}
	if(argv) {}
	return -1;
}

int stop_monitor(int restarting)
{
	if(restarting) {}
	return -1;
}

int monitor_get_pid(int restarting)
{
	if(restarting) {}
	/* Always return 0 to say that we successfully don't have a monitor
	 * running.
	 */
	return 0;
}
