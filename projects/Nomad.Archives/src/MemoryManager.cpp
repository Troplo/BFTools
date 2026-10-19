//
// Created by troplo on 10/17/26.
//

#include "Nomad/Archives/MemoryManager.hpp"
#include "Nomad/Archives/Logger.hpp"
#include <tracy/Tracy.hpp>

#if defined(TRACY_NO_CALLSTACK)
#define TRACY_ALLOC_HOOK(ptr, size) TracyAlloc(ptr, size)
#define TRACY_FREE_HOOK(ptr) TracyFree(ptr)
#else
#define TRACY_ALLOC_HOOK(ptr, size) TracyAllocS(ptr, size, 18)
#define TRACY_FREE_HOOK(ptr) TracyFreeS(ptr, 18)
#endif

#include <cstdlib>
#include <cstring>
#include <new>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <malloc.h>
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <malloc.h>
#endif

// Determine allocation size of an allocated block
static inline size_t QueryAllocSize(void* ptr) {
    if (!ptr)
        return 0;
#if defined(_WIN32)
    size_t sz = _msize(ptr);
    return (sz == static_cast<size_t>(-1)) ? 0 : sz;
#elif defined(__linux__)
    return malloc_usable_size(ptr);
#else
    return 0;
#endif
}

extern "C" {
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void* __real_malloc(size_t size);
__attribute__((weak)) void __real_free(void* ptr);
__attribute__((weak)) void* __real_calloc(size_t count, size_t size);
__attribute__((weak)) void* __real_realloc(void* ptr, size_t size);
#else
void* __real_malloc(size_t size);
void __real_free(void* ptr);
void* __real_calloc(size_t count, size_t size);
void* __real_realloc(void* ptr, size_t size);
#endif

namespace InternalMemAlloc {
    using malloc_fn = void* (*)(size_t);
    using free_fn = void (*)(void*);
    using calloc_fn = void* (*)(size_t, size_t);
    using realloc_fn = void* (*)(void*, size_t);

    static malloc_fn real_malloc = nullptr;
    static free_fn real_free = nullptr;
    static calloc_fn real_calloc = nullptr;
    static realloc_fn real_realloc = nullptr;

    static void ResolveAllocator() {
        if (real_malloc && real_free && real_calloc && real_realloc)
            return;

        if (&__real_malloc && &__real_free && &__real_calloc &&
            &__real_realloc) {
            real_malloc = &__real_malloc;
            real_free = &__real_free;
            real_calloc = &__real_calloc;
            real_realloc = &__real_realloc;
            return;
        }

#if defined(_WIN32)
        HMODULE crt = GetModuleHandleA("msvcrt.dll");
        if (!crt)
            crt = GetModuleHandleA("ucrtbase.dll");
        if (!crt)
            crt = GetModuleHandleA("ntdll.dll");

        if (crt) {
            real_malloc =
                reinterpret_cast<malloc_fn>(GetProcAddress(crt, "malloc"));
            real_free = reinterpret_cast<free_fn>(GetProcAddress(crt, "free"));
            real_calloc =
                reinterpret_cast<calloc_fn>(GetProcAddress(crt, "calloc"));
            real_realloc =
                reinterpret_cast<realloc_fn>(GetProcAddress(crt, "realloc"));
        }
#elif defined(__linux__)
        real_malloc = reinterpret_cast<malloc_fn>(dlsym(RTLD_NEXT, "malloc"));
        real_free = reinterpret_cast<free_fn>(dlsym(RTLD_NEXT, "free"));
        real_calloc = reinterpret_cast<calloc_fn>(dlsym(RTLD_NEXT, "calloc"));
        real_realloc =
            reinterpret_cast<realloc_fn>(dlsym(RTLD_NEXT, "realloc"));
#endif
        if (!real_malloc)
            real_malloc = &::malloc;
        if (!real_free)
            real_free = &::free;
        if (!real_calloc)
            real_calloc = &::calloc;
        if (!real_realloc)
            real_realloc = &::realloc;
    }
} // namespace InternalMemAlloc

void* __wrap_malloc(size_t size) {
    if (!InternalMemAlloc::real_malloc) {
        InternalMemAlloc::ResolveAllocator();
    }
    void* ptr = InternalMemAlloc::real_malloc(size);
#if __DEBUG
    if (ptr && !Nexus::MemoryManager::IsInsideAllocator()) {
        Nexus::MemoryManager::SetInsideAllocator(true);
        if (Nexus::MemoryManager::IsReady()) {
            TRACY_ALLOC_HOOK(ptr, size);
        }
        Nexus::MemoryManager::AddAllocation(
            Nexus::MemoryManager::GetTntMonitor(), ptr, size,
            alignof(std::max_align_t), std::source_location::current());
        Nexus::MemoryManager::SetInsideAllocator(false);
    }
#endif
    return ptr;
}

void __wrap_free(void* ptr) {
    if (!ptr)
        return;

    if (!InternalMemAlloc::real_free) {
        InternalMemAlloc::ResolveAllocator();
    }

#if __DEBUG
    if (!Nexus::MemoryManager::IsInsideAllocator()) {
        Nexus::MemoryManager::SetInsideAllocator(true);
        if (Nexus::MemoryManager::IsReady()) {
            TRACY_FREE_HOOK(ptr);
        }
        Nexus::MemoryManager::RemoveAllocation(
            Nexus::MemoryManager::GetTntMonitor(), ptr);
        Nexus::MemoryManager::SetInsideAllocator(false);
    }
#endif

    InternalMemAlloc::real_free(ptr);
}

void* __wrap_calloc(size_t count, size_t size) {
    if (!InternalMemAlloc::real_calloc) {
        InternalMemAlloc::ResolveAllocator();
    }
    void* ptr = InternalMemAlloc::real_calloc(count, size);
    size_t total = count * size;
#if __DEBUG
    if (ptr && !Nexus::MemoryManager::IsInsideAllocator()) {
        Nexus::MemoryManager::SetInsideAllocator(true);
        if (Nexus::MemoryManager::IsReady()) {
            TRACY_ALLOC_HOOK(ptr, total);
        }
        Nexus::MemoryManager::AddAllocation(
            Nexus::MemoryManager::GetTntMonitor(), ptr, total,
            alignof(std::max_align_t), std::source_location::current());
        Nexus::MemoryManager::SetInsideAllocator(false);
    }
#endif
    return ptr;
}

void* __wrap_realloc(void* oldPtr, size_t size) {
    if (!InternalMemAlloc::real_realloc) {
        InternalMemAlloc::ResolveAllocator();
    }

    size_t oldSize = 0;

#if __DEBUG
    if (oldPtr && !Nexus::MemoryManager::IsInsideAllocator()) {
        Nexus::MemoryManager::SetInsideAllocator(true);
        oldSize = QueryAllocSize(oldPtr);
        if (Nexus::MemoryManager::IsReady()) {
            TRACY_FREE_HOOK(oldPtr);
        }
        Nexus::MemoryManager::RemoveAllocation(
            Nexus::MemoryManager::GetTntMonitor(), oldPtr);
        Nexus::MemoryManager::SetInsideAllocator(false);
    }
#endif

    void* ptr = InternalMemAlloc::real_realloc(oldPtr, size);

#if __DEBUG
    if (!ptr && size > 0 && oldPtr) {
        if (!Nexus::MemoryManager::IsInsideAllocator()) {
            Nexus::MemoryManager::SetInsideAllocator(true);
            if (Nexus::MemoryManager::IsReady()) {
                TRACY_ALLOC_HOOK(oldPtr, oldSize);
            }
            Nexus::MemoryManager::AddAllocation(
                Nexus::MemoryManager::GetTntMonitor(), oldPtr, oldSize,
                alignof(std::max_align_t), std::source_location::current());
            Nexus::MemoryManager::SetInsideAllocator(false);
        }
        return nullptr;
    }

    if (ptr && !Nexus::MemoryManager::IsInsideAllocator()) {
        Nexus::MemoryManager::SetInsideAllocator(true);
        if (Nexus::MemoryManager::IsReady()) {
            TRACY_ALLOC_HOOK(ptr, size);
        }
        Nexus::MemoryManager::AddAllocation(
            Nexus::MemoryManager::GetTntMonitor(), ptr, size,
            alignof(std::max_align_t), std::source_location::current());
        Nexus::MemoryManager::SetInsideAllocator(false);
    }
#endif
    return ptr;
}
}

