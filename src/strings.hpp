#include <string>
#include <vector>

namespace ccutils {
std::vector<std::string> splitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> strings;
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos) {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }
    strings.push_back(str.substr(prev));
    return strings;
}

void trimInPlace(std::string& s) {
    auto f = [](char c) { return !isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), f));
    s.erase(std::find_if(s.rbegin(), s.rend(), f).base(), s.end());
}

std::string trim(std::string s) {
    trimInPlace(s);
    return s;
}
}
