#include "GuidGenerator.hpp"

#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace seabattle {

std::string GuidGenerator::generate() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t>
        dis;  // равномерное распределение по всему диапазону uint64_t

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');

    // Формат: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    ss << std::setw(8) << ((part1 >> 32) & 0xFFFFFFFF) << '-';
    ss << std::setw(4) << ((part1 >> 16) & 0xFFFF) << '-';
    ss << std::setw(4) << (part1 & 0xFFFF) << '-';
    ss << std::setw(4) << ((part2 >> 48) & 0xFFFF) << '-';
    ss << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);

    return ss.str();
}

bool GuidGenerator::isValid(const std::string& guid) {
    static const std::regex pattern(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", std::regex::icase);
    return std::regex_match(guid, pattern);
}

}  // namespace seabattle