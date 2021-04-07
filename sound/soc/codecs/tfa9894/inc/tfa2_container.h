#ifndef TFA2_CONTAINER_H
#define TFA2_CONTAINER_H

#define TFA_MAX_CNT_LENGTH (256*1024)
#define MEMTRACK_MAX_WORDS           150
#define LSMODEL_MAX_WORDS            150
#define TFA98XX_MAXTAG              (150)
#define FW_VAR_API_VERSION          (521)

#include "tfa2_dev.h"
#include "tfa9xxx_parameters.h"

/* TODO move relevant comments */
#if 1
/* tfa2_container.c */
int tfa2_load_cnt(void *cnt, int length);
int tfa2_cnt_crc_check_container(struct tfa_container *cont);
char *tfa2_cnt_get_string(struct tfa_container *cnt,
	struct tfa_desc_ptr *dsc);
char *tfa2_cnt_device_name(struct tfa_container *cnt, int dev_idx);
char *tfa2_cnt_profile_name(struct tfa_container *cnt,
	int dev_idx, int prof_idx);
int tfa2_cnt_get_app_name(struct tfa2_device *tfa, char *name);
int tfa2_cnt_get_cmd(struct tfa_container *cnt, int devidx, int profidx,
	int offset, uint8_t **array, int *length);
void tfa2_cnt_show_header(struct tfa_header *hdr);
int tfa2_cnt_get_slave(struct tfa_container *cnt, int dev_idx);
struct tfa_device_list *tfa2_cnt_device(struct tfa_container *cnt,
	int dev_idx);
struct tfa_device_list *tfa2_cnt_get_dev_list(struct tfa_container *cont,
	int dev_idx);
struct tfa_profile_list *tfa2_cnt_get_dev_prof_list(struct tfa_container *cont,
	int dev_idx, int prof_idx);
int tfa2_dev_get_dev_nprof(struct tfa2_device *tfa);
int tfa2_cnt_get_dev_nprof(struct tfa_container *cnt, int dev_idx);
int tfa2_cnt_grep_profile_name(struct tfa_container *cnt,
	int dev_idx, const char *string);
int tfa2_cnt_grep_nth_profile_name(struct tfa_container *cnt,
	int dev_idx, int n, const char *string);
int tfa2_cnt_get_clockdep_idx(struct tfa2_device *tfa,
	struct tfa_desc_ptr *dsc_list, int length, int *clockdep_idx,
	int *default_section_idx);
int tfa2_cnt_write_regs_dev(struct tfa2_device *tfa);
int tfa2_cnt_write_regs_profile(struct tfa2_device *tfa, int prof_idx);
int tfa2_cnt_write_msg(struct tfa2_device *tfa, int wlength, char *wbuf);
int tfa2_cnt_write_patches(struct tfa2_device *tfa);
int tfa2_cnt_write_msg_dsc(struct tfa2_device *tfa,
	struct tfa_desc_ptr *dsc);
int tfa2_cnt_write_files(struct tfa2_device *tfa);
int tfa2_cnt_write_files_profile(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx);
int tfa2_cnt_write_transient_profile(struct tfa2_device *tfa,
	int prof_idx);
int tfa2_cnt_write_file(struct tfa2_device *tfa,
	struct tfa_file_dsc *file);
int tfa2_cnt_write_profile(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx);
int tfa2_cnt_get_idx(struct tfa2_device *tfa);

/* TODO: move to app top level? */
void tfa2_show_current_state(struct tfa2_device *tfa);

/* from tfa2_container_crc32.c */
uint32_t crc32_le(uint32_t crc, unsigned char const *buf, size_t len);
#else
/**
 * pass the container buffer, initialize and allocate internal memory.
 * @param cnt pointer to the start of the buffer holding the container file
 * @param length of the data in bytes
 * @return
 *  - tfa_error_ok if normal
 *  - tfa_error_container invalid container data
 *  - tfa_error_bad_param invalid parameter
 */
int tfa2_load_cnt(void *cnt, int length);

/*
 * return the descriptor string
 * @param cnt pointer to the container struct
 * @param dsc pointer to nxpTfa descriptor
 * @return descriptor string
 */
char *tfa2_cnt_get_string(struct tfa_container *cnt, struct tfa_desc_ptr *dsc);

/*
 * return the descriptor tfahal
 * @param cnt pointer to the container struct
 * @param dsc pointer to nxpTfa descriptor
 * @return descriptor tfahal
 */
char *tfa2_cont_get_tfahal(struct tfa_container *cnt, struct tfa_desc_ptr *dsc);

/*
 * gets the string for the given command type number
 * @param type number representing a command
 * @return string of a command
 */
