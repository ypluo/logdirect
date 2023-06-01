#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/err.h>

struct importer_dmabuf_msg {
	unsigned int reg_addr;
	unsigned int reg_size;
};

#define IMPORTER_IOCTL_ATTACH_CMB _IOR('N', 0x60, int)
#define IMPORTER_IOCTL_DETACH_CMB _IO('N', 0x61)
#define IMPORTER_IOCTL_GET_CMB _IOW('N', 0x62, struct importer_dmabuf_msg)

static struct dma_buf *dmabuf;
static struct dma_buf_attachment *attachment;
static struct sg_table *table;
static struct device *dev;
static int fd = -1;

// it would called by dma_buf_move_notify()
// it should deal with mapping to page changed
static void importer_move_notify(struct dma_buf_attachment *attach)
{
	return;
}

// dma_buf_dynamic_attach would not pin the page, so it should provide move_notify
static struct dma_buf_attach_ops importer_dmabuf_attach_ops = {
	.allow_peer2peer = 1,
	.move_notify = importer_move_notify,
};

static long importer_attach_cmb(void __user *argp)
{
	int ret = -ENOMEM;

	if (copy_from_user(&fd, argp, sizeof(int)))
		return -EFAULT;

	pr_info("[test] importer: dmabuf_fd = %d\n", fd);

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("[test] no dma buffer, error: %ld\n", PTR_ERR(dmabuf));
		ret = PTR_ERR(dmabuf);
		return ret;
	}

	pr_info("[test] importer: dmabuf addr: %px\n", dmabuf);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto failed_dev;
	dev_set_name(dev, "importer");

	// it would pin the page
	// attachment = dma_buf_attach(dmabuf, dev);
	// it would not pin the page
	attachment = dma_buf_dynamic_attach(dmabuf, dev, &importer_dmabuf_attach_ops, NULL);
	if (IS_ERR(attachment)) {
		pr_err("[test] dma_buf_dynamic_attach failed, error: %ld\n", PTR_ERR(attachment));
		ret = PTR_ERR(attachment);
		goto failed_attach;
	}

	pr_info("[test] importer: attachment addr: %px\n", attachment);

	table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		pr_err("[test] dma_buf_map_attachment failed, error: %ld\n", PTR_ERR(table));
		ret = PTR_ERR(table);
		goto failed_attachment;
	}

	return 0;

failed_attachment:
	dma_buf_detach(dmabuf, attachment);
	attachment = NULL;
failed_attach:
	kfree_const(dev->kobj.name);
	kfree(dev);
	dev = NULL;
failed_dev:
	dma_buf_put(dmabuf);
	dmabuf = NULL;
	return ret;
}

static long importer_detach_cmb(void)
{
	if (!table)
		return 0;	

	dma_buf_unmap_attachment(attachment, table, DMA_BIDIRECTIONAL);
	table = NULL;
	dma_buf_detach(dmabuf, attachment);
	attachment = NULL;
	kfree_const(dev->kobj.name);
	kfree(dev);
	dev = NULL;
	dma_buf_put(dmabuf);
	dmabuf = NULL;
	fd = -1;

	return 0;
}

static long importer_get_cmb(struct importer_dmabuf_msg __user * umsg)
{
	struct importer_dmabuf_msg msg;

	if (!table)
		return 0;

	// get bus address of SG entries
	msg.reg_addr = sg_dma_address(table->sgl);
	msg.reg_size = sg_dma_len(table->sgl);
	pr_info("[test] reg_addr = 0x%08x, reg_size = 0x%08x\n", msg.reg_addr, msg.reg_size);

	if (copy_to_user(umsg, &msg, sizeof(msg)))
		return -EFAULT;

	return 0;
}

// ioctl, get the dmabuf fd
static long importer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case IMPORTER_IOCTL_ATTACH_CMB:
		return importer_attach_cmb(argp);
	case IMPORTER_IOCTL_DETACH_CMB:
		return importer_detach_cmb();
	case IMPORTER_IOCTL_GET_CMB:
		return importer_get_cmb(argp);
	default:
		return -ENOTTY;
	}
}
 
static struct file_operations importer_fops = {
	.owner	= THIS_MODULE,
	.unlocked_ioctl	= importer_ioctl,
};

// /dev/importer
static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "importer",
	.fops = &importer_fops,
};
 
static int __init importer_init(void)
{
	return misc_register(&mdev);
}

static void __exit importer_exit(void)
{
	misc_deregister(&mdev);
}

module_init(importer_init);
module_exit(importer_exit);

MODULE_LICENSE("GPL v2");
