#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

namespace onc::dsp {

// Single-producer/single-consumer lock-free ring buffer of float samples.
//
// This exists so the real-time audio callback never has to allocate, lock,
// or block: the callback thread is the sole consumer (or producer) and a
// non-real-time thread is the sole producer (or consumer) on the other end.
// Capacity is fixed at construction time; there is no dynamic growth.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacityFrames)
        : capacity_(capacityFrames + 1), // one slot is always kept empty
          buffer_(capacity_, 0.0f) {}

    size_t capacity() const { return capacity_ - 1; }

    // Returns the number of frames actually written (may be less than
    // `count` if the buffer is full). Safe to call only from the producer
    // thread.
    size_t write(const float* data, size_t count) {
        size_t written = 0;
        size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        while (written < count) {
            const size_t nextHead = (head + 1) % capacity_;
            if (nextHead == tail) break; // full
            buffer_[head] = data[written];
            head = nextHead;
            ++written;
        }
        head_.store(head, std::memory_order_release);
        return written;
    }

    // Returns the number of frames actually read (may be less than `count`
    // if the buffer is empty; remaining output is left untouched by the
    // caller's own zero-fill policy). Safe to call only from the consumer
    // thread.
    size_t read(float* out, size_t count) {
        size_t readCount = 0;
        size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        while (readCount < count) {
            if (tail == head) break; // empty
            out[readCount] = buffer_[tail];
            tail = (tail + 1) % capacity_;
            ++readCount;
        }
        tail_.store(tail, std::memory_order_release);
        return readCount;
    }

    size_t availableToRead() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head + capacity_ - tail) % capacity_;
    }

    void reset() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    }

private:
    size_t capacity_;
    std::vector<float> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

} // namespace onc::dsp
