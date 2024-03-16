#pragma once

#include <bitset>
#include <cstring>
#include <functional>
#include <optional>

#include <allocators/internal/platform.hpp>
#include <allocators/internal/util.hpp>

namespace allocators::internal {

template <std::size_t Size> class BlockMap {
public:
  ALLOCATORS_NO_COPY_NO_MOVE_NO_DEFAULT(BlockMap);

  bool IsFull() const { return GetSize() == GetCapacity(); }

  bool IsEmpty() const { return header.occupied.none(); }

  bool HasNext() const { return header.next != nullptr; }

  std::size_t GetSize() const { return header.occupied.count(); }

  std::size_t GetCapacity() const { return kCapacity; }

  BlockMap<Size>* GetNext() const {
    return reinterpret_cast<BlockMap<Size>*>(header.next);
  }

  bool Insert(VirtualAddressRange va_range) {
    auto key = va_range.address;
    std::hash<std::uint64_t> hasher;
    std::size_t hash = hasher(key);
    std::size_t startIndex = hash % GetCapacity();
    std::size_t probeIndex = startIndex;

    if (header.occupied[probeIndex]) {
      do {
        probeIndex = (probeIndex + 1) % GetCapacity();
      } while (probeIndex != startIndex && header.occupied[probeIndex]);

      if (probeIndex == startIndex)
        return false;
    }

    table[probeIndex] = va_range;
    header.occupied.set(probeIndex);
    return true;
  }

  std::optional<VirtualAddressRange> Take(std::uint64_t key) {
    auto index_or = Locate(key);
    if (!index_or.has_value())
      return std::nullopt;

    auto index = index_or.value();
    auto value = table[index];
    header.occupied.reset(index);
    return value;
  }

  void SetNext(std::byte* next) { header.next = next; }

private:
  std::optional<std::size_t> Locate(std::uint64_t address) const {
    std::hash<std::uint64_t> hasher;
    std::size_t hash = hasher(address);
    std::size_t startIndex = hash % GetCapacity();
    std::size_t probeIndex = startIndex;

    do {
      if (header.occupied[probeIndex]) {
        if (table[probeIndex].address == address)
          return probeIndex;

        // The stopping condition for this probe is not when it
        // encounters the first empty slot. Rather, it's when ti
        // encounters a table entry with a different address. The
        // reason for this is that for the pathological case where
        // all entries have same entries, we can get false negatives
        // after removing the first entry. This will happen because the
        // probe algorithm will see an empty slot and return std::nullopt,
        // even though all subsequent slots are taken with equivalent
        // entries.
        if (table[probeIndex].address != address)
          return std::nullopt;
      }

      probeIndex = (probeIndex + 1) % GetCapacity();
    } while (probeIndex != startIndex);

    return std::nullopt;
  }

  static constexpr std::size_t kEntrySize = sizeof(VirtualAddressRange);
  static constexpr std::size_t kMaxEntriesEstimate =
      Size / sizeof(VirtualAddressRange);

  struct Header {
    std::byte* next;
    std::bitset<kMaxEntriesEstimate> occupied;
  };

  static constexpr std::size_t kCapacity = (Size - sizeof(Header)) / kEntrySize;

  Header header;
  VirtualAddressRange table[kCapacity];
};

template <std::size_t Size>
BlockMap<Size>* AsBlockMapPtr(std::byte* block, bool zero_out = true) {
  if (zero_out)
    bzero(block, Size);
  return reinterpret_cast<BlockMap<Size>*>(block);
}

} // namespace allocators::internal