char *tfa2_cnt_get_command_string(uint32_t type);

/*
 * get the device type from the patch in this devicelist
 *  - find the patch file for this devidx
 *  - return the devid from the patch or 0 if not found
 * @param cnt pointer to container file
 * @param dev_idx device index
 * @return descriptor string
 */
int tfa2_cnt_get_devid(struct tfa_container *cnt, int dev_idx);

/*
 * get the slave for the device if it exists.
 * @param cnt
 * @param the index of the device
 * @return slave
 */
int tfa2_cnt_get_slave(struct tfa_container *cnt, int dev_idx);

void tfa2_cnt_set_slave(uint8_t slave_addr);

/*
 * get the index for a slave address.
 * @param tfa the device struct pointer
 * @return the device index
 */
int tfa2_cnt_get_idx(struct tfa2_device *tfa);

/*
 * write reg and bitfield items in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return errno
 */
int tfa2_cnt_write_regs_dev(struct tfa2_device *tfa);

/*
 * write reg and bitfield items in the profilelist to the target.
 * @param tfa the device struct pointer
 * @param prof_idx the profile index
 * @return errno
 */
int tfa2_cnt_write_regs_profile(struct tfa2_device *tfa, int prof_idx);

/*
 * write a patchfile in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return errno
 */
int tfa2_cnt_write_patches(struct tfa2_device *tfa);

/*
 * write all param files in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return errno
 */
int tfa2_cnt_write_files(struct tfa2_device *tfa);

/*
 * get sample rate from passed profile index
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return sample rate value
 */
unsigned int tfa98xx_get_profile_sr(struct tfa2_device *tfa,
	unsigned int prof_idx);

/*
 * get the device name string
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @return device name string or error string if not found
 */
char *tfa2_cnt_device_name(struct tfa_container *cnt, int dev_idx);

/*
 * get the application name from the container file application field
 * @param tfa the device struct pointer
 * @param name the input stringbuffer with size: sizeof(application field)+1
 * @return actual string length
 */
int tfa2_cnt_get_app_name(struct tfa2_device *tfa, char *name);

/*
 * get profile index of the calibration profile
 * @param tfa the device struct pointer
 * @return profile index, -2 if no calibration profile is found or -1 on error
 */
int tfa2_cnt_get_cal_profile(struct tfa2_device *tfa);

/*
 * is the profile a tap profile?
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfa2_cnt_is_tap_profile(struct tfa2_device *tfa, int prof_idx);

/*
 * get the name of the profile at certain index for a device
 * in the container file
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile name string or NULL string if not found
 */
char *tfa2_cnt_profile_name(struct tfa_container *cnt,
	int dev_idx, int prof_idx);

/*
 * process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 * @param tfa the device struct pointer
 * @param prof_idx index of the profile
 * @param vstep_idx index of the vstep
 * @return errno
 */
int tfa2_cnt_write_profile(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx);

/*
 * specify the speaker configurations (cmd id) (Left, right, both, none)
 * @param dev_idx index of the device
 * @param configuration name string of the configuration
 */
void tfa98xx_set_spkr_select(int dev_idx, char *configuration);

int tfa2_cont_write_filterbank(struct tfa2_device *tfa,
	struct tfa_filter *filter);

/*
 * write all param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @param vstep_idx the index of the vstep
 * @return errno
 */
int tfa2_cnt_write_files_profile(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx);
/*
int tfa2_cnt_write_filesVstep(struct tfa2_device *tfa,
	int prof_idx, int vstep_idx);
*/
int tfa2_cnt_write_drc_file(struct tfa2_device *tfa,
	int size, uint8_t data[]);

/*
 * Get the device list dsc from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
struct tfa_device_list *tfa2_cnt_get_dev_list
(struct tfa_container *cont, int dev_idx);

/*
 * get the Nth profile for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile list pointer
 */
struct tfa_profile_list *tfa2_cnt_get_dev_prof_list
(struct tfa_container *cont, int dev_idx, int prof_idx);

/*
 * get the 1st profilename match for this device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param string to search
 * @return profile index
 */
int tfa2_cnt_grep_profile_name(struct tfa_container * cnt,
	int devidx, const char *string);

/*
 * get the number of profiles for device from container in tfa
 * @param tfa the device struct pointer
 * @return device list pointer
 */
int tfa2_dev_get_dev_nprof(struct tfa2_device *tfa);

/*
 * get the number of profiles for device from container
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
int tfa2_cnt_get_dev_nprof(struct tfa_container * cnt, int dev_idx);

/*
 * get the Nth livedata for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param livedata_idx the index of the livedata
 * @return livedata list pointer
 */
