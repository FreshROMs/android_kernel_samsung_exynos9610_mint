#ifndef TFA2_DSP_FW_H
#define TFA2_DSP_FW_H

#include "tfa2_dev.h"

/*
 * the order matches the ACK bits order in TFA98XX_CF_STATUS
 */
enum tfa_fw_event {/* not all available on each device */
	TFA_FW_I2C_CMD_ACK,
	TFA_FW_RESET_START,
	TFA_FW_SHORT_ON_MIPS,
	TFA_FW_SOFT_MUTE_READY,
	TFA_FW_VOLUME_READY,
	TFA_FW_ERROR_DAMAGE,
	TFA_FW_CALIBRATE_DONE,
	TFA_FW_MAX
};

/* the following type mappings are compiler specific */
#define subaddress_t unsigned char

/* module Ids */
#define MODULE_FRAMEWORK        0
#define MODULE_SPEAKERBOOST     1
#define MODULE_BIQUADFILTERBANK 2
#define MODULE_TAPTRIGGER		5
#define MODULE_SETRE			9

/* RPC commands */
/* SET */
#define FW_PAR_ID_SET_MEMORY            0x03
#define FW_PAR_ID_SET_SENSES_DELAY      0x04
#define FW_PAR_ID_SETSENSESCAL          0x05
#define FW_PAR_ID_SET_INPUT_SELECTOR    0x06
#define FW_PAR_ID_SET_OUTPUT_SELECTOR   0x08
#define FW_PAR_ID_SET_PROGRAM_CONFIG    0x09
#define FW_PAR_ID_SET_GAINS             0x0A
#define FW_PAR_ID_SET_MEMTRACK          0x0B
#define FW_PAR_ID_SET_FWKUSECASE        0x11
#define TFA1_FW_PAR_ID_SET_CURRENT_DELAY 0x03
#define TFA1_FW_PAR_ID_SET_CURFRAC_DELAY 0x06
/* GET */
#define FW_PAR_ID_GET_MEMORY            0x83
#define FW_PAR_ID_GLOBAL_GET_INFO       0x84
#define FW_PAR_ID_GET_FEATURE_INFO      0x85
#define FW_PAR_ID_GET_MEMTRACK          0x8B
#define FW_PAR_ID_GET_TAG               0xFF
#define FW_PAR_ID_GET_API_VERSION       0xFE
#define FW_PAR_ID_GET_STATUS_CHANGE     0x8D

/* Load a full model into SpeakerBoost. */
/* SET */
#define SB_PARAM_SET_ALGO_PARAMS        0x00
#define SB_PARAM_SET_LAGW               0x01
#define SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET 0x02
#define SB_PARAM_SET_RE25C              0x05
#define SB_PARAM_SET_LSMODEL            0x06
#define SB_PARAM_SET_MBDRC              0x07
#define SB_PARAM_SET_MBDRC_WITHOUT_RESET 0x08
#define SB_PARAM_SET_EXCURSION_FILTERS	0x0A
#define SB_PARAM_SET_DATA_LOGGER        0x0D
#define SB_PARAM_SET_DRC                0x0F
/* GET */
#define SB_PARAM_GET_ALGO_PARAMS        0x80
#define SB_PARAM_GET_LAGW               0x81
#define SB_PARAM_GET_RE25C              0x85
#define SB_PARAM_GET_LSMODEL            0x86
#define SB_PARAM_GET_MBDRC	            0x87
#define SB_PARAM_GET_MBDRC_DYNAMICS     0x89
#define SB_PARAM_GET_EXCURSION_FILTERS	0x8A
#define SB_PARAM_GET_DATA_LOGGER        0x8D
#define SB_PARAM_GET_TAG                0xFF
#define FW_MAXTAG 150

#define SB_PARAM_SET_EQ                 0x0A /* 2 EQ filters */
#define SB_PARAM_SET_PRESET             0x0D /* Load a preset */
#define SB_PARAM_SET_CONFIG             0x0E /* Load a config */
#define SB_PARAM_SET_AGCINS             0x10
#define SB_PARAM_SET_CURRENT_DELAY      0x03
#define SB_PARAM_GET_STATE              0xC0
/* Get current Excursion Model */
#define SB_PARAM_GET_XMODEL             0xC1
/* Get coefficients for XModel */
#define SB_PARAM_GET_XMODEL_COEFFS      0x8C
/* Get excursion filters */
#define SB_PARAM_GET_EXCURSION_FILTERS  0x8A
/* Set excursion filters */
#define SB_PARAM_SET_EXCURSION_FILTERS  0x0A

/* SET: TAPTRIGGER */
#define TAP_PARAM_SET_ALGO_PARAMS       0x01
#define TAP_PARAM_SET_DECIMATION_PARAMS 0x02

/* GET: TAPTRIGGER */
#define TAP_PARAM_GET_ALGO_PARAMS		0x81
#define TAP_PARAM_GET_TAP_RESULTS		0x84

/* sets the speaker calibration impedance (@25 degrees celsius) */
#define SB_PARAM_SET_RE0                0x89

#define BFB_PAR_ID_SET_COEFS            0x00
#define BFB_PAR_ID_GET_COEFS            0x80
#define BFB_PAR_ID_GET_CONFIG           0x81

/* for compatibility */
#define FW_PARAM_GET_STATE          FW_PAR_ID_GLOBAL_GET_INFO
#define FW_PARAM_GET_FEATURE_BITS   FW_PAR_ID_GET_FEATURE_BITS

/* RPC Status results */
#define STATUS_OK                  0
#define STATUS_INVALID_MODULE_ID   2
#define STATUS_INVALID_PARAM_ID    3
#define STATUS_INVALID_INFO_ID     4

/* the maximum message length in the communication with the DSP */
#define TFA2_MAX_PARAM_SIZE (507*3) /* TFA2 */
#define TFA1_MAX_PARAM_SIZE (145*3) /* TFA1 */

#define ROUND_DOWN(a, n) (((a)/(n))*(n))

/* feature bits */
#define FEATURE1_TCOEF 0x100 /* bit8 set means tCoefA expected */
#define FEATURE1_DRC   0x200 /* bit9 NOT set means DRC expected */

/* DSP firmware xmem defines */
#define TFA2_FW_XMEM_CALIBRATION_DONE   516
#define TFA2_FW_XMEM_COUNT_BOOT         512
#define TFA2_FW_XMEM_CMD_COUNT          520

/* note that the following defs rely on the handle variable */
#define TFA_FW_XMEM_CALIBRATION_DONE	\
	TFA_FAM_FW(tfa, XMEM_CALIBRATION_DONE)
#define TFA_FW_XMEM_COUNT_BOOT	TFA_FAM_FW(tfa, XMEM_COUNT_BOOT)
#define TFA_FW_XMEM_CMD_COUNT	TFA_FAM_FW(tfa, XMEM_CMD_COUNT)

#define TFA2_FW_ReZ_SCALE	65536

#define TFA2_FW_X_DATA_SCALE	2097152
#define TFA2_FW_T_DATA_SCALE	16384
#endif /* TFA2_DSP_FW_H */
