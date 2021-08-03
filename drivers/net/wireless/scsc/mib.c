/******************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd and its Licensors.
 * All rights reserved.
 *
 *****************************************************************************/

#include "const.h"
#include "mib.h"
#include "debug.h"

#define SLSI_MIB_MORE_MASK   0x80
#define SLSI_MIB_SIGN_MASK   0x40
#define SLSI_MIB_TYPE_MASK   0x20
#define SLSI_MIB_LENGTH_MASK 0x1FU
/**
 * @brief
 *  Append a buffer to an existing buffer.
 *  This will kmalloc a new buffer and kfree the old one
 */
void slsi_mib_buf_append(struct slsi_mib_data *dst, size_t buffer_length, u8 *buffer)
{
	u8 *new_buffer = kmalloc(dst->dataLength + buffer_length, GFP_KERNEL);

	if (!new_buffer) {
		SLSI_ERR_NODEV("kmalloc(%d) failed\n", (int)(dst->dataLength + buffer_length));
		return;
	}

	memcpy(new_buffer, dst->data, dst->dataLength);
	memcpy(&new_buffer[dst->dataLength], buffer, buffer_length);

	dst->dataLength += (u16)buffer_length;
	kfree(dst->data);
	dst->data = new_buffer;
}

size_t slsi_mib_encode_uint32(u8 *buffer, u32 value)
{
	u8 i;
	u8 write_count = 0;

	if (value < 64) {
		buffer[0] = (u8)value;
		return 1;
	}

	/* Encode the Integer
	 *  0xABFF0055 = [0xAB, 0xFF, 0x00, 0x55]
	 *    0xAB0055 = [0xAB, 0x00, 0x55]
	 *      0xAB55 = [0xAB, 0x55]
	 *        0x55 = [0x55]
	 */
	for (i = 0; i < 4; i++) {
		u8 byte_value = (value & 0xFF000000) >> 24;

		if (byte_value || write_count) {
			buffer[1 + write_count] = byte_value;
			write_count++;
		}
		value = value << 8;
	}

	/* vldata Length | more bit */
	buffer[0] = write_count | SLSI_MIB_MORE_MASK;

	return 1 + write_count;
}

size_t slsi_mib_encode_int32(u8 *buffer, s32 signed_value)
{
	u8  i;
	u8  write_count = 0;
	u32 value = (u32)signed_value;

	if (!(value & 0x10000000))
		/* just use the Unsigned Encoder */
		return slsi_mib_encode_uint32(buffer, value);

	if (signed_value >= -64) {
		buffer[0] = (u8)value & 0x7F; /* vldata Length | more bit */
		return 1;
	}

	/* Encode the Negative Integer */
	for (i = 0; i < 4; i++) {
		u8 byte_value = (value & 0xFF000000) >> 24;

		if (!((byte_value == 0xFF) && (value & 0x800000)) || write_count) {
			buffer[1 + write_count] = byte_value;
			write_count++;
		}
		value = value << 8;
	}
	/* vldata Length | more bit | sign bit*/
	buffer[0] = write_count | SLSI_MIB_MORE_MASK | SLSI_MIB_SIGN_MASK;

	return 1 + write_count;
}

size_t slsi_mib_encode_octet_str(u8 *buffer, struct slsi_mib_data *octet_value)
{
	u8     i;
	u8     write_count = 0;
	size_t length = octet_value->dataLength;
	size_t ret_length = 0;

	/* Encode the Length (Up to 4 bytes 32 bits worth)
	 *  0xABFF0000 = [0xAB, 0xFF, 0x00, 0x00]
	 *    0xAB0000 = [0xAB, 0x00, 0x00]
	 *      0xAB00 = [0xAB, 0x00]
	 *        0x00 = [0x00]
	 */
	for (i = 0; i < 3; i++) {
		u8 byte_value = (length & 0xFF000000) >> 24;

		if (byte_value || write_count) {
			buffer[1 + write_count] = byte_value;
			write_count++;
		}
		length = length << 8;
	}

	buffer[0] = (1 + write_count) | SLSI_MIB_MORE_MASK | SLSI_MIB_TYPE_MASK;
	buffer[1 + write_count] = octet_value->dataLength & 0xFF;
	memcpy(&buffer[2 + write_count], octet_value->data, octet_value->dataLength);

	ret_length = (size_t)(2U + write_count + octet_value->dataLength);
	return ret_length;
}

