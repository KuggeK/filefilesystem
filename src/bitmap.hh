#ifndef BITMAP_HH
#define BITMAP_HH

class Bitmap
{
public:
    Bitmap(char* buffer, unsigned int count);
    Bitmap(unsigned int count);
    ~Bitmap();

    // Reserves the bit at i if it is free.
    bool reserve(unsigned int i);

    // Reserves the first free bit found and returns the index.
    // Returns -1 if no free bit was found.
    int reserve_first_free();

    // Frees the bit at i. Returns false if it is already free.
    bool free(unsigned int i);

    // Checks whether the bit at i is free or not.
    bool is_free(unsigned int i);

    // Returns the nth byte in the bitmap.
    char nth_byte(unsigned int n);

    // Returns the byte to which the ith element belongs.
    char byte_at(unsigned int i);

    // Gets this bitmap as a raw char array.
    char* get_bm();
    char* get_bm(unsigned int n);

    unsigned int get_size();

private:
    char* bm_ = nullptr;
    unsigned int size_;
};

#endif // BITMAP_HH
