#pragma once

#include <string>

namespace seabattle {

class GuidGenerator {
   public:
    static std::string generate();
    static bool isValid(const std::string& guid);
};

}  // namespace seabattle