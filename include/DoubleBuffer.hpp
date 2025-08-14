#include <atomic>
#include <array>
#include <thread>
#include <type_traits>

namespace yy {
template <typename T>
class DoubleBuffer {
    static_assert(std::is_copy_constructible_v<T>,
                 "T must be copy constructible");

private:
    // Buffer structure with padding (64 bytes) to prevent false sharing

    // mutable allows modification in const method
    // conceptionally, since data is not changed, read() can be a const method
    // but we need to modify ref_count, so it must be mutable
    mutable std::atomic<unsigned> reader_cnt_{0};

    // double buffer storage, 2 copies.
    std::array<T, 2> buffers_;
    
    // atomic read index (multiple readers)
    std::atomic<int> read_index_{0};
    
    // non-atomic write index (single writer)
    int write_index_{1};

public:
    // must provide init value for T
    explicit DoubleBuffer(const T& init_value) {
        buffers_[0] = init_value;
        buffers_[1] = init_value;
    }

    // disable copy
    DoubleBuffer(const DoubleBuffer&) = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;

    /**
     * @brief Reads the current value (thread-safe for multiple readers)
     * @return Copy of the stored data
     */
    T read() const noexcept {
        // Increment reference count to protect buffer
        reader_cnt_.fetch_add(1, std::memory_order_acquire);
        
        T value = buffers_[read_index_.load(std::memory_order_acquire)];

        // Decrement reference count when done
        reader_cnt_.fetch_sub(1, std::memory_order_release);
        
        return value;
    }

    /**
     * @brief Updates the stored value (single writer thread only)
     * @param new_value The new value to store
     */
    void write(const T& new_value) noexcept {
        // Update the write buffer (no readers access this yet)
        buffers_[write_index_] = new_value;
        
        // Atomically swap read and write indices
        const int old_read_index = read_index_.exchange(write_index_, std::memory_order_acq_rel);
        write_index_ = old_read_index;

        while (reader_cnt_.load(std::memory_order_acquire) != 0) {
            // Avoid busy waiting - yield CPU to other threads
            std::this_thread::yield();
        }
    }
};
}
