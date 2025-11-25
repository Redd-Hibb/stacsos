
#include <stacsos/objects.h>
#include <stacsos/console.h>
#include <stacsos/string.h>
#include <stacsos/printf.h>
#include <stacsos/dirent.h>
#include <stacsos/user-syscall.h>

using namespace stacsos;

size_t longest_name(list<dirent> files) {
    size_t max = 0;
    for (auto& file : files) {
        size_t length = memops::strlen(file.name);
        if (length > max) {
            max = length;
        }
    }
    return max;
}

int main(const char *cmdline)
{
    const int dirents_in_buffer = 10;

    // ============= Check for flags ========================
    if (!cmdline || memops::strlen(cmdline) == 0) {
		console::get().write("error: usage: ls [-f] <filename>\n");
		return 1;
	}

	bool formatting_mode = false;
	while (*cmdline) {
		if (*cmdline == '-') {
			cmdline++;

			if (*cmdline++ == 'f') {
				formatting_mode = true;
			} else {
				console::get().write("error: usage: cat [-f] <filename>\n");
				return 1;
			}
		} else {
			break;
		}
	}

	while (*cmdline == ' ') {
		cmdline++;
	};
    // ============ Checking for flags finished ================

    //create an object to manage syscalls for user
	object *directory = object::opendir(cmdline);
    
    // if syscall failed, print error
    if (!directory) {
        console::get().writef("error: unable to open file %s for listing\n", cmdline);
    }

    // separate files and directories
    list<dirent> directories;
    list<dirent> files;

    // create a buffer for readdir to write to
    dirent buffer[dirents_in_buffer];
    int n_entries = dirents_in_buffer;

    // read as many entries that will fit the buffer until no entries are left
    while (n_entries == dirents_in_buffer && n_entries > 0) {

        // read entries
        n_entries = directory->readdir(buffer, sizeof(buffer));

        // do for each dirent copied into the buffer
        for (int i=0; i < n_entries; i++) {

            dirent entry = buffer[i];
            
            // ignore hidden files
            if (entry.name[0] != '.') {
                
                if (entry.type == file_type::file) {
                    files.append(entry);
                }
                else {
                    directories.append(entry);
                }
            }
        }
    }

    // no longer need to use directory so free its resources
    directory->close();

    // print out listing of directories and files given the -f flag was given
    // in the format (<filetype>) <filename> <filesize>
    if (formatting_mode) {

        // directories
        for (auto& dir : directories) {
            console::get().writef("(D) %s\n", dir.name);
        }

        size_t longest = longest_name(files);

        // files
        for (auto& file : files) {

            console::get().writef("(F) %s  ", file.name);

            // print spaces to align filesizes into a table format
            size_t length = memops::strlen(file.name);
            for (size_t i=0; i<(longest - length); i++) {
                console::get().write(" ");
            }

            // print filesize
            console::get().writef("%llu\n", file.size);
        }
    }

    // print listing of files and directories given no -f was provided
    // in the format <filename>
    else {

        for (auto& dir : directories) {
            console::get().writef("%s\n", dir.name);
        }
        for (auto& file : files) {
            console::get().writef("%s\n", file.name);
        }
    }

    return 0;
}

