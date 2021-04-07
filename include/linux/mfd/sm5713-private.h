/*
 *  sm5713-private.h - IF-PMIC device driver for SM5713
 *
 *  Copyright (C) 2017 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5713_PRIV_H__
#define __SM5713_PRIV_H__

#include <linux/i2c.h>
#include <linux/interrupt.h>

#define SM5713_I2C_SADR_MUIC	(0x4A >> 1)
#define SM5713_I2C_SADR_CHG     (0x92 >> 1)
#define SM5713_I2C_SADR_FG      (0xE2 >> 1)

#define SM5713_IRQSRC_MUIC	    (1 << 0)
#define SM5713_IRQSRC_CHG	    (1 << 1)
#define SM5713_IRQSRC_FG        (1 << 2)
#define SM5713_REG_INVALID		(0xffff)


/* Slave addr = 0x4A : MUIC */
enum sm5713_muic_reg {
    SM5713_MUIC_REG_DeviceID        = 0x00,
    SM5713_MUIC_REG_INT1            = 0x01,
    SM5713_MUIC_REG_INT2            = 0x02,
    SM5713_MUIC_REG_INTMASK1        = 0x03,
    SM5713_MUIC_REG_INTMASK2        = 0x04,
    SM5713_MUIC_REG_CNTL            = 0x05,
    SM5713_MUIC_REG_MANUAL_SW       = 0x06,
    SM5713_MUIC_REG_DEVICETYPE1     = 0x07,
    SM5713_MUIC_REG_DEVICETYPE2     = 0x08,
    SM5713_MUIC_REG_AFCCNTL         = 0x09,
    SM5713_MUIC_REG_AFCTXD          = 0x0A,
    SM5713_MUIC_REG_AFCSTATUS       = 0x0B,
    SM5713_MUIC_REG_VBUS_VOLTAGE1   = 0x0C,
    SM5713_MUIC_REG_VBUS_VOLTAGE2   = 0x0D,
    SM5713_MUIC_REG_AFC_RXD1        = 0x0E,
    SM5713_MUIC_REG_AFC_RXD2        = 0x0F,
    SM5713_MUIC_REG_AFC_RXD3        = 0x10,
    SM5713_MUIC_REG_AFC_RXD4        = 0x11,
    SM5713_MUIC_REG_AFC_RXD5        = 0x12,
    SM5713_MUIC_REG_AFC_RXD6        = 0x13,
    SM5713_MUIC_REG_AFC_RXD7        = 0x14,
    SM5713_MUIC_REG_AFC_RXD8        = 0x15,
    SM5713_MUIC_REG_AFC_RXD9        = 0x16,
    SM5713_MUIC_REG_AFC_RXD10       = 0x17,
    SM5713_MUIC_REG_AFC_RXD11       = 0x18,
    SM5713_MUIC_REG_AFC_RXD12       = 0x19,
    SM5713_MUIC_REG_AFC_RXD13       = 0x1A,
    SM5713_MUIC_REG_AFC_RXD14       = 0x1B,
    SM5713_MUIC_REG_AFC_RXD15       = 0x1C,
    SM5713_MUIC_REG_RESET           = 0x1D,

    SM5713_MUIC_REG_END,
};

