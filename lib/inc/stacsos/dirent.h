#pragma once

constexpr size_t file_name_length = 100;

namespace stacsos {

// to match fs_node_type in the VFS, cant use it directly because its in the kernel
enum class file_type { file, directory }; 

// stores metadata of a directory entry
struct dirent {
    char name[file_name_length];
    int name_length = file_name_length; // so data can be safeley copied into name
    file_type type;
    u64 size;
};
}