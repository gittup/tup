#ifndef tup_monitor_h
#define tup_monitor_h

/** Lock to make sure two monitors don't run at once. Also used to wait for
 * the monitor to actually run before doing stop (which is really only useful
 * during tests where we may start and stop things before the monitor process
 * gets any cpu cycles).
 */
#define TUP_MONITOR_LOCK ".tup/monitor"

int monitor(int argc, char **argv);
int stop_monitor(int argc, char **argv);

#endif
