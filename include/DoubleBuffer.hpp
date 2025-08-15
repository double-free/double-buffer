#include <atomic>
#include <thread>
#include <type_traits>

namespace yy {
template <typename T> class DoubleBuffer {
  static_assert(std::is_copy_constructible_v<T>,
                "T must be copy constructible");

private:
  // Buffer structure with padding (64 bytes) to prevent false sharing
  struct alignas(64) Buffer {
    T data;
    // Mutable allows modification in const method
    // conceptionally, since data is not changed, read() can be a const method
    // but we need to modify ref_count, so it must be mutable
    mutable std::atomic<unsigned> ref_count{0};
  };

  // Double buffer storage, 2 copies.
  Buffer buffers_[2];

  // Atomic read index (multiple readers)
  std::atomic<Buffer *> read_buffer_{&buffers_[0]};

  // Non-atomic write index (single writer)
  Buffer *write_buffer_{&buffers_[1]};

public:
  // Must provide init value for T
  explicit DoubleBuffer(const T &init_value) {
    buffers_[0].data = init_value;
    buffers_[1].data = init_value;
  }

  // Disallow copy
  DoubleBuffer(const DoubleBuffer &) = delete;
  DoubleBuffer &operator=(const DoubleBuffer &) = delete;

  /**
   * @brief Reads the current value (thread-safe for multiple readers)
   * @return Copy of the stored data
   */
  T read() const noexcept {
    // Retry if copied read ptr does not match realtime read ptr
    while (true) {
      // Load the current read buffer
      const Buffer *read_ptr = read_buffer_.load(std::memory_order_acquire);

      // Increment reference count to protect buffer
      read_ptr->ref_count.fetch_add(1, std::memory_order_relaxed);

      if (read_ptr != read_buffer_.load(std::memory_order_acquire)) {
        // If the read pointer has changed, we need to retry
        read_ptr->ref_count.fetch_sub(1, std::memory_order_relaxed);
        continue; // Retry
      }

      // Copy the data to return
      T value = read_ptr->data;

      // Decrement reference count when done
      read_ptr->ref_count.fetch_sub(1, std::memory_order_release);

      return value;
    }
  }

  /**
   * @brief Updates the stored value (single writer thread only)
   * @param new_value The new value to store
   */
  void write(const T &new_value) noexcept {
    // Update the write buffer (no readers access this yet)
    write_buffer_->data = new_value;

    // Atomically swap read and write indices
    const Buffer *prev_read_ptr =
        read_buffer_.exchange(write_buffer_, std::memory_order_acq_rel);

    // Wait until all readers are done with the old buffer
    while (prev_read_ptr->ref_count.load(std::memory_order_acquire) != 0) {
      // Avoid busy waiting - yield CPU to other threads
      std::this_thread::yield();
    }
  }
};
} // namespace yy
