#ifndef tup_updater_h
#define tup_updater_h

/** Lock file used to ensure only one updater runs at a time */
#define TUP_UPDATE_LOCK ".tup/updater"

int updater(int argc, char **argv);

#endif
