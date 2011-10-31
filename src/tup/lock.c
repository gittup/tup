/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2008-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _ATFILE_SOURCE
#include "lock.h"
#include "flock.h"
#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/* So...the tri-lock business. There are three locks. They lock one thing - the
 * database. It also lets the monitor ignore the file accesses while an update
 * happens. I think they may actually all be necessary. Here's how it works,
 * more or less:
 *
 * If we're not running the monitor, everything is pretty simple - everybody
 * waits on the shared lock until whoever has that lock is done with it. They
 * keep the shared lock for their whole operation, so they can do whatever they
 * want with the database. It's called the shared lock because it used to be
 * actually shared (as in, read-only). However that makes things confusing. Now
 * it's just a mutex to make sure only one process opens/closes the object lock
 * file at a time. In theory it should be renamed to only_one_dude_can_use_the_
 * object_lock_file_at_a_time_lock, but that would be stupid.
 *
 * Now if we run the monitor, it comes in here and eventually gets the shared
 * and object locks, so it is king of the universe. The monitor, as part of its
 * initialization, will put a watch on the object lock file. It will then
 * remove its shared lock. This means anyone else who was waiting on the shared
 * lock can go and open the object lock file, which notifies the monitor. Note
 * if we didn't have the shared lock, we could have two processes open the
 * object lock and block on the flock. Then when the monitor tries to get the
 * lock again, it may succeed, but not know about the second process because
 * the file open already happened. One reason this may happen is inotify can
 * slurp duplicate events (two opens on the same file by different processes).
 *
 * So the monitor got the file notification on the object lock, it will flush
 * its queue and then take the tri lock, and release the object lock. The only
 * process waiting on the object lock is the one who has the shared lock, so he
 * gets to go. When he finishes he will release the object lock and close it.
 * The file close notification informs the monitor that it should take the
 * object lock again. Once it has the lock, the monitor will remove the tri
 * lock, letting the other process finish its unlocking. The other process then
 * drops the shared lock, so the next guy can go and open the object lock (and
 * inform the monitor again). Essentially the tri lock gives the monitor the
 * highest priority in obtaining the object lock once it is available again.
 * This way we can be sure we don't miss any events in between two subsequent
 * processes.
 *
 * With the locks as they are, the monitor doesn't drop events in between other
 * processes trying to go, and also makes sure it doesn't miss someone trying
 * to get the object lock. I ran t7005 in a loop like a million times to verify
 * it. I suppose a formal proof would be nice, but I say meh! to you. With
 * different locking mechanisms, I would sometimes get a deletion event from
 * the updater, but the monitor would read it as if it were outside the updater
 * and also try to delete the file. Another issue would be deadlocking from the
 * monitor not detecting someone trying to use the object lock. With three
 * locks I don't get those issues.
 */

static int sh_lock;
static int obj_lock;
static int tri_lock;

int tup_lock_init(void)
{
	sh_lock = openat(tup_top_fd(), TUP_SHARED_LOCK, O_RDWR);
	if(sh_lock < 0) {
		perror(TUP_SHARED_LOCK);
		return -1;
	}
	if(tup_flock(sh_lock) < 0) {
		return -1;
	}

	obj_lock = openat(tup_top_fd(), TUP_OBJECT_LOCK, O_RDWR);
	if(obj_lock < 0) {
		perror(TUP_OBJECT_LOCK);
		return -1;
	}
	if(tup_flock(obj_lock) < 0) {
		return -1;
	}

	tri_lock = openat(tup_top_fd(), TUP_TRI_LOCK, O_RDWR);
	if(tri_lock < 0) {
		perror(TUP_TRI_LOCK);
		return -1;
	}
	return 0;
}

void tup_lock_exit(void)
{
	tup_unflock(obj_lock);
	if(close(obj_lock) < 0)
		perror("close(obj_lock)");
	/* Wait for the monitor to pick up the object lock */
	tup_flock(tri_lock);
	tup_unflock(tri_lock);
	if(close(tri_lock) < 0)
		perror("close(tri_lock)");
	tup_unflock(sh_lock);
	if(close(sh_lock) < 0)
		perror("close(sh_lock)");
}

void tup_lock_close(void)
{
	if(close(obj_lock) < 0)
		perror("close(obj_lock)");
	if(close(tri_lock) < 0)
		perror("close(tri_lock)");
	if(close(sh_lock) < 0)
		perror("close(sh_lock)");
}

int tup_sh_lock(void)
{
	return sh_lock;
}

int tup_obj_lock(void)
{
	return obj_lock;
}

int tup_tri_lock(void)
{
	return tri_lock;
}