size_t slsi_mib_decode_uint32(u8 *buffer, u32 *value)
{
	size_t i;
	u32    v = 0;
	size_t length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	if (!(buffer[0] & SLSI_MIB_MORE_MASK)) {
		*value = buffer[0] & 0x7F;
		return 1;
	}

	for (i = 0; i < length; i++) {
		v = (v << 8);
		v |= buffer[1 + i];
	}

	*value = v;

	return 1 + length;
}

size_t slsi_mib_decodeUint64(u8 *buffer, u64 *value)
{
	size_t i;
	u64    v = 0;
	size_t length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	if (!(buffer[0] & SLSI_MIB_MORE_MASK)) {
		*value = buffer[0] & 0x7F;
		return 1;
	}

	for (i = 0; i < length; i++) {
		v = (v << 8);
		v |= buffer[1 + i];
	}

	*value = v;

	return 1 + length;
}

size_t slsi_mib_decodeInt32(u8 *buffer, s32 *value)
{
	size_t i;
	u32    v = 0xFFFFFFFF;
	size_t length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	if (!(buffer[0] & SLSI_MIB_SIGN_MASK))
		/* just use the Unsigned Decoder */
		return slsi_mib_decode_uint32(buffer, (u32 *)value);

	if (!(buffer[0] & SLSI_MIB_MORE_MASK)) {
		*value = (s32)(0xFFFFFF80 | buffer[0]);
		return 1;
	}

	for (i = 0; i < length; i++) {
		v = (v << 8);
		v |= buffer[1 + i];
	}

	*value = (s32)v;

	return 1 + length;
}

size_t slsi_mib_decodeInt64(u8 *buffer, s64 *value)
{
	size_t i;
	u64    v = 0xFFFFFFFFFFFFFFFFULL;
	size_t length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	if (!(buffer[0] & SLSI_MIB_SIGN_MASK))
		/* just use the Unsigned Decoder */
		return slsi_mib_decodeUint64(buffer, (u64 *)value);

	if (!(buffer[0] & SLSI_MIB_MORE_MASK)) {
		*value = (s64)(0xFFFFFFFFFFFFFF80ULL | buffer[0]);
		return 1;
	}

	for (i = 0; i < length; i++) {
		v = (v << 8);
		v |= buffer[1 + i];
	}

	*value = (s64)v;

	return 1 + length;
}

/* Just references the oid in the existing buffer. No new memory is allcated */
size_t slsi_mib_decode_octet_str(u8 *buffer, struct slsi_mib_data *octet_value)
{
	size_t i;
	u32    oid_length_value = 0;
	size_t length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	for (i = 0; i < length; i++) {
		oid_length_value = (oid_length_value << 8);
		oid_length_value |= buffer[1 + i];
	}

	octet_value->dataLength = oid_length_value;
	octet_value->data = NULL;
	if (oid_length_value)
		octet_value->data = &buffer[1 + length];

	return 1 + length + oid_length_value;
}

static u8 slsi_mib_decode_type_length(u8 *buffer, size_t *length)
{
	*length = 1;
	if (buffer[0] & SLSI_MIB_MORE_MASK)
		*length = buffer[0] & SLSI_MIB_LENGTH_MASK;

	if (buffer[0] & SLSI_MIB_SIGN_MASK)
		return SLSI_MIB_TYPE_INT;

	if ((buffer[0] & SLSI_MIB_MORE_MASK) &&
	    (buffer[0] & SLSI_MIB_TYPE_MASK)) {
		size_t i;
		size_t oid_length_value = 0;

		for (i = 0; i < *length; i++) {
			oid_length_value = (oid_length_value << 8);
			oid_length_value |= buffer[1 + i];
		}
		*length += oid_length_value;
		return SLSI_MIB_TYPE_OCTET;
	}
	return SLSI_MIB_TYPE_UINT;
}

