#pragma once
// odl/forcemodel/common.hpp — what every plugin of this module shares (SPEC-forcemodel.md §3.2).

#include <odl/core/result.hpp>
#include <odl/dynamics/force.hpp>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace odl::forcemodel {

using ForceModelError = odl::Diagnostic;

/// What a caller says it read: a manifest id and the sha256 the manifest pins for it.  The plugin cannot know a file's hash — it
/// records what it was told, and says `unsupplied` for a table it was told nothing of (§3.2).
struct SourceRecord {
    std::string id;
    std::string sha256;
};

/// `id=sha256` for each id the plugin reads, in the order the plugin names them, comma-separated; `id=unsupplied` where the caller
/// supplied none.
[[nodiscard]] std::string sha256_list(const std::vector<SourceRecord>& supplied, std::initializer_list<std::string_view> read);

/// A refusal of a model, the ephemeris, the orientation or the frames, passed on with its OWN identifier and message and the
/// plugin's name in front (FMOD-R-009): never a zero, never the previous value.
template <class E>
[[nodiscard]] dyn::DynError passthrough(std::string_view plugin, const E& e) {
    return dyn::DynError{e.id, "forcemodel '" + std::string(plugin) + "': " + e.message};
}

/// The `forcemodel=<plugin>;key=value;...` provenance identifier of §3.2.
class SourceId {
public:
    explicit SourceId(std::string_view plugin) : text_("forcemodel=" + std::string(plugin)) {}
    SourceId& add(std::string_view key, std::string_view value) {
        text_ += ";";
        text_ += key;
        text_ += "=";
        text_ += value;
        return *this;
    }
    [[nodiscard]] const std::string& str() const noexcept { return text_; }

private:
    std::string text_;
};

}  // namespace odl::forcemodel
