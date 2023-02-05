/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <linux/types.h>

struct dma_buf;

/**
 * gpexbe_dmabuf_is_cached() - check if a buffer is cpu cached
 * @dmabuf: pointer to dma_buf buffer
 *
 * Used by LEGACY_COMPAT option to check if a buffer is cached
 * before trying to sync it
 *
 * Return: true if cached
 */
bool gpexbe_dmabuf_is_cached(struct dma_buf *dmabuf);
