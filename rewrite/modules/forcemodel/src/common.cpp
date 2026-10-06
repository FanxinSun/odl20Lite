#include <odl/forcemodel/common.hpp>

namespace odl::forcemodel {

std::string sha256_list(const std::vector<SourceRecord>& supplied, std::initializer_list<std::string_view> read) {
    std::string out;
    for (std::string_view id : read) {
        std::string_view hash = "unsupplied";
        for (const auto& s : supplied)
            if (s.id == id && !s.sha256.empty()) hash = s.sha256;
        if (!out.empty()) out += ",";
        out += id;
        out += "=";
        out += hash;
    }
    return out;
}

}  // namespace odl::forcemodel
