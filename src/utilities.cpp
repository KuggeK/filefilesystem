#include "utilities.hh"

#include <algorithm>
#include <cctype>

using namespace std;

namespace Utilities {

vector<string> split(const string& str, char delim, bool keep_empty)
{
    vector<string> result = {""};
    bool insideQuotation = false;
    for ( char currentChar : str )
    {
        if ( currentChar == '"' )
        {
            insideQuotation = not insideQuotation;
        }
        else if ( currentChar == delim and not insideQuotation)
        {
            if (keep_empty or result.back() != "") {
                result.push_back("");
            }
        }
        else
        {
            result.back().push_back(currentChar);
        }
    }
    if (!keep_empty and result.back() == "") {
        result.pop_back();
    }
    return result;
}

string string_to_lower(string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

string string_to_upper(string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

bool is_int(string str, bool allow_negative)
{
    if (str.empty()) {
        return false;
    }

    // Negative number check
    if (str.at(0) == '-') {
        if (allow_negative) {
            str = str.substr(1);
        } else {
            return false;
        }
    }

    // All characters (except first, if negatives are allowed) need to be digits
    for (char c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }

    return true;
}

}
