/*
 * Qualcomm APQ8016 spmi controller driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Copied from LK/spmi, TODO: cleanup
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <asm/arch/sysmap.h>
#include <malloc.h>
#include <linux/bitops.h>


#define SPMI_GENI_BASE              (MSM_SPMI_BASE + 0xA000)
#define SPMI_PIC_BASE               (MSM_SPMI_BASE +  0x01800000)
#define PMIC_ARB_CORE               0x200F000

#define PMIC_ARB_CORE_REG_BASE               (MSM_SPMI_BASE + 0x00400000)
#define PMIC_ARB_OBS_CORE_REG_BASE           (MSM_SPMI_BASE + 0x00C00000)
#define PMIC_ARB_CHNLn_CONFIG(x)             (PMIC_ARB_CORE_REG_BASE + 0x00000004 + (x) * 0x8000)
#define PMIC_ARB_OBS_CHNLn_CONFIG(x)         (PMIC_ARB_OBS_CORE_REG_BASE + 0x00000004 + (x) * 0x8000)
#define PMIC_ARB_CHNLn_STATUS(x)             (PMIC_ARB_CORE_REG_BASE + 0x00000008 + (x) * 0x8000)
#define PMIC_ARB_OBS_CHNLn_STATUS(x)         (PMIC_ARB_OBS_CORE_REG_BASE + 0x00000008 + (x) * 0x8000)
#define PMIC_ARB_CHNLn_WDATA(x, n)           (PMIC_ARB_CORE_REG_BASE + 0x00000010 + (x) * 0x8000 + (n) * 4)
#define PMIC_ARB_CHNLn_RDATA(x,n)            (PMIC_ARB_CORE_REG_BASE + 0x00000018 + (x) * 0x8000 + (n) * 4)
#define PMIC_ARB_OBS_CHNLn_RDATA(x,n)        (PMIC_ARB_OBS_CORE_REG_BASE + 0x00000018 + (x) * 0x8000 + (n) * 4)

#define PMIC_ARB_REG_CHLN(n)                 (PMIC_ARB_CORE + 0x00000800 + 0x4 * (n))
#define PMIC_ARB_CHNLn_CMD0(x)               (PMIC_ARB_CORE_REG_BASE + (x) * 0x8000)
#define PMIC_ARB_OBS_CHNLn_CMD0(x)           (PMIC_ARB_OBS_CORE_REG_BASE + (x) * 0x8000)

/* PIC Registers */
#define SPMI_PIC_OWNERm_ACC_STATUSn(m, n)    (SPMI_PIC_BASE + 0x00100000 + 0x1000 * (m) + 0x4 * (n))
#define SPMI_PIC_ACC_ENABLEn(n)              (SPMI_PIC_BASE + 0x1000 * n )
#define SPMI_PIC_IRQ_STATUSn(n)              (SPMI_PIC_BASE + 0x00000004 + 0x1000 * (n))
#define SPMI_PIC_IRQ_CLEARn(n)               (SPMI_PIC_BASE + 0x00000008 + 0x1000 * (n))

#define PMIC_ARB_CMD_OPCODE_SHIFT            27
#define PMIC_ARB_CMD_PRIORITY_SHIFT          26
#define PMIC_ARB_CMD_SLAVE_ID_SHIFT          20
#define PMIC_ARB_CMD_ADDR_SHIFT              12
#define PMIC_ARB_CMD_ADDR_OFFSET_SHIFT       4
#define PMIC_ARB_CMD_BYTE_CNT_SHIFT          0

#define SPMI_CMD_EXT_REG_WRTIE_LONG          0x00
#define SPMI_CMD_EXT_REG_READ_LONG           0x01
#define SPMI_CMD_EXT_REG_READ_LONG_DELAYED   0x02
#define SPMI_CMD_TRANSFER_BUS_OWNERSHIP      0x03

#define SPMI_CMD_RESET                       0x04
#define SPMI_CMD_SLEEP                       0x05
#define SPMI_CMD_SHUTDOWN                    0x06
#define SPMI_CMD_WAKEUP                      0x07
#define SPMI_CMD_EXT_REG_WRITE               0x08
#define SPMI_CMD_EXT_REG_READ                0x09
#define SPMI_CMD_REG_WRITE                   0x0A
#define SPMI_CMD_REG_READ                    0x0B
#define SPMI_CMD_REG_0_WRITE                 0x0C
#define SPMI_CMD_AUTH                        0x0D
#define SPMI_CMD_MASTER_WRITE                0x0E
#define SPMI_CMD_MASTER_READ                 0x0F
#define SPMI_CMD_DEV_DESC_BLK_MASTER_READ    0x10
#define SPMI_CMD_DEV_DESC_BLK_SLAVE_READ     0x11

#define REG_OFFSET(_addr)   ((_addr) & 0xFF)
#define PERIPH_ID(_addr)    (((_addr) & 0xFF00) >> 8)
#define SLAVE_ID(_addr)     ((_addr) >> 16)

#define CHNL_IDX(sid, pid) ((sid << 8) | pid)
#define MAX_PERIPH                           128
static uint8_t *chnl_tbl;

