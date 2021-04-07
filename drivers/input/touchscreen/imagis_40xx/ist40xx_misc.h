/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST40XX_MISC_H__
#define __IST40XX_MISC_H__

#define NODE_FLAG_CDC		(1 << 0)
#define NODE_FLAG_BASE		(1 << 1)
#define NODE_FLAG_DIFF		(1 << 2)
#define NODE_FLAG_LOFS		(1 << 3)
#define NODE_FLAG_ALL		(0xF)

#define IST40XX_REC_FILENAME	"ist40xx.res"

int ist40xx_check_valid_ch(struct ist40xx_data *data, int ch_tx, int ch_rx);
int ist40xx_parse_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
        bool ium);
int parse_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
        s16 *buf16, s16 *self_buf16, s16 *prox_buf16, bool ium);
int ist40xx_read_cp_node(struct ist40xx_data *data, struct TSP_NODE_BUF *node,
        bool ium);
int ist40xx_parse_touch_node(struct ist40xx_data *data,
        struct TSP_NODE_BUF *node);
int parse_tsp_node(struct ist40xx_data *data, u8 flag,
        struct TSP_NODE_BUF *node, s16 *buf16, s16 *self_buf16,
        s16 *prox_buf16);
int ist40xx_read_touch_node(struct ist40xx_data *data, u8 flag,
        struct TSP_NODE_BUF *node);

int ist40xx_put_frame(struct ist40xx_data *data, u32 ms, u32 *touch, u32 *frame,
        int frame_cnt);
int ist40xx_recording_put_frame(struct ist40xx_data *data, u32 *frame,
        int frame_cnt);

int ist40xx_init_misc_sysfs(struct ist40xx_data *data);

#endif  // __IST40XX_MISC_H__
