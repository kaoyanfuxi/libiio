/*
 * libiio - Library for interfacing industrial I/O (IIO) devices
 *
 * Copyright (C) 2014 Analog Devices, Inc.
 * Author: Paul Cercueil <paul.cercueil@analog.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * */

#include "debug.h"
#include "iio-private.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static char *get_attr_xml(const char *attr, size_t *length)
{
	size_t len = sizeof("<attribute name=\"\" />") + strlen(attr);
	char *str = malloc(len);
	if (!str) {
		ERROR("Unable to allocate memory\n");
		return NULL;
	}

	*length = len - 1; /* Skip the \0 */
	snprintf(str, len, "<attribute name=\"%s\" />", attr);
	return str;
}

/* Returns a string containing the XML representation of this channel */
char * iio_channel_get_xml(const struct iio_channel *chn, size_t *length)
{
	size_t len = sizeof("<channel id=\"\" name=\"\" "
			"type=\"output\" scan_element=\"false\" ></channel>")
		+ strlen(chn->id) + (chn->name ? strlen(chn->name) : 0);
	char *ptr, *str, **attrs;
	size_t *attrs_len;
	unsigned int i;

	attrs_len = malloc(chn->nb_attrs * sizeof(*attrs_len));
	if (!attrs_len)
		return NULL;

	attrs = malloc(chn->nb_attrs * sizeof(*attrs));
	if (!attrs)
		goto err_free_attrs_len;

	for (i = 0; i < chn->nb_attrs; i++) {
		char *xml = get_attr_xml(chn->attrs[i], &attrs_len[i]);
		if (!xml)
			goto err_free_attrs;
		attrs[i] = xml;
		len += attrs_len[i];
	}

	str = malloc(len);
	if (!str)
		goto err_free_attrs;

	snprintf(str, len, "<channel id=\"%s\"", chn->id);
	ptr = strrchr(str, '\0');

	if (chn->name) {
		sprintf(ptr, " name=\"%s\"", chn->name);
		ptr = strrchr(ptr, '\0');
	}

	sprintf(ptr, " type=\"%s\" scan_element=\"%s\" >",
			chn->is_output ? "output" : "input",
			chn->is_scan_element ? "true" : "false");
	ptr = strrchr(ptr, '\0');

	for (i = 0; i < chn->nb_attrs; i++) {
		strcpy(ptr, attrs[i]);
		ptr += attrs_len[i];
		free(attrs[i]);
	}

	free(attrs);
	free(attrs_len);

	strcpy(ptr, "</channel>");
	*length = ptr - str + sizeof("</channel>") - 1;
	return str;

err_free_attrs:
	while (i--)
		free(attrs[i]);
	free(attrs);
err_free_attrs_len:
	free(attrs_len);
	return NULL;
}

const char * iio_channel_get_id(const struct iio_channel *chn)
{
	return chn->id;
}

const char * iio_channel_get_name(const struct iio_channel *chn)
{
	return chn->name;
}

bool iio_channel_is_output(const struct iio_channel *chn)
{
	return chn->is_output;
}

bool iio_channel_is_scan_element(const struct iio_channel *chn)
{
	return chn->is_scan_element;
}

unsigned int iio_channel_get_attrs_count(const struct iio_channel *chn)
{
	return chn->nb_attrs;
}

const char * iio_channel_get_attr(const struct iio_channel *chn,
		unsigned int index)
{
	if (index >= chn->nb_attrs)
		return NULL;
	else
		return chn->attrs[index];
}

const char * iio_channel_find_attr(const struct iio_channel *chn,
		const char *name)
{
	unsigned int i;
	for (i = 0; i < chn->nb_attrs; i++) {
		const char *attr = chn->attrs[i];
		if (!strcmp(attr, name))
			return attr;
	}
	return NULL;
}

ssize_t iio_channel_attr_read(const struct iio_channel *chn,
		const char *attr, char *dst, size_t len)
{
	if (chn->dev->ctx->ops->read_channel_attr)
		return chn->dev->ctx->ops->read_channel_attr(chn,
				attr, dst, len);
	else
		return -ENOSYS;
}

ssize_t iio_channel_attr_write(const struct iio_channel *chn,
		const char *attr, const char *src)
{
	if (chn->dev->ctx->ops->write_channel_attr)
		return chn->dev->ctx->ops->write_channel_attr(chn, attr, src);
	else
		return -ENOSYS;
}

void iio_channel_set_data(struct iio_channel *chn, void *data)
{
	chn->userdata = data;
}

void * iio_channel_get_data(const struct iio_channel *chn)
{
	return chn->userdata;
}

long iio_channel_get_index(const struct iio_channel *chn)
{
	return chn->index;
}