static size_t slsi_mib_encode_psid_indexs(u8 *buffer, const struct slsi_mib_get_entry *value)
{
	size_t i;
	int index = 0;

	SLSI_U16_TO_BUFF_LE(value->psid, &buffer[0]);
	buffer[2] = 0;
	buffer[3] = 0;
	for (i = 0; i < SLSI_MIB_MAX_INDEXES && value->index[i] != 0; i++) {
		/* index should be less than 13 because size of the buffer is 13 */
		index = 4 + buffer[2];
		if (index < 13)
			buffer[2] += (u8)slsi_mib_encode_uint32(&buffer[index], value->index[i]);
	}

	if (buffer[2] % 2 == 1) {
		/* Add a padding byte "0x00" to the encoded buffer. The Length
		 * value is NOT updated to account for this pad value. If the
		 * length is an Odd number the Pad values MUST be there if it
		 * is Even it will not be.
		 */
		index = 4 + buffer[2];
		/* index should be less than 13 because size of the buffer is 13 */
		if (index < 13) {
			buffer[index] = 0x00;
			return 5 + buffer[2];
		}
	}

	return 4 + buffer[2];
}

u16 slsi_mib_encode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value)
{
	size_t i;
	size_t required_size =  (size_t)(5U + (5U * SLSI_MIB_MAX_INDEXES) +
			      (value->value.type == SLSI_MIB_TYPE_OCTET ? value->value.u.octetValue.dataLength : 5U));
	size_t encoded_length = 4;

	u8     *tmp_buffer = kmalloc(required_size, GFP_KERNEL);

	if (!tmp_buffer) {
		SLSI_ERR_NODEV("kmalloc(%d) failed\n", (int)required_size);
		return SLSI_MIB_STATUS_FAILURE;
	}

	SLSI_U16_TO_BUFF_LE(value->psid, &tmp_buffer[0]);
	tmp_buffer[2] = 0;
	tmp_buffer[3] = 0;
	for (i = 0; i < SLSI_MIB_MAX_INDEXES && value->index[i] != 0; i++)
		tmp_buffer[2] += (u8)slsi_mib_encode_uint32(&tmp_buffer[4 + tmp_buffer[2]], value->index[i]);
	encoded_length += tmp_buffer[2];

	switch (value->value.type) {
	case SLSI_MIB_TYPE_UINT:
		encoded_length += slsi_mib_encode_uint32(&tmp_buffer[encoded_length], value->value.u.uintValue);
		break;
	case SLSI_MIB_TYPE_INT:
		encoded_length += slsi_mib_encode_int32(&tmp_buffer[encoded_length], value->value.u.intValue);
		break;
	case SLSI_MIB_TYPE_OCTET:
		encoded_length += slsi_mib_encode_octet_str(&tmp_buffer[encoded_length], &value->value.u.octetValue);
		break;
	case SLSI_MIB_TYPE_BOOL:
		encoded_length += slsi_mib_encode_uint32(&tmp_buffer[encoded_length], value->value.u.boolValue ? true : false);
		break;
	case SLSI_MIB_TYPE_NONE:
		break;
	default:
		SLSI_WARN_NODEV("Invalid Type:%d requested\n", value->value.type);
		kfree(tmp_buffer);
		return SLSI_MIB_STATUS_FAILURE;
	}

	SLSI_U16_TO_BUFF_LE(encoded_length - 4, &tmp_buffer[2]); /* length */

	if (encoded_length % 2 == 1) {
		/* Add a padding byte "0x00" to the encoded buffer. The Length
		 * value is NOT updated to account for this pad value. If the
		 * length is an Odd number the Pad values MUST be there if it
		 * is Even it will not be.
		 */
		tmp_buffer[encoded_length] = 0x00;
		encoded_length++;
	}

	slsi_mib_buf_append(buffer, encoded_length, tmp_buffer);
	kfree(tmp_buffer);

	return SLSI_MIB_STATUS_SUCCESS;
}

