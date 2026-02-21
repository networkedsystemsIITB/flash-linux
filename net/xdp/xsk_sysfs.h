/* SPDX-License-Identifier: GPL-2.0 */
/* AF_XDP sysfs interface
 * Author: Debojeet Das <debojeetdas@cse.iitb.ac.in>
 */

#ifndef _LINUX_XSK_SYSFS_H
#define _LINUX_XSK_SYSFS_H

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "xsk.h"

/*
 * This is our "object" that we will create on socket creation and register them with
 * sysfs.
 */
struct flash_obj {
    struct kobject kobj;
    int pid;
    char procname[16];
    int ifindex;
    int qid;
    int *next;
    int next_count;
};
#define to_flash_obj(x) container_of(x, struct flash_obj, kobj)

/* Global file for all flash objects */
extern int flash_tx_tracking;

/* a custom attribute that works just for a struct flash_obj. */
struct flash_attribute {
	struct attribute attr;
	ssize_t (*show)(struct flash_obj *obj, struct flash_attribute *attr, char *buf);
	ssize_t (*store)(struct flash_obj *data, struct flash_attribute *attr, const char *buf, size_t count);
};
#define to_flash_attr(x) container_of(x, struct flash_attribute, attr)

/* flash sysfs functions */
struct flash_obj *create_flash_obj(int flash_id, int pid, const char *procname, int ifindex, int qid);
void clear_flash_redr(struct flash_obj *obj);
void destroy_flash_obj(struct flash_obj *obj);
int flash_sysfs_init(void);
void flash_sysfs_exit(void);

#endif /* _LINUX_XSK_SYSFS_H */
