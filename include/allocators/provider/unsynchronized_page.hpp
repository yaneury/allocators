#pragma once

namespace allocators::provider {

// Provider class that returns page-aligned and page-sized blocks. The page size
// is determined by the platform, 4KB for most scenarios. For the actual page
// size used on particular platform, see |internal::GetPageSize|. This provider
// is not thread-safe.
template <class... Args> class UnsynchronizedPage {
public:
private:
};

} // namespace allocators::provider