/* Slave addr = 0x92 : SW Charger, RGB, FLED */
enum sm5713_charger_reg {
    /* SW Charger */
    SM5713_CHG_REG_INT_SOURCE       = 0x00,
    SM5713_CHG_REG_INT1             = 0x01,
    SM5713_CHG_REG_INT2             = 0x02,
    SM5713_CHG_REG_INT3             = 0x03,
    SM5713_CHG_REG_INT4             = 0x04,
    SM5713_CHG_REG_INT5             = 0x05,
    SM5713_CHG_REG_INT6             = 0x06,
    SM5713_CHG_REG_INTMSK1          = 0x07,
    SM5713_CHG_REG_INTMSK2          = 0x08,
    SM5713_CHG_REG_INTMSK3          = 0x09,
    SM5713_CHG_REG_INTMSK4          = 0x0A,
    SM5713_CHG_REG_INTMSK5          = 0x0B,
    SM5713_CHG_REG_INTMSK6          = 0x0C,
    SM5713_CHG_REG_STATUS1          = 0x0D,
    SM5713_CHG_REG_STATUS2          = 0x0E,
    SM5713_CHG_REG_STATUS3          = 0x0F,
    SM5713_CHG_REG_STATUS4          = 0x10,
    SM5713_CHG_REG_STATUS5          = 0x11,
    SM5713_CHG_REG_STATUS6          = 0x12,
    SM5713_CHG_REG_CNTL1            = 0x13,
    SM5713_CHG_REG_CNTL2            = 0x14,
    SM5713_CHG_REG_VBUSCNTL         = 0x15,
    SM5713_CHG_REG_CHGCNTL1         = 0x17,
    SM5713_CHG_REG_CHGCNTL2         = 0x18,
    SM5713_CHG_REG_CHGCNTL4         = 0x1A,
    SM5713_CHG_REG_CHGCNTL5         = 0x1B,
    SM5713_CHG_REG_CHGCNTL6         = 0x1C,
    SM5713_CHG_REG_CHGCNTL7         = 0x1D,
    SM5713_CHG_REG_CHGCNTL8         = 0x1E,
    SM5713_CHG_REG_CHGCNTL9         = 0x1F,
    SM5713_CHG_REG_CHGCNTL10        = 0x20,
    SM5713_CHG_REG_WDTCNTL          = 0x22,
    SM5713_CHG_REG_BSTCNTL1         = 0x23,
    SM5713_CHG_REG_FACTORY1         = 0x25,
    SM5713_CHG_REG_FACTORY2         = 0x26,
    /* RGB */
    SM5713_CHG_REG_LED123MODE       = 0x27,
    SM5713_CHG_REG_LEDCNTL          = 0x28,
    SM5713_CHG_REG_LED1CNTL1        = 0x29,
    SM5713_CHG_REG_LED1CNTL2        = 0x2A,
    SM5713_CHG_REG_LED1CNTL3        = 0x2B,
    SM5713_CHG_REG_LED2CNTL1        = 0x2C,
    SM5713_CHG_REG_LED2CNTL2        = 0x2D,
    SM5713_CHG_REG_LED2CNTL3        = 0x2E,
    SM5713_CHG_REG_LED3CNTL1        = 0x2F,
    SM5713_CHG_REG_LED3CNTL2        = 0x30,
    SM5713_CHG_REG_LED3CNTL3        = 0x31,
    /* FLED */
    SM5713_CHG_REG_SINKADJ          = 0x40,
    SM5713_CHG_REG_FLED1CNTL1       = 0x41,
    SM5713_CHG_REG_FLED1CNTL2       = 0x42,
    SM5713_CHG_REG_FLED2CNTL1       = 0x43,
    SM5713_CHG_REG_FLED2CNTL2       = 0x44,
    SM5713_CHG_REG_FLED3CNTL        = 0x45,
    SM5713_CHG_REG_CHGCNTL11        = 0x46,

    SM5713_CHG_REG_DEVICEID         = 0x50,

    SM5713_CHG_REG_END,
};

/* Slave addr = 0xE2 : FUEL GAUGE */
enum sm5713_fuelgauge_reg {
    SM5713_FG_REG_DEVICE_ID         = 0x00,
    SM5713_FG_REG_CNTL              = 0x01,
    SM5713_FG_REG_INTFG             = 0x02,
    SM5713_FG_REG_INTFG_MASK        = 0x03,
    SM5713_FG_REG_STATUS            = 0x04,
    SM5713_FG_REG_SOC               = 0x05,
    SM5713_FG_REG_OCV               = 0x06,
    SM5713_FG_REG_VOLTAGE_VBAT      = 0x07,
    SM5713_FG_REG_CURRENT           = 0x08,
    SM5713_FG_REG_TEMPERATURE       = 0x09,
    SM5713_FG_REG_SOC_CYCLE         = 0x0A,
    SM5713_FG_REG_VOLTAGE_VSYS      = 0x0B,
    SM5713_FG_REG_V_L_ALARM         = 0x0C,
    SM5713_FG_REG_V_H_ALARM         = 0x0D,
    SM5713_FG_REG_T_ALARM           = 0x0E,
    SM5713_FG_REG_SOC_ALARM         = 0x0F,

    SM5713_FG_REG_OP_STATUS         = 0x10,
    SM5713_FG_REG_TOPOFF_SOC        = 0x12,
    SM5713_FG_REG_PARAM_CTRL        = 0x13,
    SM5713_FG_REG_PARAM_RUN_UPDATE  = 0x14,
    SM5713_FG_REG_SOC_CYCLE_CFG     = 0x15,
    SM5713_FG_REG_VOLTAGE_CHGOUT    = 0x19,
    SM5713_FG_REG_VIT_PERIOD        = 0x1A,
    SM5713_FG_REG_MIX_RATE          = 0x1B,
    SM5713_FG_REG_MIX_INIT_BLANK    = 0x1C,

