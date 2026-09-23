// src/conversation.cpp
#include "core/conversation.h"
#include <stdexcept>
#include <string>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() {
    delete[] data_;  // delete[] on nullptr is a no-op, so empty/moved-from is fine
}

Conversation::Conversation(const Conversation& other) {
    if (other.size_ == 0) return;

    data_ = new Message[other.size_];
    size_ = other.size_;
    capacity_ = other.size_;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    // Copy-and-swap: tmp's destructor cleans up our old buffer, and self-assignment just works.
    Conversation tmp(other);
    std::swap(data_, tmp.data_);
    std::swap(size_, tmp.size_);
    std::swap(capacity_, tmp.capacity_);
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)) {}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    return *this;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        // Double the capacity (starting at 1) so appends are amortized O(1).
        std::size_t new_cap = (capacity_ == 0) ? 1 : capacity_ * 2;
        Message* buf = new Message[new_cap];
        for (std::size_t i = 0; i < size_; ++i) {
            buf[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = buf;
        capacity_ = new_cap;
    }
    data_[size_] = std::move(m);
    ++size_;
}

std::size_t Conversation::size() const noexcept {
    return size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}

const Message* Conversation::begin() const noexcept {
    return data_;
}

const Message* Conversation::end() const noexcept {
    return data_ + size_;
}