size_t slsi_mib_decode(struct slsi_mib_data *data, struct slsi_mib_entry *value)
{
	u8     *buffer = data->data;
	u32    buffer_length = data->dataLength;
	size_t index_count = 0;
	u32 length;
	size_t decoded_length = 4;

	memset(value, 0x00, sizeof(struct slsi_mib_entry));

	if (buffer_length < 4) {
		SLSI_WARN_NODEV("Mib Decode Length:%d Must be greater than 4\n", buffer_length);
		return 0;
	}

	if (!buffer)
		return 0;

	length = SLSI_BUFF_LE_TO_U16(&buffer[2]);

	if (buffer_length < decoded_length + length) {
		SLSI_ERR_NODEV("Mib Buffer Length:%d Must be >= than decoded length:%d\n", buffer_length, (int)(decoded_length + length));
		return 0;
	}

	value->psid = SLSI_BUFF_LE_TO_U16(buffer);
	value->value.type = SLSI_MIB_TYPE_NONE;

	while (decoded_length < 4 + length) {
		size_t next_value_length;
		u8     type = slsi_mib_decode_type_length(&buffer[decoded_length], &next_value_length);

		if (buffer_length < decoded_length + next_value_length) {
			SLSI_ERR_NODEV("Mib Buffer Length:%d Must be >= than decoded length:%d\n", buffer_length, (int)(decoded_length + next_value_length));
			memset(value, 0x00, sizeof(struct slsi_mib_entry));
			return 0;
		}

		switch (type) {
		case SLSI_MIB_TYPE_UINT:
		{
			u32 v;

			decoded_length += slsi_mib_decode_uint32(&buffer[decoded_length], &v);
			/* If this is that last value then it is the "unitValue"
			 * if other values follow it is an Index Value
			 */
			if ((decoded_length < 4 + length) &&
			    (index_count != SLSI_MIB_MAX_INDEXES)) {
				value->index[index_count] = (u16)v;
				index_count++;
			} else {
				value->value.type = type;
				value->value.u.uintValue = v;
				if (decoded_length != 4 + length)
					SLSI_WARN_NODEV("Uint Decode length:%d != expected:%d\n", (u32)decoded_length, (u32)(4 + length));
			}
			break;
		}
		case SLSI_MIB_TYPE_INT:
			value->value.type = type;
			decoded_length += slsi_mib_decodeInt32(&buffer[decoded_length], &value->value.u.intValue);
			if (decoded_length != 4 + length)
				SLSI_WARN_NODEV("Int Decode length:%d != expected:%d\n", (u32)decoded_length, (u32)(4 + length));
			break;
		case SLSI_MIB_TYPE_OCTET:
			value->value.type = type;
			decoded_length += slsi_mib_decode_octet_str(&buffer[decoded_length], &value->value.u.octetValue);
			if (decoded_length != 4 + length)
				SLSI_WARN_NODEV("Octet Decode length:%d != expected:%d\n", (u32)decoded_length, (u32)(4 + length));
			break;
		default:
			SLSI_ERR_NODEV("Invalid MIB data type(%d). Possible mbulk corruption\n", type);
			memset(value, 0x00, sizeof(struct slsi_mib_entry));
			value->value.type = SLSI_MIB_TYPE_NONE;
			return 0;
		}
	}
	if (length % 2 == 1) {
		/* Remove the padding byte "0x00" in the encoded buffer.
		 * The Length value does NOT account for this pad value
		 * If the length is an Odd number the Pad values MUST be
		 * there if it is Even it will not be.
		 */
		if (buffer[decoded_length] != 0x00)
			SLSI_WARN_NODEV("psid:0x%.4X Padding Not Detected\n", value->psid);
		length++;
	}
	return 4 + length;
}

int slsi_mib_encode_get_list(struct slsi_mib_data *buffer, u16 psids_length, const struct slsi_mib_get_entry *psids)
{
	size_t i;

	buffer->dataLength = 0;
	/* 13 Bytes per get will be loads of space for the max 3 indexes */
	buffer->data = kmalloc((u32)(psids_length * 13), GFP_KERNEL);
	if (!buffer->data) {
		SLSI_ERR_NODEV("kmalloc(%d) failed\n", psids_length * 13);
		return SLSI_MIB_STATUS_OUT_OF_MEMORY;
	}
	for (i = 0; i < psids_length; i++)
		buffer->dataLength += (u16)slsi_mib_encode_psid_indexs(&buffer->data[buffer->dataLength], &psids[i]);

	return SLSI_MIB_STATUS_SUCCESS;
}

void slsi_mib_encode_get(struct slsi_mib_data *buffer, u16 psid, u16 idx)
{
	/* 13 Bytes per get will be loads of space for the max 3 indexes */
	size_t                    size;
	u8                        tmp_buffer[13];
	struct slsi_mib_get_entry entry;

	memset(&entry, 0x00, sizeof(struct slsi_mib_get_entry));
	entry.psid = psid;
	entry.index[0] = idx;
	size = slsi_mib_encode_psid_indexs(tmp_buffer, &entry);
	slsi_mib_buf_append(buffer, size, tmp_buffer);
}

