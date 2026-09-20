// src/sentinel_scanner.cpp

#include "core/sentinel_scanner.h"

namespace {

std::size_t longest_matching_suffix(const std::string& text, const std::string& pattern) {
    if (pattern.empty()) {
        return 0;
    }
    std::size_t max_len = pattern.size() - 1;
    if (max_len > text.size()) {
        max_len = text.size();
    }
    for (std::size_t len = max_len; len > 0; --len) {
        // Compare the last `len` chars of text to the first `len` chars of pattern.
        if (text.compare(text.size() - len, len, pattern, 0, len) == 0) {
            return len;
        }
    }
    return 0;
}

}  // namespace

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out out{"", false};

    std::string buffer = pending_ + std::string(chunk);
    pending_.clear();

    std::size_t match_pos = buffer.find(sentinel_);
    if (match_pos != std::string::npos) {
        out.safe_text      = buffer.substr(0, match_pos);
        out.sentinel_found = true;
        return out;
    }

    std::size_t suffix_len = longest_matching_suffix(buffer, sentinel_);
    out.safe_text      = buffer.substr(0, buffer.size() - suffix_len);
    pending_            = buffer.substr(buffer.size() - suffix_len);
    out.sentinel_found  = false;
    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{pending_, false};
    pending_.clear();
    return out;
}