static void spmi_lookup_chnl_number(void)
{
	int i;
	uint8_t slave_id;
	uint8_t ppid_address;
	/* We need a max of sid (4 bits) + pid (8bits) of uint8_t's */
	uint32_t chnl_tbl_sz = BIT(12) * sizeof(uint8_t);

	/* Allocate the channel table */
	chnl_tbl = (uint8_t *) malloc(chnl_tbl_sz);

	for(i = 0; i < MAX_PERIPH ; i++)
	{
		slave_id = (readl(PMIC_ARB_REG_CHLN(i)) & 0xf0000) >> 16;
		ppid_address = (readl(PMIC_ARB_REG_CHLN(i)) & 0xff00) >> 8;
		chnl_tbl[CHNL_IDX(slave_id, ppid_address)] = i;
	}
}
struct pmic_arb_cmd{
	uint8_t opcode;
	uint8_t priority;
	uint8_t slave_id;
	uint8_t address;
	uint8_t offset;
	uint8_t byte_cnt;
};

struct pmic_arb_param{
	uint8_t *buffer;
	uint8_t size;
};
enum spmi_geni_cmd_return_value{
	SPMI_CMD_DONE,
	SMPI_CMD_DENIED,
	SPMI_CMD_FAILURE,
	SPMI_ILLEGAL_CMD,
	SPMI_CMD_OVERRUN = 6,
	SPMI_TX_FIFO_RD_ERR,
	SPMI_TX_FIFO_WR_ERR,
	SPMI_RX_FIFO_RD_ERR,
	SPMI_RX_FIFO_WR_ERR
};

enum pmic_arb_chnl_return_values{
	PMIC_ARB_CMD_DONE,
	PMIC_ARB_CMD_FAILURE,
	PMIC_ARB_CMD_DENIED,
	PMIC_ARB_CMD_DROPPED,
};
static uint32_t pmic_arb_chnl_num;

static void write_wdata_from_array(uint8_t *array,
				       uint8_t reg_num,
				       uint8_t array_size,
				       uint8_t* bytes_written)
{
	uint32_t shift_value[] = {0, 8, 16, 24};
	int i;
	uint32_t val = 0;

	/* Write to WDATA */
	for (i = 0; (*bytes_written < array_size) && (i < 4); i++)
	{
		val |= (uint32_t)(array[*bytes_written]) << shift_value[i];
		(*bytes_written)++;
	}

	writel(val, PMIC_ARB_CHNLn_WDATA(pmic_arb_chnl_num, reg_num));
}

/* Initiate a write cmd by writing to cmd register.
 * Commands are written according to cmd parameters
 * cmd->opcode   : SPMI opcode for the command
 * cmd->priority : Priority of the command
 *                 High priority : 1
 *                 Low Priority : 0
 * cmd->address  : SPMI Peripheral Address.
 * cmd->offset   : Offset Address for the command.
 * cmd->bytecnt  : Number of bytes to be written.
 *
 * param is the parameter to the command
 * param->buffer : Value to be written
 * param->size   : Size of the buffer.
 *
 * return value : 0 if success, the error bit set on error
 */
unsigned int pmic_arb_write_cmd(struct pmic_arb_cmd *cmd,
				struct pmic_arb_param *param)
{
	uint8_t bytes_written = 0;
	uint32_t error;
	uint32_t val = 0;

	/* Look up for pmic channel only for V2 hardware
	 * For V1-HW we dont care for channel number & always
	 * use '0'
	 */
	pmic_arb_chnl_num = chnl_tbl[CHNL_IDX(cmd->slave_id, cmd->address)];

	/* Disable IRQ mode for the current channel*/
	writel(0x0, PMIC_ARB_CHNLn_CONFIG(pmic_arb_chnl_num));
	/* Write parameters for the cmd */
	if (cmd == NULL)
	{
		return 1;
	}

	/* Write the data bytes according to the param->size
	 * Can write upto 8 bytes.
	 */

	/* Write first 4 bytes to WDATA0 */
	write_wdata_from_array(param->buffer, 0, param->size, &bytes_written);

	if (bytes_written < param->size)
	{
		/* Write next 4 bytes to WDATA1 */
		write_wdata_from_array(param->buffer, 1, param->size, &bytes_written);
	}

	/* Fill in the byte count for the command
	 * Note: Byte count is one less than the number of bytes transferred.
	 */
	cmd->byte_cnt = param->size - 1;
	/* Fill in the Write cmd opcode. */
	cmd->opcode = SPMI_CMD_EXT_REG_WRTIE_LONG;

	/* Write the command */
	val = 0;
	val |= ((uint32_t)(cmd->opcode) << PMIC_ARB_CMD_OPCODE_SHIFT);
	val |= ((uint32_t)(cmd->priority) << PMIC_ARB_CMD_PRIORITY_SHIFT);
	val |= ((uint32_t)(cmd->slave_id) << PMIC_ARB_CMD_SLAVE_ID_SHIFT);
	val |= ((uint32_t)(cmd->address) << PMIC_ARB_CMD_ADDR_SHIFT);
	val |= ((uint32_t)(cmd->offset) << PMIC_ARB_CMD_ADDR_OFFSET_SHIFT);
	val |= ((uint32_t)(cmd->byte_cnt));

