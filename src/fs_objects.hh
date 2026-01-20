#ifndef FS_OBJECTS_HH
#define FS_OBJECTS_HH
/**
 * Contains type definitions of objects used in the SimFS filesystem.
 */

#include <stdint.h>

namespace ffsys {


static constexpr int N_STATIC_FILE_BLOCKS = 15;
static constexpr int N_DYNAMIC_FILE_BLOCKS = 5;
/**
 * I-nodes are essentially tables that hold
 * metadata for each file.
 */
struct INode {
    // The ordinal number of this inode.
    uint32_t index;

    // The name of the file in ascii.
    char name[17];

    // The size of the file in bytes. Important for
    // determining, and keeping track of, EOF.
    uint64_t size;

    // The addresses of this file's data blocks.
    // 15 first are the direct addresses (static) of data blocks,
    // and the last 5 are reserved for indirect (dynamic) block addresses
    // (i.e. for address of a block that contains more of this file's data
    // block addresses), if the static ones are not enough.
    // The value -1 is used to indicate unreserved blocks.
    int32_t blocks[N_STATIC_FILE_BLOCKS + N_DYNAMIC_FILE_BLOCKS]
        = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

    // Datetime the file was created.
    uint64_t created_time;
};
static constexpr unsigned int INODE_SIZE = sizeof(INode);

/**
 * The superblock contains basic information about the filesystem,
 * mostly in terms of "pointers" to (i.e. the indices of) different objects
 * or blocks.
 */
struct Superblock {
    // The size of one block in bytes.
    uint16_t block_size;

    // The number of i-nodes. Has to be less than or equal to block_size * 8.
    uint16_t n_inodes;

    // The amount of blocks reserved for i-nodes.
    uint16_t n_inode_blocks;

    // The number of file data blocks. Same size restriction as with i-nodes.
    uint16_t n_data_blocks;

    // The three is for the superblock, the i-node bitmap and the data block bitmap.
    uint16_t total_n_blocks() {
        return 3 + n_inode_blocks + n_data_blocks;
    }

    // The index of the block containing the 'free i-node' bitmap.
    uint16_t inode_bitmap_i;

    // The index of the block containing the 'free data block' bitmap.
    uint16_t data_block_bitmap_i;

    // The block index at which the inodes start.
    uint16_t inodes_start_i;

    // The block index at which blocks reserved for file data start.
    uint16_t data_blocks_start_i;

    uint16_t n_free_inodes;
    uint16_t n_free_data_blocks;

    // The amount of address pointers that fit into one address data block.
    uint32_t address_block_capacity;
};
static constexpr unsigned int SUPERBLOCK_SIZE = sizeof(Superblock);

}

#endif // FS_OBJECTS_HH
