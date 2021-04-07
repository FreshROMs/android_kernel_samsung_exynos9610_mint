/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef FW_PANIC_RECORD_H__
#define FW_PANIC_RECORD_H__

bool fw_parse_r4_panic_record(u32 *r4_panic_record, u32 *r4_panic_record_length,
			      u32 *r4_panic_stack_record_offset, bool dump);
bool fw_parse_r4_panic_stack_record(u32 *r4_panic_stack_record, u32 *r4_panic_stack_record_length, bool dump);
bool fw_parse_m4_panic_record(u32 *m4_panic_record, u32 *m4_panic_record_length, bool dump);

bool fw_parse_get_r4_sympathetic_panic_flag(u32 *r4_panic_record);
bool fw_parse_get_m4_sympathetic_panic_flag(u32 *m4_panic_record);

int panic_record_dump_buffer(char *processor, u32 *panic_record,
			     u32 panic_record_length, char *buffer, size_t blen);
#endif /* FW_PANIC_RECORD_H__ */
