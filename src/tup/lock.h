#ifndef tup_lock_h
#define tup_lock_h

/** Tri-lock */
#define TUP_SHARED_LOCK ".tup/shared"
#define TUP_OBJECT_LOCK ".tup/object"
#define TUP_TRI_LOCK ".tup/tri"

/** Initializes the shared tup object lock. This allows multiple readers to
 * access tup. If you want exclusive access, you'll need to up the lock
 * to LOCK_EX by getting the file descriptor from tup_obj_lock().
 */
int tup_lock_init(void);

/** Unlocks the object lock and closes the file descriptor. It seems if the
 * OS is left to clean up the lock, it issues a close event before the lock
 * actually becomes available again.
 */
void tup_lock_exit(void);

/** Just closes the locks. This should by called by any forked processes. */
void tup_lock_close(void);

/* Tri-lock functions */
int tup_sh_lock(void);
int tup_obj_lock(void);
int tup_tri_lock(void);

/* Wrappers for fcntl */
int tup_wait_flock(int fd);

#endif
