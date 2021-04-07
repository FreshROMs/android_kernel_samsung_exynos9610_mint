#ifndef __MDNIE_H__
#define __MDNIE_H__

typedef u8 mdnie_t;

enum MODE {
	DYNAMIC,
	STANDARD,
	NATURAL,
	MOVIE,
	AUTO,
	EBOOK,
	MODE_MAX
};

enum SCENARIO {
	UI_MODE,
	VIDEO_NORMAL_MODE,
	CAMERA_MODE = 4,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	BROWSER_MODE,
	EBOOK_MODE,
	EMAIL_MODE,
	HMT_8_MODE,
	HMT_16_MODE,
	SCENARIO_MAX,
	DMB_NORMAL_MODE = 20,
	DMB_MODE_MAX
};

enum BYPASS {
	BYPASS_OFF,
	BYPASS_ON,
	BYPASS_MAX
};

enum LIGHT_NOTIFICATION {
	LIGHT_NOTIFICATION_OFF,
	LIGHT_NOTIFICATION_ON,
	LIGHT_NOTIFICATION_MAX
};

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	GRAYSCALE,
	GRAYSCALE_NEGATIVE,
	ACCESSIBILITY_MAX
};

enum HBM {
	HBM_OFF,
	HBM_ON,
	HBM_MAX
};

enum COLOR_OFFSET_FUNC {
	COLOR_OFFSET_FUNC_NONE,
	COLOR_OFFSET_FUNC_F1,
	COLOR_OFFSET_FUNC_F2,
	COLOR_OFFSET_FUNC_F3,
	COLOR_OFFSET_FUNC_F4,
	COLOR_OFFSET_FUNC_MAX
};

enum hmt_mode {
	HMT_MDNIE_OFF,
	HMT_MDNIE_ON,
	HMT_3000K = HMT_MDNIE_ON,
	HMT_4000K,
	HMT_6400K,
	HMT_7500K,
	HMT_MDNIE_MAX
};

enum NIGHT_MODE {
	NIGHT_MODE_OFF,
	NIGHT_MODE_ON,
	NIGHT_MODE_MAX
};

enum COLOR_LENS {
	COLOR_LENS_OFF,
	COLOR_LENS_ON,
	COLOR_LENS_MAX
};

struct mdnie_seq_info {
	mdnie_t *cmd;
	unsigned int len;
	unsigned int sleep;
};

struct mdnie_table {
#define MDNIE_IDX_MAX	8
	char *name;
	unsigned int update_flag[MDNIE_IDX_MAX];
	struct mdnie_seq_info seq[MDNIE_IDX_MAX + 1];
};

struct mdnie_scr_info {
	u32 index;
	u32 cr;
	u32 wr;
	u32 wg;
	u32 wb;
};

struct mdnie_trans_info {
	u32 index;
	u32 offset;
	u32 enable;
};

struct mdnie_night_info {
	u32 max_w;
	u32 max_h;
};

struct mdnie_color_lens_info {
	u32 max_color;
	u32 max_level;
	u32 max_w;
};

struct mdnie_tune {
	struct mdnie_table	*bypass_table;
	struct mdnie_table	*accessibility_table;
	struct mdnie_table	*light_notification_table;
	struct mdnie_table	*hbm_table;
	struct mdnie_table	*hmt_table;
	struct mdnie_table	(*main_table)[MODE_MAX];
	struct mdnie_table	*dmb_table;
	struct mdnie_table	*night_table;
	struct mdnie_table	*lens_table;

	struct mdnie_scr_info	*scr_info;
	struct mdnie_trans_info	*trans_info;
	struct mdnie_night_info	*night_info;
	struct mdnie_color_lens_info *color_lens_info;
	unsigned char **coordinate_table;
	unsigned char **adjust_ldu_table;
	unsigned char *night_mode_table;
	unsigned char *color_lens_table;
	int (*get_hbm_index)(int);
	int (*color_offset[])(int, int);
};

struct rgb_info {
	int r;
	int g;
	int b;
};

struct mdnie_ops {
	int (*write)(void *data, struct mdnie_seq_info *seq, u32 len);
	int (*read)(void *data, u8 addr, mdnie_t *buf, u32 len);
};

typedef int (*mdnie_w)(void *devdata, struct mdnie_seq_info *seq, u32 len);
typedef int (*mdnie_r)(void *devdata, u8 addr, u8 *buf, u32 len);


struct mdnie_info {
	struct device		*dev;
	struct mutex		dev_lock;
	struct mutex		lock;

	unsigned int		enable;
	unsigned int		lpm;

	struct mdnie_tune	*tune;

	enum SCENARIO		scenario;
	enum MODE		mode;
	enum BYPASS		bypass;
	enum HBM		hbm;
	enum hmt_mode		hmt_mode;
	enum NIGHT_MODE	night_mode;
	enum LIGHT_NOTIFICATION		light_notification;
	enum COLOR_LENS	color_lens;

	unsigned int		tuning;
	unsigned int		accessibility;
	unsigned int		color_correction;
	unsigned int		coordinate[2];

	char			path[50];

	void			*dd_mdnie;

	void			*data;

	struct mdnie_ops	ops;

	struct notifier_block	fb_notif;
#ifdef CONFIG_DISPLAY_USE_INFO
	struct notifier_block	dpui_notif;
#endif

	struct rgb_info		wrgb_current;
	struct rgb_info		wrgb_default;
	struct rgb_info		wrgb_balance;
	struct rgb_info		wrgb_ldu;

	unsigned int disable_trans_dimming;
	unsigned int night_mode_level;
	unsigned int color_lens_color;
	unsigned int color_lens_level;
	unsigned int ldu;

	struct mdnie_table table_buffer;
	mdnie_t sequence_buffer[256];
};

extern int mdnie_register(struct device *p, void *data, mdnie_w w, mdnie_r r, unsigned int *coordinate, struct mdnie_tune *tune);
extern ssize_t attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size);
extern struct class *get_mdnie_class(void);

#endif /* __MDNIE_H__ */
