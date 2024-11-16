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
struct exnfc_obj {
    struct kobject kobj;
    int pid;
    char procname[16];
    int ifindex;
    int qid;
    int next;
};
#define to_exnfc_obj(x) container_of(x, struct exnfc_obj, kobj)

/* a custom attribute that works just for a struct exnfc_obj. */
struct exnfc_attribute {
	struct attribute attr;
	ssize_t (*show)(struct exnfc_obj *obj, struct exnfc_attribute *attr, char *buf);
	ssize_t (*store)(struct exnfc_obj *data, struct exnfc_attribute *attr, const char *buf, size_t count);
};
#define to_exnfc_attr(x) container_of(x, struct exnfc_attribute, attr)

/* exnfc sysfs functions */
struct exnfc_obj *create_exnfc_obj(int exnfc_id, int pid, const char *procname, int ifindex, int qid);
void destroy_exnfc_obj(struct exnfc_obj *obj);
int exnfc_sysfs_init(void);

#endif /* _LINUX_XSK_SYSFS_H */
