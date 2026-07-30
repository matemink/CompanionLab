#pragma once

#include "companionlab/presentation/BoardTypeResolver.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace companionlab::adapters::ardupilot {

class BoardTypeCatalog final
    : public presentation::BoardTypeResolver {
public:
    [[nodiscard]] static BoardTypeCatalog from_file(
        const std::filesystem::path& path
    );

    [[nodiscard]] static BoardTypeCatalog from_stream(
        std::istream& input
    );

    [[nodiscard]] std::optional<presentation::BoardTypeMatch> resolve(
        std::uint16_t board_type
    ) const override;

    [[nodiscard]] std::size_t board_type_count() const;
    [[nodiscard]] std::size_t alias_count() const;

private:
    struct Entry {
        std::string preferred_name;
        std::vector<std::string> aliases;
        bool preferred_is_reserved{false};
    };

    void add(
        std::uint16_t board_type,
        std::string name,
        bool is_reserved
    );

    std::unordered_map<std::uint16_t, Entry> entries_;
    std::size_t alias_count_{0};
};

}  // namespace companionlab::adapters::ardupilot
