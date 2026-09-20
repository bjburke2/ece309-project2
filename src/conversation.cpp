// src/conversation.cpp

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

namespace {
// Growth factor: simple doubling from a small base capacity.
constexpr std::size_t kInitialCapacity = 4;
constexpr std::size_t kGrowthFactor    = 2;
}  // namespace

Conversation::~Conversation() {
    delete[] data_;
}

Conversation::Conversation(const Conversation& other) {
    // Deep copy: allocate our own buffer, then copy each element with
    // Message's own (compiler-generated, deep) copy assignment.
    capacity_ = other.capacity_;
    size_     = other.size_;
    data_     = capacity_ > 0 ? new Message[capacity_] : nullptr;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) {
        return *this;
    }
    // Build the new buffer first, then swap it in — if allocation or
    // copying above ever threw, *this would be untouched.
    Message* new_data = other.capacity_ > 0 ? new Message[other.capacity_] : nullptr;
    for (std::size_t i = 0; i < other.size_; ++i) {
        new_data[i] = other.data_[i];
    }
    delete[] data_;
    data_     = new_data;
    size_     = other.size_;
    capacity_ = other.capacity_;
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    // Steal the pointer, then zero out the source so its destructor
    // is a no-op (delete[] nullptr) and it's left valid and empty.
    other.data_     = nullptr;
    other.size_     = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    delete[] data_;
    data_     = other.data_;
    size_     = other.size_;
    capacity_ = other.capacity_;
    other.data_     = nullptr;
    other.size_     = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::grow(std::size_t min_capacity) {
    std::size_t new_capacity = (capacity_ == 0) ? kInitialCapacity : capacity_ * kGrowthFactor;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }

    Message* new_data = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        new_data[i] = std::move(data_[i]); // move existing elements, don't copy
    }
    delete[] data_;
    data_     = new_data;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        grow(size_ + 1);
    }
    data_[size_] = std::move(m);
    ++size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}
