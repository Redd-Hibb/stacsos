/* SPDX-License-Identifier: MIT */

/* StACSOS - Kernel
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#pragma once

#include <stacsos/dirent.h>
#include <stacsos/kernel/fs/fs-node.h>

namespace stacsos::kernel::fs {
class filesystem;
class file {
public:
	file(u64 size)
		: size_(size)
		, cur_offset_(0)
	{
	}

	virtual ~file() { }

	virtual u64 ioctl(u64 cmd, void *buffer, size_t length) { return 0; }

	virtual size_t pread(void *buffer, size_t offset, size_t length) = 0;
	virtual size_t pwrite(const void *buffer, size_t offset, size_t length) = 0;

	virtual size_t read(void *buffer, size_t length)
	{
		u64 read_length = length;
		if ((cur_offset_ + read_length) > size_) {
			read_length = size_ - cur_offset_;
		}

		size_t result = pread(buffer, cur_offset_, read_length);
		cur_offset_ += result;

		return result;
	}

	virtual size_t write(const void *buffer, size_t length)
	{
		u64 write_length = length;
		if ((cur_offset_ + write_length) > size_) {
			write_length = size_ - cur_offset_;
		}

		size_t result = pwrite(buffer, cur_offset_, write_length);
		cur_offset_ += result;

		return result;
	}

private:
	u64 size_;
	u64 cur_offset_;
};
// an abstract class part of the VFS, for a File System Driver to override.
// kernel calls methods from the VFS, not the FSD, so the FSD can easily be switched out.
class directory {
public:
	// leave how directory entries are stored up to the FSD
	directory()
		: cur_file_(0)
	{
	}

	// to be overriden with the functional implementation of readdir (listing files).
	virtual size_t readdir(void *buffer, size_t length) = 0;

protected:

	/**
	 * @brief helper function that makes convertig between directory entry type
	          between kernel defined types and those defined in stacsos/dirent.h
	 *
	 * @param kind the fs_node_kind enum to be converted
	 *
	 * @return file_type enum that dirent.h uses
	 */
	file_type fs_node_kind_to_file_type(fs_node_kind kind) {

		switch (kind) {
			case fs_node_kind::file:
				return file_type::file;
			case fs_node_kind::directory:
				return file_type::directory;
		}
	}


	// standardise a file index for storing current place in directory.
	u64 cur_file_;
}; // namespace stacsos::kernel::fs
}