	writel(val, PMIC_ARB_CHNLn_CMD0(pmic_arb_chnl_num));

	/* Wait till CMD DONE status */
	while (!(val = readl(PMIC_ARB_CHNLn_STATUS(pmic_arb_chnl_num))));

	/* Check for errors */
	error = val ^ (1 << PMIC_ARB_CMD_DONE);
	if (error)
	{
		printf("SPMI write command failure: cmd_id = %u, error = %u\n", cmd->opcode, error);
		return error;
	}
	else
		return 0;
}

static void read_rdata_into_array(uint8_t *array,
				  uint8_t reg_num,
				  uint8_t array_size,
				  uint8_t* bytes_read)
{
	uint32_t val = 0;
	uint32_t mask_value[] = {0xFF, 0xFF00, 0xFF0000, 0xFF000000};
	uint8_t shift_value[] = {0, 8, 16, 24};
	int i;

		val = readl(PMIC_ARB_OBS_CHNLn_RDATA(pmic_arb_chnl_num, reg_num));

	/* Read at most 4 bytes */
	for (i = 0; (i < 4) && (*bytes_read < array_size); i++)
	{
		array[*bytes_read] = (val & mask_value[i]) >> shift_value[i];
		(*bytes_read)++;
	}
}

unsigned int pmic_arb_read_cmd(struct pmic_arb_cmd *cmd,
			       struct pmic_arb_param *param)
{
	uint32_t val = 0;
	uint32_t error;
	uint8_t bytes_read = 0;

	/* Look up for pmic channel only for V2 hardware
	 * For V1-HW we dont care for channel number & always
	 * use '0'
	 */
	pmic_arb_chnl_num = chnl_tbl[CHNL_IDX(cmd->slave_id, cmd->address)];

	/* Disable IRQ mode for the current channel*/
	writel(0x0, PMIC_ARB_OBS_CHNLn_CONFIG(pmic_arb_chnl_num));

	/* Fill in the byte count for the command
	 * Note: Byte count is one less than the number of bytes transferred.
	 */
	cmd->byte_cnt = param->size - 1;
	/* Fill in the Write cmd opcode. */
	cmd->opcode = SPMI_CMD_EXT_REG_READ_LONG;

	val |= ((uint32_t)(cmd->opcode) << PMIC_ARB_CMD_OPCODE_SHIFT);
	val |= ((uint32_t)(cmd->priority) << PMIC_ARB_CMD_PRIORITY_SHIFT);
#ifndef SPMI_CORE_V2
	val |= ((uint32_t)(cmd->slave_id) << PMIC_ARB_CMD_SLAVE_ID_SHIFT);
	val |= ((uint32_t)(cmd->address) << PMIC_ARB_CMD_ADDR_SHIFT);
#endif
	val |= ((uint32_t)(cmd->offset) << PMIC_ARB_CMD_ADDR_OFFSET_SHIFT);
	val |= ((uint32_t)(cmd->byte_cnt));

		writel(val, PMIC_ARB_OBS_CHNLn_CMD0(pmic_arb_chnl_num));

	/* Wait till CMD DONE status */
		while (!(val = readl(PMIC_ARB_OBS_CHNLn_STATUS(pmic_arb_chnl_num))));

	/* Check for errors */
	error = val ^ (1 << PMIC_ARB_CMD_DONE);

	if (error)
	{
		printf("SPMI read command failure: cmd_id = %u, error = %u\n", cmd->opcode, error);
		return error;
	}

	/* Read the RDATA0 */
	read_rdata_into_array(param->buffer, 0, param->size , &bytes_read);

	if (bytes_read < param->size)
	{
		/* Read the RDATA1 */
		read_rdata_into_array(param->buffer, 1, param->size , &bytes_read);

	}

	if (bytes_read < param->size)
	{
		/* Read the RDATA2 */
		read_rdata_into_array(param->buffer, 2, param->size , &bytes_read);

	}

	return 0;
}

uint8_t pmic_bus_read(uint32_t addr)
{
	uint8_t val = 0;
	struct pmic_arb_cmd cmd;
	struct pmic_arb_param param;

	cmd.address  = PERIPH_ID(addr);
	cmd.offset   = REG_OFFSET(addr);
	cmd.slave_id = SLAVE_ID(addr);
	cmd.priority = 0;

	param.buffer = &val;
	param.size   = 1;

	pmic_arb_read_cmd(&cmd, &param);

	return val;
}

void pmic_bus_write(uint32_t addr, uint8_t val)
{
	struct pmic_arb_cmd cmd;
	struct pmic_arb_param param;

	cmd.address  = PERIPH_ID(addr);
	cmd.offset   = REG_OFFSET(addr);
	cmd.slave_id = SLAVE_ID(addr);
	cmd.priority = 0;

	param.buffer = &val;
	param.size   = 1;

	pmic_arb_write_cmd(&cmd, &param);
}

int power_pmicc_init(void)
{
	spmi_lookup_chnl_number();
	return 0;
}
