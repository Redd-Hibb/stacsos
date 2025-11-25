/* SPDX-License-Identifier: MIT */

/* StACSOS - userspace standard library
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#include <stacsos/objects.h>
#include <stacsos/user-syscall.h>

using namespace stacsos;

object *object::open(const char *path)
{
	auto result = syscalls::open(path);
	if (result.code != syscall_result_code::ok) {
		return nullptr;
	}

	return new object(result.id);
}

/** @brief creates a directory object in user space 
 *  that will call methods of the matching object in kernel space.
 *  
 *  @param path complete path to directory to open.
 *  @returns directory object.
 */
object *object::opendir(const char *path)
{
	auto result = syscalls::opendir(path);
	if (result.code != syscall_result_code::ok) {
		return nullptr;
	}

	return new object(result.id);
}

object::~object() { syscalls::close(handle_); }

/**
 * @brief free the resources of this object in the Object Manager.
 *
 * @return syscall_result_code
 */
syscall_result_code object::close() { return syscalls::close(handle_); }

size_t object::read(void *buffer, size_t length) { return syscalls::read(handle_, buffer, length).length; }
size_t object::write(const void *buffer, size_t length) { return syscalls::write(handle_, buffer, length).length; }
size_t object::pwrite(const void *buffer, size_t length, size_t offset) { return syscalls::pwrite(handle_, buffer, length, offset).length; }
size_t object::pread(void *buffer, size_t length, size_t offset) { return syscalls::pread(handle_, buffer, length, offset).length; }

/** @brief read the file metadata of the next file in the directory.
 *  
 *  @param buffer will be written to with dirent struct (stacsos/dirent.h)
 *  @param length length of buffer
 *  @return number of dirents successfully written to buffer (0 indicates error or no files left)
 */
size_t object::readdir(void *buffer, size_t length) { return syscalls::readdir(handle_, buffer, length).length; }

u64 object::ioctl(u64 cmd, void *buffer, size_t length) { return syscalls::ioctl(handle_, cmd, buffer, length).length; }
