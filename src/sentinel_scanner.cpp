// src/sentinel_scanner.cpp
#include "core/sentinel_scanner.h"
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    // Search the held-back tail plus the new chunk so a sentinel split across chunks still gets caught.
    std::string window = std::move(pending_);
    window.append(chunk.data(), chunk.size());
    pending_.clear();

    if (sentinel_.empty()) return {std::move(window), false};

    std::size_t pos = window.find(sentinel_);
    if (pos != std::string::npos) {
        window.resize(pos);  // keep only the text before the sentinel
        return {std::move(window), true};
    }

    // The last size() - 1 chars might be the start of the sentinel, so hold them back.
    const std::size_t keep = sentinel_.size() - 1;
    if (window.size() <= keep) {
        pending_ = std::move(window);
        return {std::string(), false};
    }
    const std::size_t split = window.size() - keep;
    pending_.assign(window, split, keep);
    window.resize(split);
    return {std::move(window), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    std::string rest = std::move(pending_);
    pending_.clear();
    return {std::move(rest), false};
}
