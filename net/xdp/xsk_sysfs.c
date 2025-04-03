/* SPDX-License-Identifier: GPL-2.0 */
/* AF_XDP sysfs interface
 * Author: Debojeet Das <debojeetdas@cse.iitb.ac.in>
 */

#include <linux/string.h>
#include <linux/slab.h>

#include "xsk_sysfs.h"

static struct kset *flash_kset = NULL;

/*
 * The default show function that must be passed to sysfs.  This will be
 * called by sysfs for whenever a show function is called by the user on a
 * sysfs file associated with the kobjects we have registered.  We need to
 * transpose back from a "default" kobject to our custom struct foo_obj and
 * then call the show function for that specific object.
 */
static ssize_t flash_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct flash_attribute *attribute;
	struct flash_obj *obj;

	attribute = to_flash_attr(attr);
	obj = to_flash_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(obj, attribute, buf);
}

/*
 * Just like the default show function above, but this one is for when the
 * sysfs "store" is requested (when a value is written to a file.)
 */
static ssize_t flash_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	struct flash_attribute *attribute;
	struct flash_obj *obj;

	attribute = to_flash_attr(attr);
	obj = to_flash_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(obj, attribute, buf, len);
}

/* Our custom sysfs_ops that we will associate with our ktype later on */
static const struct sysfs_ops flash_sysfs_ops = {
	.show = flash_attr_show,
	.store = flash_attr_store,
};

/*
 * The release function for our object.  This is REQUIRED by the kernel to
 * have.  We free the memory held in our object here.
 */
static void flash_release(struct kobject *kobj)
{
	struct flash_obj *obj;

	obj = to_flash_obj(kobj);
	kfree(obj);
}

/*
 * This for the "procname" file where the .procname variable is read only file.
 */
static ssize_t ro_buffer_show(struct flash_obj *obj, struct flash_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%s\n", obj->procname);
}

/* Sysfs attributes cannot be world-writable. */
static struct flash_attribute proc_attribute = __ATTR(procname, 0444, ro_buffer_show, NULL);

/*
 * More complex function where we determine which variable is being accessed by
 * looking at the attribute for the "pid", "ifindex" and "queue_id" files.
 * All of them are read only files
 */
static ssize_t ro_int_show(struct flash_obj *obj, struct flash_attribute *attr, char *buf)
{
    int var;

    if (strcmp(attr->attr.name, "pid") == 0)
        var = obj->pid;
    else if (strcmp(attr->attr.name, "ifindex") == 0)
        var = obj->ifindex;
    else
        var = obj->qid;
    return sysfs_emit(buf, "%d\n", var);
}

static struct flash_attribute pid_attribute = __ATTR(pid, 0444, ro_int_show, NULL);
static struct flash_attribute ifindex_attribute = __ATTR(ifindex, 0444, ro_int_show, NULL);
static struct flash_attribute qid_attribute = __ATTR(qid, 0444, ro_int_show, NULL);

/*
 * The "next" file where the .next variable is read from and written to.
 */
static ssize_t rw_int_show(struct flash_obj *obj, struct flash_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", obj->next);
}

static ssize_t rw_int_store(struct flash_obj *obj, struct flash_attribute *attr, const char *buf, size_t count)
{
	int ret;
    int current_id;

	ret = kstrtoint(buf, 10, &obj->next);
	if (ret < 0)
		return ret;

    ret = kstrtoint(obj->kobj.name, 10, &current_id);
    if (ret < 0)
        return ret;

    ret = flash_update_chain_map(current_id, obj->next);
    if (ret < 0) {
        obj->next = -1;
        return ret;
    }

	return count;
}

/* Sysfs attributes cannot be world-writable. */
static struct flash_attribute next_attribute = __ATTR(next, 0644, rw_int_show, rw_int_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *flash_default_attrs[] = {
	&pid_attribute.attr,
    &proc_attribute.attr,
	&ifindex_attribute.attr,
	&qid_attribute.attr,
    &next_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};
ATTRIBUTE_GROUPS(flash_default);

/*
 * Our own ktype for our kobjects.  Here we specify our sysfs ops, the
 * release function, and the set of default attributes we want created
 * whenever a kobject of this type is registered with the kernel.
 */
static const struct kobj_type flash_ktype = {
	.sysfs_ops = &flash_sysfs_ops,
	.release = flash_release,
	.default_groups = flash_default_groups,
};

struct flash_obj *create_flash_obj(int flash_id, int pid, const char *procname, int ifindex, int qid)
{
    struct flash_obj *obj;
    int retval;
    char flash_name[12];

    snprintf(flash_name, sizeof(flash_name), "%d", flash_id);

    /* allocate the memory for the whole object */
    obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;

    /*
     * As we have a kset for this kobject, we need to set it before calling
     * the kobject core.
     */
    obj->kobj.kset = flash_kset;

    /*
     * Initialize the object's fields
     */
    obj->pid = pid;
    obj->ifindex = ifindex;
    obj->qid = qid;
    obj->next = -1;
    strcpy(obj->procname, procname);

    /*
     * Initialize and add the kobject to the kernel.  All the default files
     * will be created here.  As we have already specified a kset for this
     * kobject, we don't have to set a parent for the kobject, the kobject
     * will be placed beneath that kset automatically.
     */

    retval = kobject_init_and_add(&obj->kobj, &flash_ktype, NULL, "%s", flash_name);
    if (retval) {
        kfree(obj);
        return NULL;
    }

    /*
     * We are always responsible for sending the uevent that the kobject
     * was added to the system.
     */
    kobject_uevent(&obj->kobj, KOBJ_ADD);

    return obj;
}

void destroy_flash_obj(struct flash_obj *obj)
{
    kobject_put(&obj->kobj);
}

/*
 * @brief The moudule entry that sets up the sysfs directory
 */
int flash_sysfs_init(void)
{
    /*
     * Create a kset with the name of "flash",
     * located under /sys/kernel/
     */
    flash_kset = kset_create_and_add("flash", NULL, kernel_kobj);
    if (!flash_kset)
        return ENOMEM;

    return 0;
}

/*
 * @brief The exit point
 * In kernel this should not be present. Right?
 */
// static void flash_exit(void)
// {
//     kset_unregister(flash_kset);
// }