// SPDX-License-Identifier: GPL-2.0
/* Bluetooth HCI driver model support. */

#include <linux/module.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

static struct class *bt_class;

static void bt_link_release(struct device *dev)
{
	struct hci_conn *conn = to_hci_conn(dev);
	kfree(conn);
}

static const struct device_type bt_link = {
	.name    = "link",
	.release = bt_link_release,
};

void hci_conn_init_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	conn->dev.type = &bt_link;
	conn->dev.class = bt_class;
	conn->dev.parent = &hdev->dev;

	device_initialize(&conn->dev);
}

void hci_conn_add_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	if (device_is_registered(&conn->dev))
		return;

	dev_set_name(&conn->dev, "%s:%d", hdev->name, conn->handle);

	if (device_add(&conn->dev) < 0)
		bt_dev_err(hdev, "failed to register connection device");
}

void hci_conn_del_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	if (!device_is_registered(&conn->dev)) {
		/* If device_add() has *not* succeeded, use *only* put_device()
		 * to drop the reference count.
		 */
		put_device(&conn->dev);
		return;
	}

	/* If there are devices using the connection as parent reset it to NULL
	 * before unregistering the device.
	 */
	while (1) {
		struct device *dev;

		dev = device_find_any_child(&conn->dev);
		if (!dev)
			break;
		device_move(dev, NULL, DPM_ORDER_DEV_LAST);
		put_device(dev);
	}

	device_unregister(&conn->dev);
}

static void bt_host_release(struct device *dev)
{
	struct hci_dev *hdev = to_hci_dev(dev);

	if (hci_dev_test_flag(hdev, HCI_UNREGISTER))
		hci_cleanup_dev(hdev);
	kfree(hdev);
	module_put(THIS_MODULE);
}

static const struct device_type bt_host = {
	.name    = "host",
	.release = bt_host_release,
};

void hci_init_sysfs(struct hci_dev *hdev)
{
	struct device *dev = &hdev->dev;

	dev->type = &bt_host;
	dev->class = bt_class;

	__module_get(THIS_MODULE);
	device_initialize(dev);
}

int __init bt_sysfs_init(void)
{
	bt_class = class_create(THIS_MODULE, "bluetooth");

	return PTR_ERR_OR_ZERO(bt_class);
}

void bt_sysfs_cleanup(void)
{
	class_destroy(bt_class);
}
