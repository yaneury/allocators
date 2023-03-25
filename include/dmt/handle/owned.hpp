#pragma once

#include <cstddef>

#include "provider.hpp"

namespace dmt::handle {

template <class T> class Owned {
public:
  Owned(Provider* p, Id h) : p_(p), h_(h) {}

  T* operator->() { return reinterpret_cast<T*>(p_->GetCurrentAddress(h_)); }

  T& operator*() {
    T* ptr = reinterpret_cast<T*>(p_->GetCurrentAddress(h_));
    return *ptr;
  }

private:
  Provider* p_;
  Id h_;
};

template <class T, class... Args>
Owned<T> MakeOwned(Provider& p, Args... args) {
  Id h = p.Request(sizeof(T));
  return Owned<T>(&p, h);
}

} // namespace dmt::handle
