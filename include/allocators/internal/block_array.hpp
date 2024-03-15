#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <utility>

#include <allocators/internal/util.hpp>

namespace allocators::internal {

struct BlockArrayHeader {
  std::size_t size;
  std::byte* next;
};

template <std::size_t Size, class T> class BlockArray {
public:
  ALLOCATORS_NO_COPY_NO_MOVE_NO_DEFAULT(BlockArray);

  bool IsFull() const { return header.size == kCapacity; }

  bool IsEmpty() const { return header.size == 0; }

  bool HasNext() const { return header.next != 0; }

  std::size_t GetSize() const { return header.size; }

  std::size_t GetCapacity() const { return kCapacity; }

  BlockArray<Size, T>* GetNext() const {
    return reinterpret_cast<BlockArray<Size, T>*>(header.next);
  }

  void PushBackUnchecked(T value) { entries[header.size++] = value; }

  bool PushBack(T value) {
    if (IsFull())
      return false;

    PushBackUnchecked(value);
    return true;
  }

  T PopBackUnchecked() { return entries[--header.size]; }

  bool Remove(T target) {
    for (auto i = 0; i < header.size; ++i) {
      if (entries[i] == target) {
        if (i != header.size - 1)
          std::swap(entries[i], entries[header.size - 1]);
        PopBackUnchecked();
        return true;
      }
    }

    return false;
  }

  std::optional<T> RemoveIf(std::function<bool(const T&)> predicate) {
    for (auto i = 0; i < header.size; ++i) {
      if (predicate(entries[i])) {
        if (i != header.size - 1)
          std::swap(entries[i], entries[header.size - 1]);
        return PopBackUnchecked();
      }
    }

    return std::nullopt;
  }

  void SetNext(std::byte* next) { header.next = next; }

private:
  static constexpr std::size_t kCapacity =
      (Size - sizeof(BlockArrayHeader)) / AlignUp(sizeof(T), sizeof(void*));

  BlockArrayHeader header;
  T entries[kCapacity];
};

template <std::size_t Size, class T>
BlockArray<Size, T>* AsBlockArrayPtr(std::byte* block, bool zero_out = true) {
  if (zero_out)
    bzero(block, Size);
  return reinterpret_cast<BlockArray<Size, T>*>(block);
}

} // namespace allocators::internal
