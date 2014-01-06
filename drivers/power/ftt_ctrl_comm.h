#ifndef __FTT_CTRL_COMM_H__
#define __FTT_CTRL_COMM_H__

/***** feature define enable/disable *****/
#define FTT_CHARGER_DEBUG									1
#define FTT_STATISTICS_DEBUG								1
#define FTT_UEVENT											1
#define FTT_CHARACTER_DEVICE								1
#define FTT_CHARGER_STATUS_TIMER							1
#define FTT_CHIP_ENABLE_PIN_USE								0
#define FTT_FILE_OPEN_ENABLE								1
#define FTT_FREQUENCY_ROUND									1
#define FTT_FREQ_CORRECTION									1
#define FTT_FREQ_CORRECTION_TABLE							1

#define FTT_DD_MAJOR_VERSION								3
#define FTT_DD_MINOR_VERSION_A								2
#define FTT_DD_MINOR_VERSION_B								1

#define FTT_SD_MAJOR_VERSION								FTT_DD_MAJOR_VERSION
#define FTT_SD_MINOR_VERSION_A								FTT_DD_MINOR_VERSION_A
#define FTT_SD_MINOR_VERSION_B								FTT_DD_MINOR_VERSION_B


#define FTT_LOG_SAMPLING				11
/* #define FTT_LOG_PAD_DETECT			12 */
#define FTT_LOG_LEVEL				13
#define FTT_LOG_PING_TEST			20
#define FTT_LOG_PAD_DETECT_TEST		21

#define FTT_LOG_LEVEL_ALWAYS		10
#define FTT_LOG_LEVEL_ASSERT		8
#define FTT_LOG_LEVEL_ERROR			7
#define FTT_LOG_LEVEL_WARN			6
#define FTT_LOG_LEVEL_INFO			5
#define FTT_LOG_LEVEL_DEBUG			4
#define FTT_LOG_LEVEL_VERBOSE_1		3
#define FTT_LOG_LEVEL_VERBOSE_2		2
#define FTT_LOG_LEVEL_VERBOSE_3		1

#define FTT_ALWAYS					FTT_LOG_LEVEL_ALWAYS
#define FTT_ASSERT					FTT_LOG_LEVEL_ASSERT
#define FTT_ERROR						FTT_LOG_LEVEL_ERROR
#define FTT_WARN						FTT_LOG_LEVEL_WARN
#define FTT_INFO						FTT_LOG_LEVEL_INFO
#define FTT_DEBUG						FTT_LOG_LEVEL_DEBUG
#define FTT_VERBOSE1					FTT_LOG_LEVEL_VERBOSE_1
#define FTT_VERBOSE2					FTT_LOG_LEVEL_VERBOSE_2
#define FTT_VERBOSE3					FTT_LOG_LEVEL_VERBOSE_3

#define FTT_CMD						FTT_LOG_LEVEL_INFO


#ifndef __KERNEL__
#define ftt_print				printf
#define FTT_LOGHEAD			"[ftt_ctrld] : "
#else /* __KERNEL__ */
#define ftt_print				printk
#define FTT_LOGHEAD			"[ftt_kernel] : "
#endif /* __KERNEL__ */

#define PRINT(x...) {\
	ftt_print(FTT_LOGHEAD x); \
}
#if FTT_CHARGER_DEBUG
extern unsigned int ftt_is_debug;
#define DPRINT(l, x...) {\
		if(((FTT_LOG_LEVEL_ALWAYS >= l) && (ftt_is_debug <= l)) || ((FTT_LOG_LEVEL_ALWAYS < l) && (ftt_is_debug == l))) ftt_print(FTT_LOGHEAD x); \
}
#define DPRINTC(l, x...) {\
		if(((FTT_LOG_LEVEL_ALWAYS >= l) && (ftt_is_debug <= l)) || ((FTT_LOG_LEVEL_ALWAYS < l) && (ftt_is_debug == l))) ftt_print(x); \
}
#else
#define DPRINT(l, x...)   /* !!!! */
#define DPRINTC(l, x...)   /* !!!! */
#endif


#define FTT_1n_SEC					(1)
#define FTT_10n_SEC					(10)
#define FTT_100n_SEC				(100)
#define FTT_1u_SEC					(1000)
#define FTT_10u_SEC					(10000)
#define FTT_100u_SEC				(100000)
#define FTT_1m_SEC					(1000000)
#define FTT_10m_SEC					(10000000)
#define FTT_100m_SEC				(100000000)
#define FTT_1_SEC					(1000000000)
#define FTT_AVERIGE_TIME			FTT_1m_SEC

#define FTT_DETECT_SAMPLE_COUNT					(10)

/****   level constant  *****/
#define FTT_BATT_CHARGING_WARRNING				(0)
#define FTT_BATT_CHARGING_NO_TABLE				(-1)
#define FTT_BATT_CHARGING_NO_CHARGING			(-2)
#define FTT_BATT_CHARGING_NO_DEFINED_TYPE		(-3)

