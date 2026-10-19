//
// Created by troplo on 10/17/26.
//

#ifndef NOMADARCHIVES_MEMORYMANAGER_H
#define NOMADARCHIVES_MEMORYMANAGER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace Nexus {
    struct AllocationInfo {
        size_t Size{0};
        size_t Alignment{0};
        std::source_location Location{std::source_location::current()};
        std::thread::id Thread{};
        std::chrono::steady_clock::time_point Time{};
    };

    struct AllocationMonitor {
        std::mutex Mutex;
        std::unordered_map<void*, AllocationInfo> Allocations;
        std::atomic<uint64_t> CurrentBytes{0};
        std::atomic<uint64_t> PeakBytes{0};
        std::atomic<uint64_t> TotalAllocated{0};
        std::atomic<uint64_t> TotalFreed{0};
        std::atomic<uint64_t> ActiveAllocations{0};
    };

    class MemoryManager {
      public:
        static MemoryManager& GetInstance() {
            static MemoryManager instance;
            return instance;
        }

      private:
        static AllocationMonitor m_tntMon;
        static AllocationMonitor m_nomadMon;
        static thread_local bool m_insideAllocator;
        static std::atomic<bool> m_ready;
        static std::atomic<bool> m_detailedTracking;

      public:
        MemoryManager();
#if __DEBUG
        static void DumpLeaks(AllocationMonitor& monitor,
                              std::string_view name);
        static void DumpLeaks();
        static void DumpReport(std::string_view name);
#endif
        static void ResetPeak();
        static void AddAllocation(AllocationMonitor& monitor, void* ptr,
                                  size_t size, size_t alignment = 0,
                                  const std::source_location& location =
                                      std::source_location::current());
        static bool RemoveAllocation(AllocationMonitor& monitor, void* ptr);
        static AllocationMonitor& GetTntMonitor() { return m_tntMon; }
        static AllocationMonitor& GetNomadMonitor() { return m_nomadMon; }
        static bool IsInsideAllocator() { return m_insideAllocator; }
        static void SetInsideAllocator(bool v) { m_insideAllocator = v; }
        static bool IsReady() {
            return m_ready.load(std::memory_order_relaxed);
        }
        static void SetReady(bool r) {
            m_ready.store(r, std::memory_order_relaxed);
        }
        static bool IsDetailedTracking() {
            return m_detailedTracking.load(std::memory_order_relaxed);
        }
        static void SetDetailedTracking(bool dt) {
            m_detailedTracking.store(dt, std::memory_order_relaxed);
        }
    };
} // namespace Nexus

namespace Nomad::Archives {
    using MemoryManager = Nexus::MemoryManager;
    using AllocationMonitor = Nexus::AllocationMonitor;
    using AllocationInfo = Nexus::AllocationInfo;
} // namespace Nomad::Archives

#endif // NOMADARCHIVES_MEMORYMANAGER_H