void* operator new(std::size_t size) {
    void* ptr = __wrap_malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    void* ptr = __wrap_malloc(size);
    if (!ptr)
        throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept { __wrap_free(ptr); }

void operator delete[](void* ptr) noexcept { __wrap_free(ptr); }

void operator delete(void* ptr, std::size_t) noexcept { __wrap_free(ptr); }

void operator delete[](void* ptr, std::size_t) noexcept { __wrap_free(ptr); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return __wrap_malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return __wrap_malloc(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    __wrap_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    __wrap_free(ptr);
}

namespace Nexus {
    AllocationMonitor MemoryManager::m_tntMon{};
    AllocationMonitor MemoryManager::m_nomadMon{};
    thread_local bool MemoryManager::m_insideAllocator = false;
    std::atomic<bool> MemoryManager::m_ready{false};
    std::atomic<bool> MemoryManager::m_detailedTracking{false};

    MemoryManager::MemoryManager() {}

    void MemoryManager::ResetPeak() {
        m_tntMon.PeakBytes.store(
            m_tntMon.CurrentBytes.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        m_nomadMon.PeakBytes.store(
            m_nomadMon.CurrentBytes.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    }

    void MemoryManager::AddAllocation(AllocationMonitor& monitor, void* ptr,
                                      size_t size, size_t alignment,
                                      const std::source_location& location) {
        if (!ptr || !m_ready.load(std::memory_order_relaxed)) {
            return;
        }

        uint64_t cur =
            monitor.CurrentBytes.fetch_add(size, std::memory_order_relaxed) +
            size;
        monitor.TotalAllocated.fetch_add(size, std::memory_order_relaxed);
        monitor.ActiveAllocations.fetch_add(1, std::memory_order_relaxed);

        uint64_t peak = monitor.PeakBytes.load(std::memory_order_relaxed);
        while (cur > peak && !monitor.PeakBytes.compare_exchange_weak(
                                 peak, cur, std::memory_order_relaxed)) {
        }

        if (m_detailedTracking.load(std::memory_order_relaxed)) {
            AllocationInfo info{.Size = size,
                                .Alignment = alignment,
                                .Location = location,
                                .Thread = std::this_thread::get_id(),
                                .Time = std::chrono::steady_clock::now()};

            std::lock_guard lock(monitor.Mutex);
            monitor.Allocations.emplace(ptr, std::move(info));
        }
    }

    bool MemoryManager::RemoveAllocation(AllocationMonitor& monitor,
                                         void* ptr) {
        if (!ptr || !m_ready.load(std::memory_order_relaxed))
            return false;

        size_t size = 0;
        if (m_detailedTracking.load(std::memory_order_relaxed)) {
            std::lock_guard lock(monitor.Mutex);
            auto it = monitor.Allocations.find(ptr);
            if (it != monitor.Allocations.end()) {
                size = it->second.Size;
                monitor.Allocations.erase(it);
            }
        } else {
            size = QueryAllocSize(ptr);
        }

        if (size > 0) {
            monitor.CurrentBytes.fetch_sub(size, std::memory_order_relaxed);
            monitor.TotalFreed.fetch_add(size, std::memory_order_relaxed);
        }
        monitor.ActiveAllocations.fetch_sub(1, std::memory_order_relaxed);

        return true;
    }

#if __DEBUG
    void MemoryManager::DumpReport(std::string_view name) {
        auto& mon = m_tntMon;
        uint64_t cur = mon.CurrentBytes.load(std::memory_order_relaxed);
        uint64_t peak = mon.PeakBytes.load(std::memory_order_relaxed);
        uint64_t total = mon.TotalAllocated.load(std::memory_order_relaxed);
        uint64_t freed = mon.TotalFreed.load(std::memory_order_relaxed);
        uint64_t active = mon.ActiveAllocations.load(std::memory_order_relaxed);

        double curMB = static_cast<double>(cur) / (1024.0 * 1024.0);
        double peakMB = static_cast<double>(peak) / (1024.0 * 1024.0);
        double totalMB = static_cast<double>(total) / (1024.0 * 1024.0);
        double freedMB = static_cast<double>(freed) / (1024.0 * 1024.0);

        Nomad::Archives::Logger::Info("MemoryManager", "!! {} Memory Report !!",
                                      name);
        Nomad::Archives::Logger::Info(
            "MemoryManager", "  Current Allocated: {:.2f} MB ({} bytes)", curMB,
            cur);
        Nomad::Archives::Logger::Info(
            "MemoryManager", "  Peak Allocated:    {:.2f} MB ({} bytes)",
            peakMB, peak);
        Nomad::Archives::Logger::Info(
            "MemoryManager", "  Total Allocated:   {:.2f} MB ({} bytes)",
            totalMB, total);
        Nomad::Archives::Logger::Info(
            "MemoryManager", "  Total Freed:       {:.2f} MB ({} bytes)",
            freedMB, freed);
        Nomad::Archives::Logger::Info("MemoryManager",
                                      "  Active Allocations: {}", active);
    }

    void MemoryManager::DumpLeaks(AllocationMonitor& monitor,
                                  std::string_view name) {
        std::lock_guard lock(monitor.Mutex);
        Nomad::Archives::Logger::Info(
            "MemoryManager", "========== {} Leaks Report ==========", name);

        uint64_t leakedBytes = 0;
        size_t count = 0;
        for (const auto& [ptr, info] : monitor.Allocations) {
            leakedBytes += info.Size;
            if (count < 50) {
                Nomad::Archives::Logger::Info(
                    "MemoryManager",
                    "  LEAK {} size={} align={} thread={} loc={}:{}", ptr,
                    info.Size, info.Alignment, info.Thread,
                    info.Location.file_name(), info.Location.line());
                ++count;
            }
        }
        if (monitor.Allocations.size() > 50) {
            Nomad::Archives::Logger::Info(
                "MemoryManager", "  ... (and {} more active allocations)",
                monitor.Allocations.size() - 50);
        }
        Nomad::Archives::Logger::Info(
            "MemoryManager",
            "Total Leaked: {:.2f} MB ({} bytes in {} allocations)",
            static_cast<double>(leakedBytes) / (1024.0 * 1024.0), leakedBytes,
            monitor.Allocations.size());
    }

    void MemoryManager::DumpLeaks() { DumpLeaks(m_tntMon, "Main"); }
#endif
} // namespace Nexus