    SM5713_FG_REG_USER_RESERV_1     = 0x1E,
    SM5713_FG_REG_USER_RESERV_2     = 0x1F,

    SM5713_FG_REG_RCE0              = 0x20,
    SM5713_FG_REG_RCE1              = 0x21,
    SM5713_FG_REG_RCE2              = 0x22,
    SM5713_FG_REG_DTCD              = 0x23,
    SM5713_FG_REG_AUTO_RS_MAN       = 0x24,
    SM5713_FG_REG_RS_MIX_FACTOR     = 0x25,
    SM5713_FG_REG_RS_MAX            = 0x26,
    SM5713_FG_REG_RS_MIN            = 0x27,
    SM5713_FG_REG_RS_TUNE           = 0x28,
    SM5713_FG_REG_RS_MAN            = 0x29,
    SM5713_FG_REG_IOFF_MODE         = 0x2B,
    SM5713_FG_REG_IOCV_MAN          = 0x2E,
    SM5713_FG_REG_END_V_IDX         = 0x2F,

    SM5713_FG_REG_START_LB_V        = 0x30,
    SM5713_FG_REG_START_CB_V        = 0x38,
    SM5713_FG_REG_START_LB_S        = 0x40,
    SM5713_FG_REG_START_CB_S        = 0x48,
    SM5713_FG_REG_START_LB_I        = 0x50,
    SM5713_FG_REG_START_CB_I        = 0x58,

    SM5713_FG_REG_SOC_CHG_INFO      = 0x60,
    SM5713_FG_REG_SOC_DISCHG_INFO   = 0x61,
    SM5713_FG_REG_BAT_CAP           = 0x62,
    SM5713_FG_REG_Q_MEAS_INIT       = 0x63,
    SM5713_FG_REG_VOLT_CAL          = 0x6F,

    SM5713_FG_REG_VOLT_TEMP_CAL     = 0x70,
    SM5713_FG_REG_VSBC_VOLT_TEMP_CAL= 0x71,
    SM5713_FG_REG_DP_EV_I_OFF       = 0x74,
    SM5713_FG_REG_DP_EV_I_SLO       = 0x75,
    SM5713_FG_REG_DP_CSP_I_OFF      = 0x76,
    SM5713_FG_REG_DP_CSP_I_SLO      = 0x77,
    SM5713_FG_REG_DP_CSN_I_OFF      = 0x78,
    SM5713_FG_REG_DP_CSN_I_SLO      = 0x79,
    SM5713_FG_REG_EV_I_OFF          = 0x7A,
    SM5713_FG_REG_EV_I_SLO          = 0x7B,
    SM5713_FG_REG_CSP_I_OFF         = 0x7C,
    SM5713_FG_REG_CSP_I_SLO         = 0x7D,
    SM5713_FG_REG_CSN_I_OFF         = 0x7E,
    SM5713_FG_REG_CSN_I_SLO         = 0x7F,

	/* for debug */
    SM5713_FG_REG_OCV_STATE         = 0x80,
    SM5713_FG_REG_VDS               = 0x84,

    SM5713_FG_REG_CURRENT_EST       = 0x85,
    SM5713_FG_REG_CURRENT_ERR       = 0x86,
    SM5713_FG_REG_CURR_ALG          = 0x87,
    SM5713_FG_REG_Q_EST             = 0x88,
    SM5713_FG_REG_Q_MEAS            = 0x89,
    SM5713_FG_REG_Q_DUMP            = 0x8A,
    SM5713_FG_REG_AGING_INFO        = 0x8B,
    SM5713_FG_REG_CURR_MQ           = 0x8C,

	/* etc */
    SM5713_FG_REG_MISC              = 0x90,
    SM5713_FG_REG_RESET             = 0x91,
    SM5713_FG_REG_AUX_1             = 0x92,
    SM5713_FG_REG_AUX_2             = 0x93,
    SM5713_FG_REG_AUX_3             = 0x94,
    SM5713_FG_REG_AUX_STAT          = 0x95,
    SM5713_FG_REG_BAT_PTT1          = 0x96,
    SM5713_FG_REG_V_ALARM_HYS       = 0x99,
    SM5713_FG_REG_AGING_CTRL        = 0x9C,

