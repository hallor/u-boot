/*
 * (C) Copyright 2013
 *
 * Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Based on Linux dm9601.c
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/unaligned.h>
#include <common.h>
#include <usb.h>
#include <linux/mii.h>
#include "usb_ether.h"
#include <malloc.h>
#include <memalign.h>

/* control requests */
#define DM_READ_REGS	0x00
#define DM_WRITE_REGS	0x01
#define DM_READ_MEMS	0x02
#define DM_WRITE_REG	0x03
#define DM_WRITE_MEMS	0x05
#define DM_WRITE_MEM	0x07

/* registers */
#define DM_NET_CTRL	0x00
#define DM_NET_STATUS 0x1
#define DM_TX_CTRL 0x2
#define DM_TX_STATUS1 0x3
#define DM_TX_STATUS2 0x4
#define DM_RX_CTRL	0x05
#define DM_RX_STATUS 0x6
#define DM_PHY_ADDR	0x10	/* 6 bytes */
#define DM_GPR_CTRL	0x1e
#define DM_GPR_DATA	0x1f

#define USB_CTRL_SET_TIMEOUT 5000
#define USB_CTRL_GET_TIMEOUT 5000
#define USB_BULK_SEND_TIMEOUT 5000
#define USB_BULK_RECV_TIMEOUT 5000

#define DM_BASE_NAME "dm9601"

/* local vars */
static int curr_eth_dev; /* index for name of next device detected */

static int dm_write_cmd(struct ueth_data *dev, u8 cmd, u16 value, u16 index,
				 u16 size, void *data)
{
	int len;

	len = usb_control_msg(
		dev->pusb_dev,
		usb_sndctrlpipe(dev->pusb_dev, 0),
		cmd,
		USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		value,
		index,
		data,
		size,
		USB_CTRL_SET_TIMEOUT);

	return len == size ? 0 : -1;
}

static int dm_read_cmd(struct ueth_data *dev, u8 cmd, u16 value, u16 index,
				u16 size, void *data)
{
	int len;

	len = usb_control_msg(
		dev->pusb_dev,
		usb_rcvctrlpipe(dev->pusb_dev, 0),
		cmd,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		value,
		index,
		data,
		size,
		USB_CTRL_GET_TIMEOUT);
	return len == size ? 0 : -1;
}

static u8 dm_read_reg(struct ueth_data *dev, u8 reg, int * err) {
	if (err)
		*err = dm_read_cmd(dev, DM_READ_REGS, 0, reg, 1, &reg);
	else
		dm_read_cmd(dev, DM_READ_REGS, 0, reg, 1, &reg);
	return reg;
}

static int dm_write_reg(struct ueth_data *dev, u8 reg, u8 val) {
	return dm_write_cmd(dev, DM_WRITE_REG, val, reg, 0, NULL);
}

static u16 dm_read_phy(struct ueth_data *dev, u8 reg, int *err) {
	u16 data;
	int ret = 0;
	dm_write_reg(dev, 0xC, reg | 0x40); // Address @ phy
	dm_write_reg(dev, 0xB, 0x4 | 0x8); // read phy
	while (dm_read_reg(dev, 0xb, &ret) & 0x1) {
		if (ret) {
			printf("Read error.\n");
			return 0;
		}
		udelay(10);
	}

	ret = dm_write_reg(dev, 0xB, 0); // confirm
	if (ret)
		goto err;

	ret = dm_read_cmd(dev, DM_READ_REGS, 0, 0xD, 2, &data);
	if (ret)
		goto err;
	data = le16_to_cpu(data);
	return data;
err:
	if (*err)
		*err = ret;
	printf("%s(): Error\n", __FUNCTION__);
	return 0;
}

static int dm_write_phy(struct ueth_data *dev, u8 reg, u16 val) {
	int ret = 0;
	val = cpu_to_le16(val);
	ret = dm_write_cmd(dev, DM_WRITE_REGS, 0, 0xD, 2, &val); // data
	if (ret)
		goto err;
	ret = dm_write_reg(dev, 0xC, reg | 0x40); // Address @ phy
	if (ret)
		goto err;
	ret = dm_write_reg(dev, 0xB, 0x2 | 0x8); // write phy
	if (ret)
		goto err;
	while (dm_read_reg(dev, 0xb, &ret) & 0x1) {
		if (ret)
			goto err;
		udelay(10);
	}
	ret = dm_write_reg(dev, 0xB, 0); // confirm
	if (ret)
		goto err;

	return 0;
err:
	printf("%s(): Error\n", __FUNCTION__);
	return ret;
}

/* Misc */

