// include/core/conversation.h

#pragma once
#include <cstddef>
#include "core/message.h"

class Conversation {
public:
    // Empty conversation: size() == 0, no allocation yet.
    Conversation() = default;

    // Releases all owned Message storage. No effect if already empty
    // (e.g. moved-from).
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    // this->begin() must differ from other.begin() afterward.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer — no per-element copying. Afterward,
    // other is left valid and empty (safe to destroy or reassign).
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed. Amortized O(1) —
    // document and justify your growth strategy in the design log
    // (see Appendix C if you want a refresher first).
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept { return size_; }

    // Bounds-checked access. Decide what happens on i >= size() (throw,
    // assert, whatever you pick) and test that behavior explicitly.
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first. begin() == end() when
    // size() == 0.
    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

private:
    // Grows the backing array to at least min_capacity, moving existing elements into the new buffer and freeing the old one.
    void grow(std::size_t min_capacity);

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