u8 *slsi_mib_find(struct slsi_mib_data *buffer, const struct slsi_mib_get_entry *entry)
{
	size_t buffer_length = buffer->dataLength;
	u8     *buff = buffer->data;

	if (buffer_length % 2 == 1) {
		SLSI_WARN_NODEV("buffer_length(%d) %% 2 != 0 (Invalid Mib data Detected)\n", (int)buffer_length);
		return NULL;
	}
	while (buffer_length >= 4) {
		u16    psid = SLSI_BUFF_LE_TO_U16(buff);
		size_t length = 4U + SLSI_BUFF_LE_TO_U16(&buff[2]);

		if (entry->psid == psid) {
			size_t i;
			u32    idx;
			size_t bytes_read = 0;

			for (i = 0; i < SLSI_MIB_MAX_INDEXES; i++) {
				if (!entry->index[i])
					return buff;
				bytes_read = slsi_mib_decode_uint32(&buff[4 + bytes_read], &idx);
				if (entry->index[i] != idx)
					break;
			}
			if (i == SLSI_MIB_MAX_INDEXES)
				return buff;
		}
		if (length % 2 == 1)
			/* Remove the padding byte "0x00" in the encoded buffer.
			 * The Length value does NOT account for this pad value
			 * If the length is an Odd number the Pad values MUST be
			 * there if it is Even it will not be.
			 */
			length++;

		buff += length;
		buffer_length -= length;
	}

	return NULL;
}

struct slsi_mib_value *slsi_mib_decode_get_list(struct slsi_mib_data *buffer, u16 psids_length, const struct slsi_mib_get_entry *psids)
{
	struct slsi_mib_value *results = kmalloc_array((size_t)psids_length, sizeof(struct slsi_mib_value), GFP_KERNEL);
	size_t                i, mib_decode_len = 0;
	int len = 0;
	char psids_not_found[150] = "";

	if (!results) {
		SLSI_ERR_NODEV("kmalloc(%d) failed\n", (int)(sizeof(struct slsi_mib_value) * psids_length));
		return results;
	}

	for (i = 0; i < psids_length; i++) {
		struct slsi_mib_entry value;
		struct slsi_mib_data  data;

		data.data = slsi_mib_find(buffer, &psids[i]);
		if (data.data) {
			data.dataLength = buffer->dataLength - (data.data - buffer->data);
			value.psid = psids[i].psid;
			memcpy(value.index, psids[i].index, sizeof(value.index));
			mib_decode_len = slsi_mib_decode(&data, &value);
			if (mib_decode_len == 0)
				SLSI_DBG1_NODEV(SLSI_MLME, "Mib decode error for psid %d\n", value.psid);

			results[i] = value.value;
		} else {
			len += snprintf(&psids_not_found[0] + len, 150 - len, "%d ", psids[i].psid);
			results[i].type = SLSI_MIB_TYPE_NONE;
		}
	}

	if (len)
		SLSI_DBG1_NODEV(SLSI_MLME, "Could not find psid's: %s\n", psids_not_found);

	return results;
}

u16 slsi_mib_encode_bool(struct slsi_mib_data *buffer, u16 psid, bool value, u16 idx)
{
	struct slsi_mib_entry v;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_BOOL;
	v.value.u.boolValue = value;
	return slsi_mib_encode(buffer, &v);
}

u16 slsi_mib_encode_int(struct slsi_mib_data *buffer, u16 psid, s32 value, u16 idx)
{
	struct slsi_mib_entry v;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_INT;
	v.value.u.intValue = value;
	return slsi_mib_encode(buffer, &v);
}

u16 slsi_mib_encode_uint(struct slsi_mib_data *buffer, u16 psid, u32 value, u16 idx)
{
	struct slsi_mib_entry v;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_UINT;
	v.value.u.uintValue = value;
	return slsi_mib_encode(buffer, &v);
}

u16 slsi_mib_encode_octet(struct slsi_mib_data *buffer, u16 psid, size_t dataLength, const u8 *data, u16 idx)
{
	struct slsi_mib_entry v;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_OCTET;
	v.value.u.octetValue.dataLength = (u32)dataLength;
	v.value.u.octetValue.data = (u8 *)data;
	return slsi_mib_encode(buffer, &v);
}
