// include/core/conversation.h
#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    Conversation();
    ~Conversation();

    // Deep copy: gets its own buffer.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer and leaves it empty.
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Doubles capacity when full, so amortized O(1).
    void append(Message m);

    std::size_t size() const noexcept;

    // Throws std::out_of_range if i >= size().
    const Message& at(std::size_t i) const;

    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