struct tfa_livedata_list *tfa2_cnt_get_dev_live_data_list
(struct tfa_container *cont, int dev_idx, int livedata_idx);

/*
 * check CRC for container
 * @param cont pointer to the tfaContainer
 * @return error value 0 on error
 */
int tfa2_cnt_crc_check_container(struct tfa_container *cont);

/*
 * get the device list pointer
 * @param cnt pointer to the container struct
 * @param dev_idx the index of the device
 * @return pointer to device list
 */
struct tfa_device_list *tfa2_cnt_device(struct tfa_container *cnt,
	int dev_idx);

/*
 * return the pointer to the first profile in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first profile in profile list
 */
struct tfa_profile_list *tfa2_cnt_get1st_prof_list
(struct tfa_container *cont);

/*
 * return the pointer to the next profile in a list
 * @param prof is the pointer to the profile list
 * @return profile list pointer
 */
struct tfa_profile_list* tfa2_cnt_next_profile
(struct tfa_profile_list *prof);

/*
 * return the pointer to the first livedata in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first livedata in profile list
 */
struct tfa_livedata_list *tfa2_cnt_get1st_live_data_list
(struct tfa_container *cont);

/*
 * return the pointer to the next livedata in a list
 * @param livedata_idx is the pointer to the livedata list
 * @return livedata list pointer
 */
struct tfa_livedata_list* tfa2_cnt_next_live_data
(struct tfa_livedata_list *livedata_idx);

/*
 * write a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to write
 * @return errno
 */
int tfa_run_write_bitfield
(struct tfa2_device *tfa, struct tfa_bitfield bf);

/*
 * write a parameter file to the device
 * @param tfa the device struct pointer
 * @param file filedescriptor pointer
 * @return errno
 */
int tfa2_cnt_write_file
(struct tfa2_device *tfa, struct tfa_file_dsc *file);

/*
 * get the max volume step associated with Nth profile for the Nth device
 * @param tfa the device struct pointer
 * @param prof_idx profile index
 * @return the number of vsteps
 */
int tfa_cont_get_max_vstep(struct tfa2_device *tfa, int prof_idx);

/*
 * get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 * @param tfa the device struct pointer
 * @param prof_idx I2C profile index in the device
 * @param type file type
 * @return 0 NULL if file type is not found
 * @return 1 file contents
 */
struct tfa_file_dsc *tfa_cont_get_file_data
(struct tfa2_device *tfa, int prof_idx, enum tfa_header_type type);

/*
 * dump the contents of the file header
 * @param hdr pointer to file header data
 */
void tfa2_cnt_show_header(struct tfa_header *hdr);

/*
 * read a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to read out
 * @return errno
 */
int tfa_run_read_bitfield
(struct tfa2_device *tfa, struct tfa_bitfield *bf);

/*
 * get hw feature bits from container file
 * @param tfa the device struct pointer
 * @param hw_feature_register pointer to where hw features are stored
 */
/*
void get_hw_features_from_cnt
(struct tfa2_device *tfa, int *hw_feature_register);
*/
/*
 * get sw feature bits from container file
 * @param tfa the device struct pointer
 * @param sw_feature_register pointer to where sw features are stored
 */
/*
void get_sw_features_from_cnt
(struct tfa2_device *tfa, int sw_feature_register[2]);
*/

/*
 * factory trimming for the Boost converter
 * check if there is a correction needed
 * @param tfa the device struct pointer
 */
int tfa98xx_factory_trimmer(struct tfa2_device *tfa);

/*
 * search for filters settings and if found then write them to the device
 * @param tfa the device struct pointer
 * @param prof_idx profile to look in
 * @return errno
 */
int tfa2_set_filters(struct tfa2_device *tfa, int prof_idx);

/*
 * get the firmware version from the patch in the container file
 * @param tfa the device struct pointer
 * @return firmware version
 */
int tfa2_cnt_get_patch_version(struct tfa2_device *tfa);

int tfa2_tib_dsp_msgmulti
(struct tfa2_device *tfa, int length, const char *buffer);

/*
 * get profile index of the calibration profile.
 * @param tfa the device struct pointer
 * @return (profile index) if found, (-2) if no calibration profile is found or (-1) on error
 */
int tfa2_cnt_get_main_profile(struct tfa2_device *tfa);

/*
 * write the rpc msg fomr the descriptor
 */
int tfa2_cnt_write_msg_dsc
(struct tfa2_device *tfa, struct tfa_desc_ptr * dsc);
#endif

#endif /* TFA2_CONTAINER_H */
