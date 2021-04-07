#ifndef __ASV_EXYNOS9810_H__
#define __ASV_EXYNOS9810_H__

#include <linux/io.h>

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

struct asv_tbl_info {
	unsigned bigcpu_asv_group:4;
	int      bigcpu_modify_group:4;
	unsigned ssa_reserved1:4;
	unsigned bigcpu_ssa0:4;
	unsigned littlecpu_asv_group:4;
	int      littlecpu_modify_group:4;
	unsigned ssa_reserved2:4;
	unsigned littlecpu_ssa0:4;

	unsigned g3d_asv_group:4;
	int      g3d_modify_group:4;
	unsigned ssa_reserved3:4;
	unsigned g3d_ssa0:4;
	unsigned mif_asv_group:4;
	int      mif_modify_group:4;
	unsigned ssa_reserved4:4;
	unsigned mif_ssa0:4;

	unsigned int_asv_group:4;
	int      int_modify_group:4;
	unsigned ssa_reserved5:4;
	unsigned int_ssa0:4;
	unsigned cam_disp_asv_group:4;
	int      cam_disp_modify_group:4;
	unsigned ssa_reserved6:4;
	unsigned cam_disp_ssa0:4;

	unsigned asv_table_version:4;
	unsigned asv_group_type:4;
	unsigned reserved_0:4;
	unsigned cp_asv_group:4;
	int      cp_modify_group:4;
	unsigned ssa_reserved7:4;
	unsigned cp_ssa0:4;

	unsigned big_vthr:2;
	unsigned big_delta:2;
	unsigned little_vthr:2;
	unsigned little_delta:2;
	unsigned g3d_vthr:2;
	unsigned g3d_delta:2;
	unsigned int_vthr:2;
	unsigned int_delta:2;
	unsigned mif_vthr:2;
	unsigned mif_delta:2;
	unsigned cp_vthr :2;
	unsigned cp_delta :2;
	unsigned fsys_vthr:2;
	unsigned fsys_delta:2;
	unsigned ssa_reserved8:4;

	unsigned fsys_asv_group:4;
	int      fsys_modify_group:4;
	unsigned fsys_reserved9:4;
	unsigned fsys_ssa0:4;
//	unsigned fsys_reserved9:16;
};




struct id_tbl_info {
	unsigned reserved_0;
	unsigned reserved_1;
	unsigned reserved_2:16;
	unsigned char ids_mngs:8;
	unsigned char ids_g3d:8;
	unsigned char ids_int:8;
	unsigned char ids_others:8; /* apollo, mif, disp, cp */
	unsigned reserved_3:16;
	unsigned reserved_4:16;
	unsigned short sub_rev:4;
	unsigned short main_rev:4;
	unsigned reserved_5:8;
};

static struct asv_tbl_info *asv_tbl;
static struct id_tbl_info *id_tbl;

int asv_get_grp(unsigned int id)
{
	int grp = -1;

	if (!asv_tbl)
		return grp;

	switch (id) {
	case dvfs_mif:
		grp = asv_tbl->mif_asv_group + asv_tbl->mif_modify_group;
		break;
	case dvfs_int:
	case dvfs_intcam:
		grp = asv_tbl->int_asv_group + asv_tbl->int_modify_group;
		break;
	case dvfs_cpucl0:
		grp = asv_tbl->bigcpu_asv_group + asv_tbl->bigcpu_modify_group;
		break;
	case dvfs_cpucl1:
		grp = asv_tbl->littlecpu_asv_group + asv_tbl->littlecpu_modify_group;
		break;
	case dvfs_g3d:
		grp = asv_tbl->g3d_asv_group + asv_tbl->g3d_modify_group;
		break;
	case dvfs_cam:
	case dvfs_disp:
		grp = asv_tbl->cam_disp_asv_group + asv_tbl->cam_disp_modify_group;
		break;
	case dvfs_cp:
		grp = asv_tbl->cp_asv_group + asv_tbl->cp_modify_group;
		break;
	case dvfs_fsys0:
		grp = asv_tbl->fsys_asv_group + asv_tbl->fsys_modify_group;
		break;
	default:
		pr_info("Un-support asv grp %d\n", id);
	}

	return grp;
}

int asv_get_ids_info(unsigned int id)
{
	int ids = 0;

	if (!id_tbl)
		return ids;

	switch (id) {
	case dvfs_cpucl0:
		ids = id_tbl->ids_mngs;
		break;
	case dvfs_g3d:
		ids = id_tbl->ids_g3d;
		break;
	case dvfs_int:
	case dvfs_intcam:
		ids = id_tbl->ids_int;
		break;
	case dvfs_cpucl1:
	case dvfs_mif:
	case dvfs_disp:
	case dvfs_cp:
	case dvfs_cam:
		ids = id_tbl->ids_others;
		break;
	default:
		pr_info("Un-support ids info %d\n", id);
	}

	return ids;
}

int asv_get_table_ver(void)
{
	int ver = 0;

	if (asv_tbl)
		ver = asv_tbl->asv_table_version;

	return ver;
}

int id_get_rev(void)
{
	int rev = 0;

	if (id_tbl)
		rev = id_tbl->main_rev;

	return rev;
}

int asv_table_init(void)
{
	asv_tbl = ioremap(ASV_TABLE_BASE, SZ_4K);
	if (!asv_tbl)
		return 0;

	pr_info("asv_table_version : %d\n", asv_tbl->asv_table_version);
	pr_info("  bigcpu grp : %d\n", asv_tbl->bigcpu_asv_group);
	pr_info("  littlecpu grp : %d\n", asv_tbl->littlecpu_asv_group);
	pr_info("  g3d grp : %d\n", asv_tbl->g3d_asv_group);
	pr_info("  mif grp : %d\n", asv_tbl->mif_asv_group);
	pr_info("  int grp : %d\n", asv_tbl->int_asv_group);
	pr_info("  cam_disp grp : %d\n", asv_tbl->cam_disp_asv_group);
	pr_info("  cp grp : %d\n", asv_tbl->cp_asv_group);
	pr_info("  fsys grp : %d\n", asv_tbl->fsys_asv_group);

	id_tbl = ioremap(ID_TABLE_BASE, SZ_4K);

	return asv_tbl->asv_table_version;
}
#endif