struct ant_level_type {
	u32 ping_freq;
	u32 ant_level;
};

#if FTT_FREQ_CORRECTION_TABLE
struct ftt_freq_correction_type {
	u32 ftt_freq;
	s32 correction_freq;
};
#endif /* FTT_FREQ_CORRECTION_TABLE */


#define MAX_ANT_LEVEL				3

#define MAX_ANT_LEVEL_TABLE			10

#define CMD_GET_FTT_FREQ			0x11
#define CMD_GET_ONLINE				0x12
#define CMD_GET_FTT_INT_STATE		0x13
#define CMD_GET_FTT_DD_VERSION		0x14
#define CMD_SET_LEVEL				0x51
#define CMD_SET_FTT_INT_STATE		0x52
#define CMD_SET_PADTYPE				0x53

enum FTT_READ_COMMAND {
	RD_INTERRUPT = 0x01,
	RD_INTERRUPT_PING,
	RD_INTERRUPT_ONLINE,
	RD_SUSPEND,
	WT_PAD_TABLE,
	WT_DEBUG_LEVEL,
	WT_FTT_START,
	WT_FTT_STOP,
	WT_FTT_TIMER,
	WT_FTT_COUNT,
	WT_FTT_VALUE,
};

#define START_STOP_CHECK_CODE					0x3295d0b5

#define CMD_BUFFER_SIZE							200
#define MAX_READ_CMD_BUFFER						CMD_BUFFER_SIZE
#define MAX_WRITE_CMD_BUFFER					CMD_BUFFER_SIZE

#define FTT_READ_CMD_DATA_SIZE					4
#define FTT_READ_CMD_FREQ_SIZE					2	/* 2byte */

#define FTT_READ_CMD_INTERRUPT_PING_ARG_NUM		FTT_DETECT_SAMPLE_COUNT
#define FTT_READ_CMD_GET_LEVEL_ARG_NUM			4
#define FTT_READ_CMD_PUT_PAD_SIZE				4
#define FTT_READ_CMD_PUT_HYSTERESIS_SIZE		4
#define FTT_READ_CMD_PUT_PAD_STRING_SIZE		8
#define FTT_READ_CMD_PUT_TABLE_SIZE				4

#define FTT_PROTOCOL_VER						0x03
#define FTT_CHECKSUM							0x5d9328c2

struct ftt_cmd_hdr {
	u8 cmd;
	u8 ver;
	u8 reserved1;
	u8 reserved2;
	s32 checksum;
	u32 payload_len;
};

struct ftt_cmd_pad_table {
	s32 pad_type;
	char pad_name[FTT_READ_CMD_PUT_PAD_STRING_SIZE];
	u32 ftt_hysteresis;
	u32 pad_table_size;
	u32 pad_table;
};

enum status_timer {
	FTT_STATUS_TIMER_INIT,
	FTT_STATUS_TIMER_DET_ENTER,
	FTT_STATUS_TIMER_DET,
	FTT_STATUS_TIMER_PRECHG,
	FTT_STATUS_TIMER_PRECHG_TOTAL,
	FTT_STATUS_TIMER_CHG_ENTER,
	FTT_STATUS_TIMER_CHG,
	FTT_STATUS_TIMER_CHG_SUSPEND,
	FTT_STATUS_TIMER_MAX,
};

struct ftt_cmd_time_status_table {
	u16 ftt_status_timer[FTT_STATUS_TIMER_MAX];
};

enum ftt_count {
	FTT_COUNT_PING_POLLING_COUNT,
	FTT_COUNT_FREQUENCY_COMPARE_COUNT,
	FTT_COUNT_FREQUENCY_SAMPLE_COUNT,
	FTT_COUNT_MAX
};

struct ftt_cmd_count_table {
	u32 ftt_count[FTT_COUNT_MAX];
};

enum ftt_value {
	FTT_VALUE_SAMPLE,
	FTT_VALUE_MAX
};

struct ftt_cmd_value_table {
	u32 ftt_value[FTT_VALUE_MAX];
};

struct ftt_cmd_data {
	struct ftt_cmd_hdr cmd_hdr;
	union {
		u32 payload32;
		u16 payload16;
		u8 payload8;
		struct ftt_cmd_pad_table ppt;
		struct ftt_cmd_time_status_table tst;
		struct ftt_cmd_count_table fct;
		struct ftt_cmd_value_table fvt;
	} u;
};

#ifdef FTT_NETWORD_ORDER
#define F_HTONL(x)						htonl(x)
#define F_NTOHL(x)						ntohl(x)
#define F_HTONS(x)						htons(x)
#define F_NTOHS(x)						ntohs(x)
#else
#define F_HTONL(x)						(x)
#define F_NTOHL(x)						(x)
#define F_HTONS(x)						(x)
#define F_NTOHS(x)						(x)
#endif /* FTT_NETWORD_ORDER */

#endif /* __FTT_CTRL_COMM_H__ */