    SM5713_FG_REG_TABLE_0_START     = 0xA0,
    SM5713_FG_REG_TABLE_1_START     = 0xB8,
    SM5713_FG_REG_TABLE_2_START     = 0xD0,

    SM5713_FG_REG_END,
};

enum sm5713_irq_source {
    MUIC_INT1 = 0,
    MUIC_INT2,
    CHG_INT1,
    CHG_INT2,
    CHG_INT3,
    CHG_INT4,
    CHG_INT5,
    CHG_INT6,
    FG_INT,

    SM5713_IRQ_GROUP_NR,
};
#define SM5713_NUM_IRQ_MUIC_REGS    2
#define SM5713_NUM_IRQ_CHG_REGS     6


enum sm5713_irq {
    SM5713_MUIC_IRQ_WORK = (-2),            /* -2 */
    SM5713_MUIC_IRQ_PROBE = (-1),           /* -1 */
    /* MUIC INT1 */
    SM5713_MUIC_IRQ_INT1_DPDM_OVP = 0,      /* 00 */
    SM5713_MUIC_IRQ_INT1_VBUS_RID_DETACH,   /* 01 */
    SM5713_MUIC_IRQ_INT1_AUTOVBUSCHECK,     /* 02 */
    SM5713_MUIC_IRQ_INT1_RID_DETECT,        /* 03 */
    SM5713_MUIC_IRQ_INT1_CHGTYPE,           /* 04 */
    SM5713_MUIC_IRQ_INT1_DCDTIMEOUT,        /* 05 */
    /* MUIC INT2 */
    SM5713_MUIC_IRQ_INT2_AFC_ERROR,         /* 06 */
    SM5713_MUIC_IRQ_INT2_AFC_STA_CHG,       /* 07 */
    SM5713_MUIC_IRQ_INT2_MULTI_BYTE,        /* 08 */
    SM5713_MUIC_IRQ_INT2_VBUS_UPDATE,       /* 09 */
    SM5713_MUIC_IRQ_INT2_AFC_ACCEPTED,      /* 10 */
    SM5713_MUIC_IRQ_INT2_AFC_TA_ATTACHED,   /* 11 */
    /* CHG INT1 */
    SM5713_CHG_IRQ_INT1_VBUSLIMIT,          /* 12 */
    SM5713_CHG_IRQ_INT1_VBUSOVP,            /* 13 */
    SM5713_CHG_IRQ_INT1_VBUSUVLO,           /* 14 */
    SM5713_CHG_IRQ_INT1_VBUSPOK,            /* 15 */
    /* CHG INT2 */
    SM5713_CHG_IRQ_INT2_WDTMROFF,           /* 16 */
    SM5713_CHG_IRQ_INT2_DONE,               /* 17 */
    SM5713_CHG_IRQ_INT2_TOPOFF,             /* 18 */
    SM5713_CHG_IRQ_INT2_Q4FULLON,           /* 19 */
    SM5713_CHG_IRQ_INT2_CHGON,              /* 20 */
    SM5713_CHG_IRQ_INT2_NOBAT,              /* 21 */
    SM5713_CHG_IRQ_INT2_BATOVP,             /* 22 */
    SM5713_CHG_IRQ_INT2_AICL,               /* 23 */
    /* CHG INT3 */
    SM5713_CHG_IRQ_INT3_VSYSOVP,            /* 24 */
    SM5713_CHG_IRQ_INT3_nENQ4,              /* 25 */
    SM5713_CHG_IRQ_INT3_FASTTMROFF,         /* 26 */
    SM5713_CHG_IRQ_INT3_PRETMROFF,          /* 27 */
    SM5713_CHG_IRQ_INT3_DISLIMIT,           /* 28 */
    SM5713_CHG_IRQ_INT3_OTGFAIL,            /* 29 */
    SM5713_CHG_IRQ_INT3_THEMSHDN,           /* 30 */
    SM5713_CHG_IRQ_INT3_THEMREG,            /* 31 */
    /* CHG INT4 */
    SM5713_CHG_IRQ_INT4_CVMODE,             /* 32 */
    SM5713_CHG_IRQ_INT4_VBUS_UPDATE,        /* 33 */
    SM5713_CHG_IRQ_INT4_MRSTB,              /* 34 */
    SM5713_CHG_IRQ_INT4_nVBUSOK,            /* 35 */
    SM5713_CHG_IRQ_INT4_BOOSTPOK,           /* 36 */
    SM5713_CHG_IRQ_INT4_BOOSTPOK_NG,        /* 37 */
    /* CHG INT5 */
    SM5713_CHG_IRQ_INT5_ABSTMR2OFF,         /* 38 */
    SM5713_CHG_IRQ_INT5_ABSTMR1OFF,         /* 39 */
    SM5713_CHG_IRQ_INT5_FLED3OPEN,          /* 40 */
    SM5713_CHG_IRQ_INT5_FLED3SHORT,         /* 41 */
    SM5713_CHG_IRQ_INT5_FLED2OPEN,          /* 42 */
    SM5713_CHG_IRQ_INT5_FLED2SHORT,         /* 43 */
    SM5713_CHG_IRQ_INT5_FLED1OPEN,          /* 44 */
    SM5713_CHG_IRQ_INT5_FLED1SHORT,         /* 45 */
    /* CHG INT6 */
    SM5713_CHG_IRQ_INT6_VBUSSHORT,          /* 46 */
    /* FG INT */
    SM5713_FG_IRQ_INT_LOW_SOC,              /* 47 */
    SM5713_FG_IRQ_INT_HIGH_TEMP,            /* 48 */
    SM5713_FG_IRQ_INT_LOW_TEMP,             /* 49 */
    SM5713_FG_IRQ_INT_HIGH_VOLTAGE,         /* 50 */
    SM5713_FG_IRQ_INT_LOW_VOLTAGE,          /* 51 */

