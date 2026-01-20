#include "bitmap.hh"

#include <algorithm>

Bitmap::Bitmap(char *buffer, unsigned int byte_count):
    bm_(new char[byte_count]), size_(byte_count)
{
    for (int i = 0; i < byte_count; ++i) {
        bm_[i] = *(buffer+i);
    }
}

Bitmap::Bitmap(unsigned int byte_count):
    bm_(new char[byte_count]), size_(byte_count)
{
    // Initialize as all free
    std::fill_n(bm_, byte_count, 0b11111111);
}

Bitmap::~Bitmap()
{
    delete[] bm_;
}

bool Bitmap::reserve(unsigned int i)
{
    if (!is_free(i)) {
        return false;
    }

    // Set bit to 0
    bm_[i / 8] &= ~(1 << (i % 8));
    return true;
}

int Bitmap::reserve_first_free()
{
    for (unsigned int i = 0; i < size_; ++i) {
        if (reserve(i)) {
            return i;
        }
    }

    return -1;
}

bool Bitmap::free(unsigned int i)
{
    if (is_free(i)) {
        return false;
    }

    bm_[i / 8] |= (1 << (i % 8));
    return true;
}

bool Bitmap::is_free(unsigned int i)
{
    return (1 << (i % 8) & bm_[i / 8]) != 0;
}

char *Bitmap::get_bm()
{
    return bm_;
}

char *Bitmap::get_bm(unsigned int n)
{
    return bm_ + n;
}

unsigned int Bitmap::get_size()
{
    return size_;
}