static int dm_read_mac(struct eth_device *eth)
{
	struct ueth_data *dev = (struct ueth_data *)eth->priv;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, buf, ETH_ALEN);

	if (dm_read_cmd(dev, DM_READ_REGS, 0, DM_PHY_ADDR, ETH_ALEN, buf) < 0) {
		printf("Error reading MAC\n");
		return -1;
	}
	memcpy(eth->enetaddr, buf, ETH_ALEN);
	return 0;
}

static int dm_reset(struct ueth_data * dev) {
	int ret;
	// Reset
	debug("** %s()\n", __func__);
	if (dm_write_reg(dev, DM_NET_CTRL, 0x1) < 0)
		return -1;
	udelay(15);
	debug("Waiting for reset..\n");
	while (dm_read_reg(dev, DM_NET_CTRL, &ret) & 0x1) {
		if (ret) {
			printf("Error waiting for reset.\n");
			return ret;
		}
		udelay(10);
	}
	// Reset phy
	if (dm_write_phy(dev, MII_BMCR, BMCR_RESET))
		return -1;

	udelay(100);
	if (dm_write_phy(dev, MII_ADVERTISE, ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP))
		return -1;
	if (dm_write_phy(dev, MII_BMCR, 0))
		return -1;
	udelay(1000);

	debug("reset completed.\n");
	return 0;
}

/* Commands */
#define PHY_CONNECT_TIMEOUT 5000
static int dm9601_init(struct eth_device *eth, bd_t *bd)
{
	struct ueth_data	*dev = (struct ueth_data *)eth->priv;
	int timeout = 0;
#define TIMEOUT_RESOLUTION 50	/* ms */
	int link_detected = 0;
	int ret;

	debug("** %s()\n", __func__);

	// Configure
	if (dm_write_reg(dev, DM_RX_CTRL, 0x3d) < 0) // RX enable + small packets + drop errous + multicast
		goto out_err;

	// Enable phy
	if (dm_write_reg(dev, DM_GPR_CTRL, 1))
		goto out_err;
	if (dm_write_reg(dev, DM_GPR_DATA, 0))
		goto out_err;

	do {
		link_detected = dm_read_phy(dev, MII_BMSR, &ret) & BMSR_LSTATUS; // LINKST
		if (ret) {
			printf("Error waiting for link.\n");
			goto out_err;
		}
		if (!link_detected) {
			if (timeout == 0)
				printf("Waiting for Ethernet connection... ");
			udelay(TIMEOUT_RESOLUTION * 1000);
			timeout += TIMEOUT_RESOLUTION;
		}
	} while (!link_detected && timeout < PHY_CONNECT_TIMEOUT);

	if (link_detected) {
		if (timeout < PHY_CONNECT_TIMEOUT)
			printf("Link detected.\n");
		else {
			printf("timeout.\n");
			goto out_err;
		}
	} else {
		printf("unable to connect.\n");
		goto out_err;
	}

	return 0;
out_err:
	printf("error.\n");
	return -1;
}

static int dm9601_send(struct eth_device *eth, void *packet, int length)
{
	struct ueth_data *dev = (struct ueth_data *)eth->priv;
	int err;
	u16 packet_len;
	int actual_len;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, msg, PKTSIZE + sizeof(packet_len));

	msg[0] = length;
	msg[1] = length >> 8;
	memcpy(msg + 2, (void *)packet, length);

	if (dm_write_reg(dev, DM_NET_STATUS, 0x8 | 0x4)) {
		printf("error writing status register.\n");
		return -1;
	}

	err = usb_bulk_msg(dev->pusb_dev,
				usb_sndbulkpipe(dev->pusb_dev, dev->ep_out),
				(void *)msg,
				length + 2,
				&actual_len,
				USB_BULK_SEND_TIMEOUT);
	if (actual_len != length + 2) {
		printf("Failed to send data.\n");
		return -1;
	}
	udelay(1000);
	return err;
}