const struct iio_data_format * iio_channel_get_data_format(
		const struct iio_channel *chn)
{
	return &chn->format;
}

bool iio_channel_is_enabled(const struct iio_channel *chn)
{
	return chn->index >= 0 && chn->dev->mask &&
		TEST_BIT(chn->dev->mask, chn->index);
}

void iio_channel_enable(struct iio_channel *chn)
{
	if (chn->index >= 0 && chn->dev->mask)
		SET_BIT(chn->dev->mask, chn->index);
}

void iio_channel_disable(struct iio_channel *chn)
{
	if (chn->index >= 0 && chn->dev->mask)
		CLEAR_BIT(chn->dev->mask, chn->index);
}

void free_channel(struct iio_channel *chn)
{
	unsigned int i;
	for (i = 0; i < chn->nb_attrs; i++)
		free((char *) chn->attrs[i]);
	if (chn->nb_attrs)
		free(chn->attrs);
	if (chn->name)
		free((char *) chn->name);
	if (chn->id)
		free((char *) chn->id);
	free(chn);
}

static void byte_swap(uint8_t *dst, const uint8_t *src, size_t len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
		dst[i] = src[len - i - 1];
}

static void shift_bits(uint8_t *dst, size_t shift, size_t len, bool left)
{
	unsigned int i;
	size_t shift_bytes = shift / 8;
	shift %= 8;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	if (!left)
#else
	if (left)
#endif
	{
		if (shift_bytes) {
			memmove(dst, dst + shift_bytes, len - shift_bytes);
			memset(dst + len - shift_bytes, 0, shift_bytes);
		}
		if (shift) {
			for (i = 0; i < len; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				dst[i] >>= shift;
				if (i < len - 1)
					dst[i] |= dst[i + 1] << (8 - shift);
#else
				dst[i] <<= shift;
				if (i < len - 1)
					dst[i] |= dst[i + 1] >> (8 - shift);
#endif
			}
		}
	} else {
		if (shift_bytes) {
			memmove(dst + shift_bytes, dst, len - shift_bytes);
			memset(dst, 0, shift_bytes);
		}
		if (shift) {
			for (i = len; i > 0; i--) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				dst[i - 1] <<= shift;
				if (i > 1)
					dst[i - 1] |= dst[i - 2] >> (8 - shift);
#else
				dst[i - 1] >>= shift;
				if (i > 1)
					dst[i - 1] |= dst[i - 2] << (8 - shift);
#endif
			}
		}
	}
}

static void sign_extend(uint8_t *dst, size_t bits, size_t len)
{
	size_t upper_bytes = ((len * 8 - bits) / 8);
	uint8_t msb, msb_bit = 1 << ((bits - 1) % 8);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	msb = dst[len - 1 - upper_bytes] & msb_bit;
	if (upper_bytes)
		memset(dst + len - upper_bytes, msb ? 0xff : 0x00, upper_bytes);
	if (msb)
		dst[len - 1 - upper_bytes] |= ~(msb_bit - 1);
#else
	/* XXX: untested */
	msb = dst[upper_bytes] & msb_bit;
	if (upper_bytes)
		memset(dst, msb ? 0xff : 0x00, upper_bytes);
	if (msb)
		dst[upper_bytes] |= ~(msb_bit - 1);
#endif
}

void iio_channel_convert(const struct iio_channel *chn,
		void *dst, const void *src)
{
	unsigned int len = chn->format.length / 8;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	bool swap = chn->format.is_be;
#else
	bool swap = !chn->format.is_be;
#endif

	if (len == 1 || !swap)
		memcpy(dst, src, len);
	else
		byte_swap(dst, src, len);

	if (chn->format.shift)
		shift_bits(dst, chn->format.shift, len, false);
	if (chn->format.is_signed)
		sign_extend(dst, chn->format.bits, len);
}

void iio_channel_convert_inverse(const struct iio_channel *chn,
		void *dst, const void *src)
{
	unsigned int len = chn->format.length / 8;
	unsigned int bits = chn->format.bits;
	unsigned int i;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	bool swap = chn->format.is_be;
#else
	bool swap = !chn->format.is_be;
#endif
	uint8_t buf[1024];

	/* Somehow I doubt we will have samples of 8192 bits each. */
	if (len > sizeof(buf))
		return;

	memcpy(buf, src, len);

	/* Clear upper bits */
	if (bits % 8)
		buf[bits / 8] &= (1 << (bits % 8)) - 1;

	/* Clear upper bytes */
	for (i = (bits + 7) / 8; i < len; i++)
		buf[i] = 0;

	if (chn->format.shift)
		shift_bits(buf, chn->format.shift, len, true);

	if (len == 1 || !swap)
		memcpy(dst, buf, len);
	else
		byte_swap(dst, buf, len);
}

