// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include <linux/of_device.h>
#include <linux/mm.h>

#include <asm/io.h>

#include <esp_accelerator.h>
#include <esp.h>

#include "conv_stratus.h"

#define DRV_NAME	"conv_stratus"

/* <<--regs-->> */
#define CONV_MEM_OUTPUT_ADDR_REG 0x5c
#define CONV_MEM_INPUT_ADDR_REG 0x58
#define CONV_S_REG 0x54
#define CONV_R_REG 0x50
#define CONV_Q_REG 0x4c
#define CONV_P_REG 0x48
#define CONV_M_REG 0x44
#define CONV_C_REG 0x40

struct conv_stratus_device {
	struct esp_device esp;
};

static struct esp_driver conv_driver;

static struct of_device_id conv_device_ids[] = {
	{
		.name = "SLD_CONV_STRATUS",
	},
	{
		.name = "eb_098",
	},
	{
		.compatible = "sld,conv_stratus",
	},
	{ },
};

static int conv_devs;

static inline struct conv_stratus_device *to_conv(struct esp_device *esp)
{
	return container_of(esp, struct conv_stratus_device, esp);
}

static void conv_prep_xfer(struct esp_device *esp, void *arg)
{
	struct conv_stratus_access *a = arg;

	/* <<--regs-config-->> */
	iowrite32be(a->mem_output_addr, esp->iomem + CONV_MEM_OUTPUT_ADDR_REG);
	iowrite32be(a->mem_input_addr, esp->iomem + CONV_MEM_INPUT_ADDR_REG);
	iowrite32be(a->S, esp->iomem + CONV_S_REG);
	iowrite32be(a->R, esp->iomem + CONV_R_REG);
	iowrite32be(a->Q, esp->iomem + CONV_Q_REG);
	iowrite32be(a->P, esp->iomem + CONV_P_REG);
	iowrite32be(a->M, esp->iomem + CONV_M_REG);
	iowrite32be(a->C, esp->iomem + CONV_C_REG);
	iowrite32be(a->src_offset, esp->iomem + SRC_OFFSET_REG);
	iowrite32be(a->dst_offset, esp->iomem + DST_OFFSET_REG);

}

static bool conv_xfer_input_ok(struct esp_device *esp, void *arg)
{
	/* struct conv_stratus_device *conv = to_conv(esp); */
	/* struct conv_stratus_access *a = arg; */

	return true;
}

static int conv_probe(struct platform_device *pdev)
{
	struct conv_stratus_device *conv;
	struct esp_device *esp;
	int rc;

	conv = kzalloc(sizeof(*conv), GFP_KERNEL);
	if (conv == NULL)
		return -ENOMEM;
	esp = &conv->esp;
	esp->module = THIS_MODULE;
	esp->number = conv_devs;
	esp->driver = &conv_driver;
	rc = esp_device_register(esp, pdev);
	if (rc)
		goto err;

	conv_devs++;
	return 0;
 err:
	kfree(conv);
	return rc;
}

static int __exit conv_remove(struct platform_device *pdev)
{
	struct esp_device *esp = platform_get_drvdata(pdev);
	struct conv_stratus_device *conv = to_conv(esp);

	esp_device_unregister(esp);
	kfree(conv);
	return 0;
}

static struct esp_driver conv_driver = {
	.plat = {
		.probe		= conv_probe,
		.remove		= conv_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = conv_device_ids,
		},
	},
	.xfer_input_ok	= conv_xfer_input_ok,
	.prep_xfer	= conv_prep_xfer,
	.ioctl_cm	= CONV_STRATUS_IOC_ACCESS,
	.arg_size	= sizeof(struct conv_stratus_access),
};

static int __init conv_init(void)
{
	return esp_driver_register(&conv_driver);
}

static void __exit conv_exit(void)
{
	esp_driver_unregister(&conv_driver);
}

module_init(conv_init)
module_exit(conv_exit)

MODULE_DEVICE_TABLE(of, conv_device_ids);

MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("conv_stratus driver");
