#include "ffsys.hh"
#include "utilities.hh"

#include <iostream>
#include <iomanip>
#include <ctime>

using namespace std;

namespace ffsys {

FFSys::FFSys(string path, unsigned long block_size):
    fs_(path,
        std::ios_base::binary
      | std::ios_base::in
      | std::ios_base::out
      | std::ios_base::trunc
    )
{

    if (block_size < SUPERBLOCK_SIZE) {
        throw std::string("Error: block size is too small");
    }

    if (!fs_) {
        throw std::string("Error opening file");
    }

    // Superblock with default values, calculated based on block size.
    sb_.block_size = block_size;
    sb_.n_data_blocks = 8 * block_size;
    sb_.n_inodes = 8 * block_size;
    sb_.n_inode_blocks = (sb_.n_inodes * INODE_SIZE + block_size - 1) / block_size;

    sb_.inode_bitmap_i = 1;
    sb_.data_block_bitmap_i = 2;
    sb_.inodes_start_i = 3;
    sb_.data_blocks_start_i = sb_.inodes_start_i + sb_.n_inode_blocks;

    sb_.n_free_inodes = sb_.n_inodes;
    sb_.n_free_data_blocks = sb_.n_data_blocks;

    sb_.address_block_capacity = block_size / sizeof(int32_t);

    // Init file as all zero bytes.
    for (int i = 0; i < sb_.total_n_blocks() * block_size; ++i) {
        fs_.put(NULL_CHAR);
    }

    // Write superblock
    write_superblock();

    // Allocate bitmap helper buffers
    inode_bitmap_ = new Bitmap(block_size);
    data_block_bitmap_ = new Bitmap(block_size);

    // Helper buffer for initializing address blocks.
    empty_address_block_buffer = new int32_t[sb_.address_block_capacity];
    for (int i = 0; i < sb_.address_block_capacity; ++i) {
        empty_address_block_buffer[i] = -1;
    }

    // Init the bitmaps.
    write_block(sb_.inode_bitmap_i, inode_bitmap_->get_bm());
    write_block(sb_.data_block_bitmap_i, data_block_bitmap_->get_bm());
}

FFSys::FFSys(string path):
    fs_(path,
        std::ios_base::binary
      | std::ios_base::in
      | std::ios_base::out
    )
{
    if (!fs_) {
        throw std::string("Error opening file");
    }

    // Read superblock
    if (!read_superblock(sb_)) {
        throw "Error reading superblock, corrupted.";
    }

    // Read bitmaps
    inode_bitmap_ = new Bitmap(sb_.block_size);
    read_block(sb_.inode_bitmap_i, inode_bitmap_->get_bm());

    data_block_bitmap_ = new Bitmap(sb_.block_size);
    read_block(sb_.data_block_bitmap_i, data_block_bitmap_->get_bm());

    // Helper buffer for initializing address blocks.
    empty_address_block_buffer = new int32_t[sb_.address_block_capacity];
    for (int i = 0; i < sb_.address_block_capacity; ++i) {
        empty_address_block_buffer[i] = -1;
    }
}

FFSys::~FFSys()
{
    if (fs_) {
        fs_.close();
        cout << "FS file closed." << endl;
    }

    if (inode_bitmap_ != nullptr) {
        delete inode_bitmap_;
    }

    if (data_block_bitmap_ != nullptr) {
        delete data_block_bitmap_;
    }

    if (empty_address_block_buffer != nullptr) {
        delete[] empty_address_block_buffer;
    }
}

file_descriptor FFSys::open(std::string name, int flags)
{
    INode file = {};

    // Try to find file from FS by name.
    if (!find_file(name, file)) {

        // If file wasn't found and the create flag was not specified, return
        if (not (flags & OpenFlags::CREATE)) {
            errnum_ = ErrorNumber::NO_SUCH_FILE;
            return -1;
        }

        // Try to create the file.
        if (!create_file(name, file)) {
            return -1;
        }
    }

    // Clear the file contents if TRUNCATE is wanted
    if (flags & OpenFlags::TRUNCATE) {
        file.size = 0;
        free_unused_file_blocks(file);
    }

    // Generate new file descriptor.
    file_descriptor fd = 0;
    while (open_files_.find(fd) != open_files_.end()) {
        ++fd;
    }

    size_t file_pos = flags & OpenFlags::END ? file.size : 0;
    auto file_info = make_shared<OpenFile>(fd, file.index, file_pos);
    open_files_.insert({file_info->fd, file_info});

    return file_info->fd;
}

ssize_t FFSys::read(file_descriptor fd, char* buf, size_t count)
{
    if (open_files_.find(fd) == open_files_.end()) {
        errnum_ = ErrorNumber::NO_SUCH_FILE_DESCRIPTOR;
        return -1;
    }

    auto file = open_files_.at(fd);

    INode inode = {};
    if (!read_inode(file->inode, inode)) {
        errnum_ = ErrorNumber::CANT_READ_INODE;
        return -1;
    }

    size_t read = read_n_bytes_from_file(inode, buf, count, file->pos);
    file->pos += read;

    return read;
}

ssize_t FFSys::write(file_descriptor fd, char* buffer, size_t count)
{
    if (open_files_.find(fd) == open_files_.end()) {
        errnum_ = ErrorNumber::NO_SUCH_FILE_DESCRIPTOR;
        return -1;
    }

    auto file = open_files_.at(fd);

    INode inode = {};
    if (!read_inode(file->inode, inode)) {
        errnum_ = ErrorNumber::CANT_READ_INODE;
        return -1;
    }

    size_t written = write_n_bytes_to_file(inode, buffer, count, file->pos);
    file->pos += written;

    return written;
}

bool FFSys::close(file_descriptor fd)
{
    auto file_iter = open_files_.find(fd);
    if (file_iter == open_files_.end()) {
        errnum_ = ErrorNumber::NO_SUCH_FILE_DESCRIPTOR;
        return false;
    }

    open_files_.erase(file_iter);

    return true;
}

bool FFSys::seek(file_descriptor fd, size_t pos)
{
    if (open_files_.find(fd) == open_files_.end()) {
        errnum_ = ErrorNumber::NO_SUCH_FILE_DESCRIPTOR;
        return false;
    }

    auto file = open_files_.at(fd);

    INode inode = {};
    if (!read_inode(file->inode, inode)) {
        errnum_ = ErrorNumber::CANT_READ_INODE;
        return false;
    }

    if (inode.size < pos) {
        return false;
    }

    file->pos = pos;
    return true;
}

ErrorNumber FFSys::errnum()
{
    return errnum_;
}


bool FFSys::read_block(unsigned int block_i, char* block_buf, size_t count, size_t offset)
{
    fs_.seekg(block_i * sb_.block_size + offset);
    fs_.read(block_buf, count);
    return true;
}

bool FFSys::read_block(unsigned int block_i, char *block_buffer)
{
    return read_block(block_i, block_buffer, sb_.block_size);
}

void FFSys::write_block(unsigned int block_i, char* block_buffer, size_t count, size_t offset)
{
    fs_.seekp(block_i * sb_.block_size + offset);
    fs_.write(block_buffer, count);
}

void FFSys::write_block(unsigned int block_i, char *block_buffer)
{
    write_block(block_i, block_buffer, sb_.block_size);
}

bool FFSys::read_inode(int inode_i, INode& result)
{
    // Read bytes
    unsigned int inode_start = sb_.inodes_start_i * sb_.block_size + inode_i * INODE_SIZE;
    fs_.seekg(inode_start);
    char buf[INODE_SIZE];
    fs_.read(buf, INODE_SIZE);

    // Cast the byte array to an INode.
    result = bit_cast<INode>(buf);
    return true;
}

void FFSys::write_inode(INode& inode)
{
    unsigned int inode_start = sb_.inodes_start_i * sb_.block_size + inode.index * INODE_SIZE;
    fs_.seekp(inode_start);
    fs_.write(reinterpret_cast<char*>(&inode), INODE_SIZE);
}

bool FFSys::read_superblock(Superblock &result)
{
    char buf[SUPERBLOCK_SIZE];

    if (!read_block(SUPERBLOCK_I, buf, SUPERBLOCK_SIZE)) {
        return false;
    }

    result = bit_cast<Superblock>(buf);
    return true;
}

void FFSys::write_superblock()
{
    write_block(SUPERBLOCK_I, reinterpret_cast<char*>(&sb_), SUPERBLOCK_SIZE);
}

bool FFSys::create_file(string name, INode &result)
{
    int inode_i = reserve_inode();
    if (inode_i == -1) {
        errnum_ = ErrorNumber::NO_FREE_INODES;
        return false;
    }

    INode inode;
    inode.index = inode_i;
    inode.size = 0;
    inode.created_time = time(nullptr);

    int i = 0;
    while (i < min(sizeof(INode::name)-1, name.size())) {
        inode.name[i] = name.at(i);
        ++i;
    }
    inode.name[i] = '\0';

    int initial_data_block = reserve_data_block();
    if (initial_data_block != -1) {
        inode.blocks[0] = initial_data_block;
    }

    // Write inode to disk
    write_inode(inode);

    result = inode;
    return true;
}

bool FFSys::find_file(std::string name, INode& result)
{
    INode inode;
    unsigned int inodes_checked = 0;
    for (unsigned int i = 0; i < sb_.n_inodes; ++i) {
        // If we have already checked all used inodes.
        if (inodes_checked >= (sb_.n_inodes - sb_.n_free_inodes)) {
            break;
        }

        if (!inode_bitmap_->is_free(i) and read_inode(i, inode)) {
            if (inode.name == name) {
                result = inode;
                return true;
            }
            ++inodes_checked;
        }
    }
    return false;
}

size_t FFSys::read_n_bytes_from_file(INode const& file, char* buffer, size_t count, size_t pos)
{
    if (pos + count >= file.size) {
        count = file.size - pos;
    }

    size_t read_count = 0;
    unsigned int block_index = pos / sb_.block_size;

    // Read to the end of the current block, if we are starting from the middle.
    size_t leftover = pos % sb_.block_size;
    if (leftover != 0) {
        int block_address = sb_.data_blocks_start_i + get_file_block_address(file, block_index);
        size_t pos = (block_address) * sb_.block_size + leftover;
        fs_.seekg(pos);

        size_t to_read = min(sb_.block_size - leftover, count);
        fs_.read(buffer, to_read);
        read_count += to_read;
        ++block_index;
    }

    // Read the rest as full blocks until finished.
    while (read_count < count) {
        int block_address = get_file_block_address(file, block_index);

        if (block_address == -1) {
            break;
        }

        size_t to_read = min((size_t)sb_.block_size, count - read_count);
        read_block(sb_.data_blocks_start_i + block_address, buffer + read_count, to_read);
        read_count += to_read;

        ++block_index;
    }

    return read_count;
}

size_t FFSys::write_n_bytes_to_file(INode &file, char *buffer, size_t count, size_t pos)
{
    size_t left_to_write = count;
    size_t written = 0;

    // The index of the block we start from (not the index of the actual
    // file data block, but the index into the i-node's blocks, which gives
    // the actual one).
    unsigned int current_file_block_i = pos / sb_.block_size;

    while (left_to_write > 0) {
        // Get the address of the current data block.
        int current_block_address = get_file_block_address(file, current_file_block_i);

        // If file space has ran out, reserve a new block.
        if (current_block_address == -1) {
            current_block_address = reserve_file_block(file, current_file_block_i);

            // No more free data blocks.
            if (current_block_address == -1) {
                break;
            }
        }

        // Always write to the next block (or just whatever's left, if it is less).
        size_t offset = pos % sb_.block_size;
        size_t to_write = min(sb_.block_size - offset, left_to_write);
        size_t block_i = sb_.data_blocks_start_i + current_block_address;
        write_block(block_i, buffer + written, to_write, offset);

        written += to_write;
        left_to_write -= to_write;
        pos += to_write;

        ++current_file_block_i;
    }

    file.size = max((uint64_t)pos, file.size);
    write_inode(file);

    return written;
}

int FFSys::reserve_inode()
{
    int reserved_i = inode_bitmap_->reserve_first_free();
    if (reserved_i == -1) {
        return -1;
    }

    // Write to disk
    unsigned int bm_pos = sb_.inode_bitmap_i * sb_.block_size;
    unsigned int byte_pos = reserved_i / 8;
    fs_.seekp(bm_pos + byte_pos);
    fs_.write(inode_bitmap_->get_bm(byte_pos), 1);

    sb_.n_free_inodes -= 1;
    write_superblock();

    return reserved_i;
}

int FFSys::reserve_data_block()
{
    int reserved_i = data_block_bitmap_->reserve_first_free();
    if (reserved_i == -1) {
        errnum_ = ErrorNumber::NO_FREE_DATA_BLOCKS;
        return -1;
    }

    // Write to disk
    unsigned int bm_pos = sb_.data_block_bitmap_i * sb_.block_size;
    unsigned int byte_pos = reserved_i / 8;
    fs_.seekp(bm_pos + byte_pos);
    fs_.write(data_block_bitmap_->get_bm(byte_pos), 1);

    sb_.n_free_data_blocks -= 1;
    write_superblock();

    return reserved_i;
}

bool FFSys::free_data_block(int i)
{
    if (!data_block_bitmap_->free(i)) {
        return false;
    }

    // Write to disk
    unsigned int bm_pos = sb_.data_block_bitmap_i * sb_.block_size;
    unsigned int byte_pos = i / 8;
    fs_.seekp(bm_pos + byte_pos);
    fs_.write(data_block_bitmap_->get_bm(byte_pos), 1);

    sb_.n_free_data_blocks += 1;
    write_superblock();

    return true;
}

/**
 * Reserves a data block for the i:th block of file.
 */
int FFSys::reserve_file_block(INode &inode, unsigned int i)
{
    int reserved_i = reserve_data_block();
    if (reserved_i == -1) {
        return -1;
    }

    // Write the new address to the address block.
    if (set_file_block_address(inode, i, (int32_t)reserved_i)) {
        return reserved_i;
    } else {
        return -1;
    }
}

bool FFSys::free_file_block(INode &inode, unsigned int i)
{
    int block = get_file_block_address(inode, i);
    if (block == -1) {
        return false;
    }

    free_data_block(block);
    set_file_block_address(inode, i, -1);

    return true;
}

void FFSys::free_unused_file_blocks(INode &inode)
{
    int last_block = (inode.size + sb_.block_size - 1) / sb_.block_size;

    // Always keep at least one block reserved, even when the file size is 0.
    if (last_block == 0) {
        last_block = 1;
    }

    // Free up each unused file block.
    for (int i = last_block; i < N_STATIC_FILE_BLOCKS + N_DYNAMIC_FILE_BLOCKS * sb_.address_block_capacity; ++i) {
        if (get_file_block_address(inode, i) == -1) {
            break;
        }

        free_file_block(inode, i);
    }

    // If any of the address blocks is no longer needed, free them.
    // At this point, it should contain only -1:s anyway.
    for (int i = 0; i < N_DYNAMIC_FILE_BLOCKS; ++i)
    {
        if (inode.blocks[N_STATIC_FILE_BLOCKS + i] != -1 &&
            last_block < N_STATIC_FILE_BLOCKS + i * sb_.address_block_capacity)
        {
            free_data_block(inode.blocks[N_STATIC_FILE_BLOCKS + i]);
            inode.blocks[N_STATIC_FILE_BLOCKS + i] = -1;
        }
    }

    // Write to disk
    write_block(sb_.data_block_bitmap_i, data_block_bitmap_->get_bm());
    write_superblock();
    write_inode(inode);
}

int FFSys::initialize_address_block()
{
    int reserved_i = reserve_data_block();
    if (reserved_i == -1) {
        return -1;
    }

    // Initialize the newly reserved block as empty.
    write_block(reserved_i + sb_.data_blocks_start_i, (char*)empty_address_block_buffer, sb_.address_block_capacity*sizeof(int32_t));
    return reserved_i;
}

bool FFSys::set_file_block_address(INode &inode, unsigned int i, int32_t new_value)
{
    // If the wanted block is a static one, it can be set
    // directly to the i-node.
    if (i < N_STATIC_FILE_BLOCKS) {
        inode.blocks[i] = new_value;
        write_inode(inode);
        return true;
    }

    int dyn_block_i = (i - N_STATIC_FILE_BLOCKS) / sb_.address_block_capacity + N_STATIC_FILE_BLOCKS;
    if (dyn_block_i >= N_STATIC_FILE_BLOCKS + N_DYNAMIC_FILE_BLOCKS) {
        return false;
    }

    // If the address block has not been reserved yet, try to reserve it
    if (inode.blocks[dyn_block_i] == -1) {
        inode.blocks[dyn_block_i] = initialize_address_block();

        // Reserving failed
        if (inode.blocks[dyn_block_i] == -1) {
            return false;
        }

        write_inode(inode);
    }

    // The local index of the address inside the dynamic address block
    int i_in_dyn_block = (i - N_STATIC_FILE_BLOCKS) % sb_.address_block_capacity;

    // Write the new address to the address block.
    unsigned int address_block = sb_.data_blocks_start_i + inode.blocks[dyn_block_i];
    write_block(address_block, reinterpret_cast<char*>(&new_value), sizeof(int32_t), i_in_dyn_block * sizeof(int32_t));

    return true;
}

/**
 * Gets the address of the i:th data block of the given inode
 */
int FFSys::get_file_block_address(INode const& inode, unsigned int i)
{
    // If the wanted block is a static one, it can be retrieved
    // directly from the i-node.
    if (i < N_STATIC_FILE_BLOCKS) {
        return inode.blocks[i];
    }

    int dyn_block_i = (i - N_STATIC_FILE_BLOCKS) / sb_.address_block_capacity + N_STATIC_FILE_BLOCKS;

    // If the wanted block is over the max file block amount, or it's
    // corresponding address block has not been reserved yet, return -1.
    if (dyn_block_i >= (N_DYNAMIC_FILE_BLOCKS + N_STATIC_FILE_BLOCKS) ||
        inode.blocks[dyn_block_i] == -1)
    {
        return -1;
    }

    // The index of the wanted address inside the dyn block
    int i_in_dyn_block = (i - N_STATIC_FILE_BLOCKS) % sb_.address_block_capacity;

    unsigned int address_block = sb_.data_blocks_start_i + inode.blocks[dyn_block_i];
    char pointer[sizeof(int32_t)];

    // Read only the wanted pointer from the address block.
    read_block(address_block, pointer, sizeof(int32_t), i_in_dyn_block * sizeof(int32_t));

    // Return as int
    return bit_cast<int32_t>(pointer);
}

void FFSys::print_inode(INode &inode) {
    cout << "- " << string(inode.name) << endl;
    cout << "  Size: " << inode.size << endl;
    cout << "  I-node: " << inode.index << endl;

    time_t created_time = (time_t)inode.created_time;
    cout << "  Created: " << put_time(localtime(&created_time), "%d/%m/%Y - %H:%M") << endl;

    int reserved_blocks = 0;
    for (int i = 0; i < N_STATIC_FILE_BLOCKS + N_DYNAMIC_FILE_BLOCKS * sb_.address_block_capacity; ++i) {
        if (get_file_block_address(inode, i) == -1) {
            break;
        }
        ++reserved_blocks;
    }
    cout << "  Reserved " << reserved_blocks << " data blocks." << endl;
}

// PRINT FUNCTIONS FOR TESTING

void FFSys::print_superblock() {
    cout << "Block size: " << sb_.block_size << endl;
    cout << "Address block capacity: " << sb_.address_block_capacity << endl << endl;

    cout << "N i-nodes: " << sb_.n_inodes << endl;
    cout << "N free i-nodes: " << sb_.n_free_inodes << endl;
    cout << "N i-node blocks: " << sb_.n_inode_blocks << endl << endl;

    cout << "N data blocks: " << sb_.n_data_blocks << endl;
    cout << "N free data blocks: " << sb_.n_free_data_blocks << endl << endl;

    cout << "Total N blocks: " << sb_.total_n_blocks() << endl;
}

void FFSys::print_all_files()
{
    cout << "Files: " << endl;
    INode file;
    for (int i = 0; i < sb_.n_inodes; ++i) {
        if (!inode_bitmap_->is_free(i)) {
            if (read_inode(i,file)) {
                print_inode(file);
                cout << endl;
            }
        }
    }
}

void FFSys::print_open_files()
{
    cout << "Open files: " << endl;
    INode file;
    for (auto [fd, open_file] : open_files_) {
        if (!read_inode(open_file->inode, file)) {
            cout << "Error: could not read i-node for open file with fd " << fd << endl;
            continue;
        }
        cout << "- " << fd << ": " << string(file.name) << endl;
        cout << "  Current position: " << open_file->pos << endl;
    }
}


} // namespace simfs