size_t iio_channel_read_raw(const struct iio_channel *chn,
		struct iio_buffer *buf, void *dst, size_t len)
{
	uintptr_t src_ptr, dst_ptr = (uintptr_t) dst, end = dst_ptr + len;
	unsigned int length = chn->format.length / 8;

	for (src_ptr = (uintptr_t) iio_buffer_first(buf, chn);
			src_ptr < (uintptr_t) iio_buffer_end(buf) &&
			dst_ptr + length <= end;
			src_ptr += iio_buffer_step(buf),
			dst_ptr += length)
		memcpy((void *) dst_ptr, (const void *) src_ptr, length);
	return dst_ptr - (uintptr_t) dst;
}

size_t iio_channel_read(const struct iio_channel *chn,
		struct iio_buffer *buf, void *dst, size_t len)
{
	uintptr_t src_ptr, dst_ptr = (uintptr_t) dst, end = dst_ptr + len;
	unsigned int length = chn->format.length / 8;

	for (src_ptr = (uintptr_t) iio_buffer_first(buf, chn);
			src_ptr < (uintptr_t) iio_buffer_end(buf) &&
			dst_ptr + length <= end;
			src_ptr += iio_buffer_step(buf),
			dst_ptr += length)
		iio_channel_convert(chn,
				(void *) dst_ptr, (const void *) src_ptr);
	return dst_ptr - (uintptr_t) dst;
}

size_t iio_channel_write_raw(const struct iio_channel *chn,
		struct iio_buffer *buf, const void *src, size_t len)
{
	uintptr_t dst_ptr, src_ptr = (uintptr_t) src, end = src_ptr + len;
	unsigned int length = chn->format.length / 8;

	for (dst_ptr = (uintptr_t) iio_buffer_first(buf, chn);
			dst_ptr < (uintptr_t) iio_buffer_end(buf) &&
			src_ptr + length <= end;
			dst_ptr += iio_buffer_step(buf),
			src_ptr += length)
		memcpy((void *) dst_ptr, (const void *) src_ptr, length);
	return src_ptr - (uintptr_t) src;
}

size_t iio_channel_write(const struct iio_channel *chn,
		struct iio_buffer *buf, const void *src, size_t len)
{
	uintptr_t dst_ptr, src_ptr = (uintptr_t) src, end = src_ptr + len;
	unsigned int length = chn->format.length / 8;

	for (dst_ptr = (uintptr_t) iio_buffer_first(buf, chn);
			dst_ptr < (uintptr_t) iio_buffer_end(buf) &&
			src_ptr + length <= end;
			dst_ptr += iio_buffer_step(buf),
			src_ptr += length)
		iio_channel_convert_inverse(chn,
				(void *) dst_ptr, (const void *) src_ptr);
	return src_ptr - (uintptr_t) src;
}

int iio_channel_attr_read_longlong(const struct iio_channel *chn,
		const char *attr, long long *val)
{
	char *end, buf[1024];
	long long value;
	ssize_t ret = iio_channel_attr_read(chn, attr, buf, sizeof(buf));
	if (ret < 0)
		return (int) ret;

	value = strtoll(buf, &end, 0);
	if (end == buf)
		return -EINVAL;
	*val = value;
	return 0;
}

int iio_channel_attr_read_bool(const struct iio_channel *chn,
		const char *attr, bool *val)
{
	long long value;
	int ret = iio_channel_attr_read_longlong(chn, attr, &value);
	if (ret < 0)
		return ret;

	*val = !!value;
	return 0;
}

int iio_channel_attr_read_double(const struct iio_channel *chn,
		const char *attr, double *val)
{
	char *end, buf[1024];
	double value;
	ssize_t ret = iio_channel_attr_read(chn, attr, buf, sizeof(buf));
	if (ret < 0)
		return (int) ret;

	value = strtod(buf, &end);
	if (end == buf)
		return -EINVAL;
	*val = value;
	return 0;
}

int iio_channel_attr_write_longlong(const struct iio_channel *chn,
		const char *attr, long long val)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%lld", val);
	return iio_channel_attr_write(chn, attr, buf);
}

int iio_channel_attr_write_double(const struct iio_channel *chn,
		const char *attr, double val)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%lf", val);
	return iio_channel_attr_write(chn, attr, buf);
}

int iio_channel_attr_write_bool(const struct iio_channel *chn,
		const char *attr, bool val)
{
	if (val)
		return iio_channel_attr_write(chn, attr, "1");
	else
		return iio_channel_attr_write(chn, attr, "0");
}