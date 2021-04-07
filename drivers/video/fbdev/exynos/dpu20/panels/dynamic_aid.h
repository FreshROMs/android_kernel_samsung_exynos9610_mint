#ifndef __DYNAMIC_AID_H
#define __DYNAMIC_AID_H __FILE__

enum {
	CI_RED,
	CI_GREEN,
	CI_BLUE,
	CI_MAX
};

struct rgb_t {
	int rgb[CI_MAX];
};

struct formular_t {
	int numerator;
	int denominator;
};

struct dynamic_aid_param_t {
	int				vreg;
	int				vref_h;
	const int		*iv_tbl;
	int				iv_max;
	int				*mtp;
	const int		*gamma_default;
	const struct formular_t *formular;
	const int		*vt_voltage_value;

	const int	*ibr_tbl;
	int			ibr_max;
	const struct rgb_t	(*offset_color)[];
	int			*iv_ref;
	const int	(*m_gray)[];
};

extern int dynamic_aid(struct dynamic_aid_param_t d_aid, int **gamma);

#endif /* __DYNAMIC_AID_H */
