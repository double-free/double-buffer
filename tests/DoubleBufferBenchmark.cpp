#include "DoubleBuffer.hpp"

#include <gtest/gtest.h>
#include <shared_mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <numeric>

// Test configuration
constexpr int kIterations = 100000;
constexpr int kMaxThreads = 16;
constexpr int kValueSize = 32; // Elements in test array

struct TestData {
    int data[kValueSize];
    TestData(int v = 0) { std::fill_n(data, kValueSize, v); }
    bool operator==(const TestData& other) const {
        return std::equal(data, data + kValueSize, other.data);
    }
};

class DoubleBufferTest : public ::testing::Test {
protected:
    yy::DoubleBuffer<TestData> buffer{TestData(0)};
};

// Basic functionality tests
TEST_F(DoubleBufferTest, InitialValue) {
    EXPECT_EQ(buffer.read(), TestData(0));
}

TEST_F(DoubleBufferTest, ReadAfterWrite) {
    buffer.write(TestData(42));
    EXPECT_EQ(buffer.read(), TestData(42));
}

// Performance measurement utilities
class PerfTimer {
    std::chrono::high_resolution_clock::time_point start;
public:
    PerfTimer() : start(std::chrono::high_resolution_clock::now()) {}
    double elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }
};

template<typename Func>
double measure_throughput(Func&& f, int iterations) {
    PerfTimer timer;
    for (int i = 0; i < iterations; ++i) {
        f();
    }
    return iterations / timer.elapsed();
}

// Benchmark tests
TEST_F(DoubleBufferTest, ReadThroughputSingleThread) {
    const double ops_sec = measure_throughput([this] {
        buffer.read();
    }, kIterations);
    
    std::cout << "Single-thread read throughput: " << ops_sec << " ops/sec\n";
    EXPECT_GT(ops_sec, 1e6); // Expect >1M ops/sec
}

TEST_F(DoubleBufferTest, WriteLatency) {
    constexpr int kSamples = 1000;
    std::vector<double> latencies;
    latencies.reserve(kSamples);
    
    for (int i = 0; i < kSamples; ++i) {
        PerfTimer timer;
        buffer.write(TestData(i));
        latencies.push_back(timer.elapsed() * 1e6); // microseconds
    }
    
    const double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / kSamples;
    std::cout << "Average write latency: " << avg << " μs\n";
    EXPECT_LT(avg, 10); // Expect <10μs latency
}

// Concurrency tests
TEST_F(DoubleBufferTest, ConcurrentReads) {
    std::atomic<bool> running{true};
    std::atomic<int> read_count{0};
    std::vector<std::thread> readers;
    
    // Start reader threads
    for (int i = 0; i < kMaxThreads; ++i) {
        readers.emplace_back([&] {
            while (running) {
                auto val = buffer.read();
                EXPECT_EQ(val, TestData(0)) << "Invalid value read";
                read_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Let them run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    
    for (auto& t : readers) t.join();
    std::cout << "Total reads: " << read_count.load() << "\n";
    EXPECT_GT(read_count.load(), kMaxThreads * 1000);
}

TEST_F(DoubleBufferTest, ReadDuringWrite) {
    std::atomic<int> valid_reads{0};
    constexpr int kTestValue = 123456;
    
    std::thread writer([&] {
        for (int i = 0; i < 100; ++i) {
            buffer.write(TestData(kTestValue));
        }
    });
    
    std::thread reader([&] {
        for (int i = 0; i < kIterations; ++i) {
            auto val = buffer.read();
            if ((val == TestData(0)) || (val == TestData(kTestValue))) {
                valid_reads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });
    
    writer.join();
    reader.join();
    
    EXPECT_EQ(valid_reads.load(), kIterations);
}

TEST_F(DoubleBufferTest, ReadAndWriteString) {
    yy::DoubleBuffer<std::string> buffer("init");

    std::atomic<int> valid_reads{0};
    
    std::thread writer([&] {
        for (int i = 0; i < 100; ++i) {
            buffer.write("updated1");
        }
        for (int i = 0; i < 100; ++i) {
            buffer.write("updated2");
        }
    });

    std::vector<std::thread> readers;
    for (int i = 0; i < kMaxThreads; ++i) {
        readers.emplace_back([&] {
            for (int i = 0; i < kIterations / kMaxThreads; ++i) {
                auto val = buffer.read();
                if (val == "init" || val == "updated1" || val == "updated2") {
                    valid_reads.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    writer.join();
    for (auto& t : readers) t.join();

    EXPECT_EQ(valid_reads.load(), kIterations);
}