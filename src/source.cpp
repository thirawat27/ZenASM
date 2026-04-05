#include "zenasm/source.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace zenasm {

SourceFile::SourceFile(std::string path, std::string content)
    : path_(std::move(path)),
      content_(std::move(content)) {
    computeLineOffsets();
}

SourceFile SourceFile::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open source file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return SourceFile(path, buffer.str());
}

std::string_view SourceFile::path() const noexcept {
    return path_;
}

std::string_view SourceFile::content() const noexcept {
    return content_;
}

std::string_view SourceFile::lineText(const int line) const {
    if (line <= 0 || static_cast<std::size_t>(line) > line_offsets_.size()) {
        return {};
    }

    const std::size_t start = line_offsets_[static_cast<std::size_t>(line - 1)];
    std::size_t end = content_.size();
    if (static_cast<std::size_t>(line) < line_offsets_.size()) {
        end = line_offsets_[static_cast<std::size_t>(line)];
    }

    while (end > start && (content_[end - 1] == '\n' || content_[end - 1] == '\r')) {
        --end;
    }
    return std::string_view(content_).substr(start, end - start);
}

const std::vector<std::size_t>& SourceFile::lineOffsets() const noexcept {
    return line_offsets_;
}

void SourceFile::computeLineOffsets() {
    line_offsets_.clear();
    line_offsets_.push_back(0);
    for (std::size_t index = 0; index < content_.size(); ++index) {
        if (content_[index] == '\n' && index + 1 < content_.size()) {
            line_offsets_.push_back(index + 1);
        }
    }
}

std::string_view slice(const SourceFile& source, const SourceSpan span) {
    const auto content = source.content();
    if (!span.valid() || span.end.offset > content.size() || span.begin.offset > span.end.offset) {
        return {};
    }
    return content.substr(span.begin.offset, span.end.offset - span.begin.offset);
}

}  // namespace zenasm
