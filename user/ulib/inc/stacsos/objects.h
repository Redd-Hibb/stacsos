/* SPDX-License-Identifier: MIT */

/* StACSOS - userspace standard library
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#pragma once

#include <stacsos/user-syscall.h>

namespace stacsos {
class object {
public:
	static object *open(const char *path);
	
	// static method used for object creation
	static object *opendir(const char *path);

	virtual ~object();

	// free the objects resources
	syscall_result_code close();

	size_t write(const void *buffer, size_t length);
	size_t pwrite(const void *buffer, size_t length, size_t offset);

	size_t read(void *buffer, size_t length);
	size_t pread(void *buffer, size_t length, size_t offset);

	// calls the readdir syscall
	size_t readdir(void *buffer, size_t length);

	

	u64 ioctl(u64 cmd, void *buffer, size_t length);

private:
	u64 handle_;

	object(u64 handle)
		: handle_(handle)
	{
	}
};
} // namespace stacsos
