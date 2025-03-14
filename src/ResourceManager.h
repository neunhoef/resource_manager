#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>

// Alignment for cache line to prevent false sharing
struct alignas(64) EpochSlot {
  std::atomic<uint64_t> epoch{0};
};

template <typename T> class ResourceManager {
private:
  // Resource management
  std::atomic<T *> current_resource;
  std::atomic<uint64_t> global_epoch{1}; // Start at 1, 0 means "not reading"
  std::mutex writer_mutex;

  // Epoch slots for reader tracking
  std::vector<EpochSlot> epoch_slots;

  // Thread-local storage for slot index
  static thread_local std::optional<size_t> thread_slot_index;

  // Constants
  static constexpr size_t EPOCH_SLOTS = 128;

  // Get the thread's slot index, initializing if needed
  size_t get_thread_slot() {
    if (!thread_slot_index) {
      // Random initialization to distribute threads across slots
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<size_t> dist(0, EPOCH_SLOTS - 1);
      thread_slot_index = dist(gen);
    }
    return *thread_slot_index;
  }

public:
  // Check if all active readers are using newer epochs than the given one
  bool can_reclaim(uint64_t epoch) {
    for (const auto &slot : epoch_slots) {
      uint64_t slot_epoch = slot.epoch.load(std::memory_order_seq_cst);

      // If slot is reading (non-zero) and using epoch <= target, we can't
      // reclaim
      if (slot_epoch != 0 && slot_epoch <= epoch) {
        return false;
      }
    }
    return true;
  }

  // Constructor with initial resource
  explicit ResourceManager(std::unique_ptr<T> initial_resource)
      : epoch_slots(EPOCH_SLOTS) {
    current_resource.store(initial_resource.release(),
                           std::memory_order_relaxed);
  }

  // Destructor
  ~ResourceManager() {
    auto [current, epoch] = update(nullptr);
    while (!can_reclaim(epoch)) {
      std::this_thread::yield();
    }
    current.reset();
  }

  // Delete copy/move constructors and assignment operators
  ResourceManager(const ResourceManager &) = delete;
  ResourceManager &operator=(const ResourceManager &) = delete;
  ResourceManager(ResourceManager &&) = delete;
  ResourceManager &operator=(ResourceManager &&) = delete;

  // Reader API: Get access to the resource
  template <typename F>
  auto read(F &&f) -> decltype(f(std::declval<const T &>())) {
    // Get thread's slot
    size_t slot = get_thread_slot();

    // Get current global epoch
    uint64_t current_epoch = global_epoch.load(std::memory_order_acquire);

    // Try to find an available slot
    while (true) {
      // Try to announce reading at this epoch using compare_exchange
      uint64_t expected = 0; // expected: 0 (not in use)
      if (epoch_slots[slot].epoch.compare_exchange_strong(
              expected, current_epoch, std::memory_order_seq_cst,
              std::memory_order_seq_cst)) {
        // Successfully claimed the slot
        // Read resource pointer and execute reader function
        T *resource_ptr = current_resource.load(std::memory_order_acquire);

        decltype(f(std::declval<const T &>())) result{};

        // Execute reader function
        if (resource_ptr != nullptr) {
          result = f(*resource_ptr);
        }

        // Mark slot as "not reading" with a simple write
        epoch_slots[slot].epoch.store(0, std::memory_order_release);

        return result;
      }

      // Slot is in use, try the next one:
      slot += 1;
      if (slot >= EPOCH_SLOTS) {
        slot = 0;
      }
      // Continue the loop with the new slot
    }
  }

  // Writer API: Update the resource
  std::pair<std::unique_ptr<T>, uint64_t>
  update(std::unique_ptr<T> new_resource) {
    std::lock_guard<std::mutex> lock(writer_mutex, std::adopt_lock);

    // Extract raw pointer from unique_ptr
    T *new_ptr = new_resource.release();

    // Swap pointers with SeqCst ordering for maximum visibility
    T *old_ptr = current_resource.exchange(new_ptr, std::memory_order_seq_cst);

    // Advance global epoch with release to ensure all threads see the new epoch
    // and new current_resource:
    uint64_t retire_epoch =
        global_epoch.fetch_add(1, std::memory_order_release);

    return std::pair(std::unique_ptr<T>(old_ptr), retire_epoch);
  }
};

// Initialize the thread_local storage
template <typename T>
thread_local std::optional<size_t> ResourceManager<T>::thread_slot_index;
