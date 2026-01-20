#ifndef UTILITIES_HH
#define UTILITIES_HH

#include <vector>
#include <string>

/**
 * Namespace for miscellaneous utility functions that are useful in many
 * different contexts.
 */
namespace Utilities {

// Splits the given string into parts by a given delimiter and stores them
// into a vector. If keep_empty is false, the function doesn't store empty parts.
std::vector<std::string> split(const std::string& str, char delim, bool keep_empty = false);

// Returns a copy of the given string in lowercase.
std::string string_to_lower(std::string str);

// Returns a copy of the given string in uppercase.
std::string string_to_upper(std::string str);

// Checks if the given string is an integer, and consists only of digits.
// If allow_negative is true, the string can additionally have a '-' prefix.
bool is_int(std::string str, bool allow_negative = false);

}

#endif // UTILITIES_HH
