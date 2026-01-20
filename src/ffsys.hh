#ifndef FFSYS_H
#define FFSYS_H

#include "fs_objects.hh"
#include "bitmap.hh"

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <memory>

// FFSys = FileFileSystem
namespace ffsys {

enum class ErrorNumber {
    NO_ERROR,
    PATH_NOT_FOUND,
    NO_SUCH_FILE_DESCRIPTOR,
    CANT_READ_INODE,
    NO_FREE_INODES,
    NO_FREE_DATA_BLOCKS,
    FILE_ALREADY_EXISTS,
    NO_SUCH_FILE,
    FILE_ALREADY_OPEN
};

/**
 * File descriptors are just integer id's for open files, with the value -1
 * representing a missing id/error when used as a return value.
 */
using file_descriptor = int;

/**
 * Describes the details of an open file in our simulated filesystem.
 */
struct OpenFile {
    // The file's unique id.
    file_descriptor fd;

    // The i-node number of the file.
    unsigned int inode;

    // Current byte position in the file (from start of file).
    unsigned int pos;

    OpenFile(file_descriptor fd_p, unsigned int inode_p, unsigned int pos_p):
        fd(fd_p), inode(inode_p), pos(pos_p) {}
};

/**
 * Bitflags for specifying policy for opening FFSys files.
 */
enum OpenFlags {
    // Clear file contents when opened, otherwise keep them.
    TRUNCATE = 0x01,

    // Create the file if it doesn't exist, otherwise only open existing files.
    CREATE = 0x02,

    // Set file position to file's end after opening, otherwise at 0.
    END = 0x04,
};

/**
 * The objects of the FFSys (File FileSystem) class provide an interface
 * for creating, reading and writing files into a filesystem that lives in
 * a single actual file. The filesystem is based mainly on the EXT2 filesystem.
 * An object of this class represents a single FFileSystem.
 */
class FFSys
{
public:

    /**
     * Creates and mounts a new FFSys file.
     * @param path The path to create the file at.
     * @param block_size Specifies the block_size to use in the file.
     * @throws std::string, if the file could not be created.
     */
    FFSys(std::string path, unsigned long block_size);

    /**
     * Mounts the given FFSys file.
     * @param path The path of the file.
     * @throws std::string, if the file could not be opened.
     */
    FFSys(std::string path);

    ~FFSys();

    /**
     * Tries to open the file with the given name. Flags are used to specify
     * how to open the file (see enum OpenFlags).
     */
    file_descriptor open(std::string filename, int flags = 0);

    /**
     * Tries to read count number of bytes from the file referred to
     * by the given file descriptor into the buffer. Returns the amount
     * of bytes read, or -1 if an error occurred, in which case
     * errno is also set to indicate the reason for error.
     */
    ssize_t read(file_descriptor fd, char* buffer, size_t count);

    /**
     * Writes count number of bytes from the buffer into the file
     * corresponding to the given file descriptor. Returns the amount
     * of bytes written, or -1 if an error occurred, in which case
     * errno is also set to indicate reason for error.
     */
    ssize_t write(file_descriptor fd, char* buffer, size_t count);

    /**
     * Closes the file corresponding to the given file descriptor.
     * Returns true on success and false if an error occurred, in
     * which case errno is also set to indicate the reason for
     * the error.
     */
    bool close(file_descriptor fd);

    /**
     * Sets the file position of the corresponding file. Returns false
     * if the position is larger than file size or the file descriptor
     * is unknown.
     */
    bool seek(file_descriptor fd, size_t pos);

    /**
     * Returns the current error code.
     */
    ErrorNumber errnum();

    // Printing functions to aid in testing
    void print_superblock();
    void print_all_files();
    void print_open_files();

private:
    // File stream into an FFSys-file.
    std::fstream fs_;
    // Superblock of the FFSys-file as a struct. Contains metadata about the FS.
    Superblock sb_ = {};

    ErrorNumber errnum_ = ErrorNumber::NO_ERROR;

    // Map of open files: file descriptor, file's inode and current file position.
    std::map<file_descriptor, std::shared_ptr<OpenFile>> open_files_ = {};

    // Helper bitmaps
    Bitmap* inode_bitmap_ = nullptr;
    Bitmap* data_block_bitmap_ = nullptr;

    // Helper buffer for initializing new address blocks (filled with -1).
    int32_t* empty_address_block_buffer = nullptr;

    // Reads count n bytes from the i:th block of the file,
    // into the given buffer, starting from n bytes offset into the block.
    bool read_block(unsigned int block_i, char* block_buffer, size_t count, size_t offset = 0);

    // Reads the whole i:th block of the file into the given buffer.
    bool read_block(unsigned int block_i, char* block_buffer);

    // Writes i:th block.
    void write_block(unsigned int block_i, char* block_buffer, size_t count, size_t offset = 0);
    void write_block(unsigned int block_i, char* block_buffer);

    // Reading and writing i-nodes.
    bool read_inode(int inode_i, INode& result);
    void write_inode(INode& inode);

    // Reading and writing Superblock.
    bool read_superblock(Superblock& result);
    void write_superblock();

    // Tries to create a file of the given name (reserves + initializes i-node)
    bool create_file(std::string name, INode& result);

    // Tries to find a file with the given name.
    bool find_file(std::string name, INode& result);

    // Reading and writing files. Internal helpers for read() and
    // write() respectively.
    size_t read_n_bytes_from_file(INode const& file, char* buffer, size_t count, size_t pos = 0);
    size_t write_n_bytes_to_file(INode& file, char* buffer, size_t count, size_t pos = 0);

    // Helpers that reserve/free bits from the corresponding bitmaps, and
    // update the changes to the FFSys file.
    int reserve_inode();
    int reserve_data_block();
    bool free_data_block(int i);

    // Helpers for reserving/freeing file blocks,
    int reserve_file_block(INode& inode, unsigned int i);
    bool free_file_block(INode& inode, unsigned int i);
    void free_unused_file_blocks(INode& inode);

    // Reserves a data block for use as an address block (block filled
    // with addresses of other data blocks), and fills it up with null
    // addresses (-1).
    int initialize_address_block();

    // Low-level helpers for setting/getting a file block address.
    bool set_file_block_address(INode& inode, unsigned int i, int32_t new_value);
    int get_file_block_address(INode const& inode, unsigned int i);

    // Helper print function
    void print_inode(ffsys::INode& inode);

    // CONSTANTS
    static constexpr char NULL_CHAR = '\0';

    // Superblock is always the first block.
    static constexpr unsigned long SUPERBLOCK_I = 0;
};

} // namespace simfs

#endif // FFSYS_H