    SM5713_IRQ_NR,
};

struct sm5713_dev {
	struct device *dev;
	struct i2c_client *charger;     /* 0x92; Charger */
	struct i2c_client *fuelgauge;   /* 0xE2; Fuelgauge */
	struct i2c_client *muic;        /* 0x4A; MUIC */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[SM5713_IRQ_GROUP_NR];
	int irq_masks_cache[SM5713_IRQ_GROUP_NR];

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_muic_dump[SM5713_MUIC_REG_END];
    u8 reg_chg_dump[SM5713_CHG_REG_END];
	u16 reg_fg_dump[SM5713_FG_REG_END];
#endif

    /* For IC-Reset protection */
    void (*check_muic_reset)(struct i2c_client *, void *);
    void (*check_chg_reset)(struct i2c_client *, void *);
    void (*check_fg_reset)(struct i2c_client *, void *);
    void *muic_data;
    void *chg_data;
    void *fg_data;

	u8 pmic_rev;
	u8 vender_id;

	struct sm5713_platform_data *pdata;
};

enum sm5713_types {
	TYPE_SM5713,
};

enum sm5713_dev_types {
	DEV_TYPE_SM5713_MUIC = 0x0,
	DEV_TYPE_SM5713_CCIC = 0x1,
};

extern int sm5713_irq_init(struct sm5713_dev *sm5713);
extern void sm5713_irq_exit(struct sm5713_dev *sm5713);

/* SM5713 shared i2c API function */
extern int sm5713_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int sm5713_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5713_read_word(struct i2c_client *i2c, u8 reg);
extern int sm5713_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int sm5713_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5713_write_word(struct i2c_client *i2c, u8 reg, u16 value);

extern int sm5713_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sm5713_update_word(struct i2c_client *i2c, u8 reg, u16 val, u16 mask);

/* support SM5713 Charger operation mode control module */
enum {
    SM5713_CHARGER_OP_EVENT_VBUSIN      = 0x5,
	SM5713_CHARGER_OP_EVENT_USB_OTG     = 0x4,
    SM5713_CHARGER_OP_EVENT_PWR_SHAR    = 0x3,
    SM5713_CHARGER_OP_EVENT_FLASH       = 0x2,
    SM5713_CHARGER_OP_EVENT_TORCH       = 0x1,
    SM5713_CHARGER_OP_EVENT_SUSPEND     = 0x0,
};

extern int sm5713_charger_oper_table_init(struct sm5713_dev *sm5713);
extern int sm5713_charger_oper_push_event(int event_type, bool enable);
extern int sm5713_charger_oper_get_current_status(void);
extern int sm5713_charger_oper_get_current_op_mode(void);
extern int sm5713_charger_oper_get_vbus_voltage(void);
extern int sm5713_charger_oper_en_factory_mode(int dev_type, int rid, bool enable);

#endif /* __SM5713_PRIV_H__ */

