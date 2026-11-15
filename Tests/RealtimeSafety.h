#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace heapProbe {
inline thread_local bool active = false;
inline std::atomic<size_t> allocations{0};
inline void record() { if (active) allocations.fetch_add(1, std::memory_order_relaxed); }
}
void* operator new(size_t size) {
    heapProbe::record();
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
void* operator new(size_t size, std::align_val_t alignment) {
    heapProbe::record();
    void* p = nullptr;
#if defined(_WIN32)
    p = _aligned_malloc(size ? size : 1, static_cast<size_t>(alignment));
#else
    if (posix_memalign(&p, static_cast<size_t>(alignment), size ? size : 1) != 0) p = nullptr;
#endif
    if (p) return p;
    throw std::bad_alloc{};
}
void* operator new[](size_t size, std::align_val_t a) { return ::operator new(size, a); }
void operator delete(void* p, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
