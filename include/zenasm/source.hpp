#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace zenasm {

struct SourceLocation {
    std::size_t offset = 0;
    int line = 1;
    int column = 1;
};

struct SourceSpan {
    SourceLocation begin {};
    SourceLocation end {};

    [[nodiscard]] bool valid() const noexcept {
        return end.offset >= begin.offset;
    }
};

class SourceFile {
  public:
    SourceFile() = default;
    SourceFile(std::string path, std::string content);

    static SourceFile load(const std::string& path);

    [[nodiscard]] std::string_view path() const noexcept;
    [[nodiscard]] std::string_view content() const noexcept;
    [[nodiscard]] std::string_view lineText(int line) const;
    [[nodiscard]] const std::vector<std::size_t>& lineOffsets() const noexcept;

  private:
    void computeLineOffsets();

    std::string path_ {};
    std::string content_ {};
    std::vector<std::size_t> line_offsets_ {};
};

[[nodiscard]] std::string_view slice(const SourceFile& source, SourceSpan span);

}  // namespace zenasm