static int dm9601_recv(struct eth_device *eth)
{
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, recv_buf, 2048);
	struct ueth_data *dev = (struct ueth_data *)eth->priv;
	int err = 0;
	int actual_len = 0;
	u8 packet_status;
	u16 packet_len;

	while (dm_read_reg(dev, DM_NET_STATUS, &err) & 0x1)
	{
		if (err) {
			printf("error reading status register.\n");
			return -1;
		}
		udelay(100);
		err = usb_bulk_msg(dev->pusb_dev,
						   usb_rcvbulkpipe(dev->pusb_dev, dev->ep_in),
						   (void *)recv_buf,
						   2048,
						   &actual_len,
						   USB_BULK_RECV_TIMEOUT);
		if (err != 0) {
			debug("Rx: failed to receive\n");
			//		dm_reset(dev);
			return -1;
		}
		if (actual_len > 2048) {
			debug("Rx: received too many bytes %d\n", actual_len);
			return -1;
		}

		if (actual_len > 0) {
			if (actual_len < 7) {
				debug("Rx: incomplete packet length\n");
				return -1;
			}

			packet_status = recv_buf[0];
			packet_len = (recv_buf[1] | (recv_buf[2] << 8)) - 4;
			actual_len -=7;

			if (actual_len < packet_len) {
				printf("Error, received too little bytes (%d vs %d)\n", actual_len, packet_len);
				return 0;
			}

			if (packet_status & 0xBF) {
				if (packet_status & 0x01) printf("fifo error\n");
				if (packet_status & 0x02) printf("rx crc error\n");
				if (packet_status & 0x04) printf("rx frame error\n");
				if (packet_status & 0x20) printf("rx missed error\n");
				if (packet_status & 0x90) printf("rx length error\n");
				printf("Packet damaged.\n");
				return 0;
			}
			memmove(recv_buf, recv_buf + 3, actual_len);

			/* Notify net stack */
			net_process_received_packet(recv_buf, actual_len);
		}

	}
	return err;
}

static void dm9601_halt(struct eth_device *eth)
{
	dm_write_reg(eth->priv, DM_RX_CTRL, 0x30); // RX enable
	debug("** %s()\n", __func__);
}


/* Public interface */
void dm9601_eth_before_probe(void)
{
	curr_eth_dev = 0;
}

int dm9601_eth_probe(struct usb_device *dev, unsigned int ifnum, struct ueth_data *ss)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *iface_desc;
	int i;

	/* let's examine the device now */
	iface = &dev->config.if_desc[ifnum];
	iface_desc = &dev->config.if_desc[ifnum].desc;

	/* Just one device for now, no need for table */
	if (dev->descriptor.idVendor != 0x0a46 ||
		dev->descriptor.idProduct != 0x9601)
		return 0;

	/* At this point, we know we've got a live one */
	debug("\n\nUSB Ethernet device detected\n");
	memset(ss, '\0', sizeof(struct ueth_data));

	/* Initialize the ueth_data structure with some useful info */
	ss->ifnum = ifnum;
	ss->pusb_dev = dev;
	ss->subclass = iface_desc->bInterfaceSubClass;
	ss->protocol = iface_desc->bInterfaceProtocol;

	/*
	 * We are expecting a minimum of 3 endpoints - in, out (bulk), and int.
	 * We will ignore any others.
	 */
	for (i = 0; i < iface_desc->bNumEndpoints; i++) {
		/* is it an BULK endpoint? */
		if ((iface->ep_desc[i].bmAttributes &
			 USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
			if (iface->ep_desc[i].bEndpointAddress & USB_DIR_IN)
				ss->ep_in =
					iface->ep_desc[i].bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
			else
				ss->ep_out =
					iface->ep_desc[i].bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
		}

		/* is it an interrupt endpoint? */
		if ((iface->ep_desc[i].bmAttributes &
			USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
			ss->ep_int = iface->ep_desc[i].bEndpointAddress &
				USB_ENDPOINT_NUMBER_MASK;
			ss->irqinterval = iface->ep_desc[i].bInterval;
		}
	}
	debug("Endpoints In %d Out %d Int %d\n",
		  ss->ep_in, ss->ep_out, ss->ep_int);

	/* Do some basic sanity checks, and bail if we find a problem */
	if (usb_set_interface(dev, iface_desc->bInterfaceNumber, 0) ||
		!ss->ep_in || !ss->ep_out || !ss->ep_int) {
		debug("Problems with device\n");
		return 0;
	}
	dev->privptr = (void *)ss;
	return 1;
}

int dm9601_eth_get_info(struct usb_device *dev, struct ueth_data *ss, struct eth_device *eth)
{
	if (!eth) {
		debug("%s: missing parameter.\n", __func__);
		return 0;
	}
	sprintf(eth->name, "%s%d", DM_BASE_NAME, curr_eth_dev++);
	eth->init = dm9601_init;
	eth->send = dm9601_send;
	eth->recv = dm9601_recv;
	eth->halt = dm9601_halt;
	eth->priv = ss;

	if (dm_reset(ss))
		return 0;

	/* Get the MAC address */
	if (dm_read_mac(eth))
		return 0;
	debug("MAC %pM\n", eth->enetaddr);

	return 1;
}
