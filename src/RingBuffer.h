#pragma once
#include <Arduino.h>

// Fixed-capacity ring of int16 samples with static storage.
//
// Storage is a member array rather than a heap allocation on purpose: the WiFi
// and Bluetooth stacks contend hard for heap, and a 12KB malloc that succeeds at
// boot can fail after a few reconnect cycles. Declare instances as globals.
//
// The writer is the 200Hz sampling task on core 1; readers are the print path
// and the web server on core 0. Callers must hold the lock in Sensor.h while
// snapshotting -- individual int16 reads are atomic but a multi-sample copy is
// not, and without the lock a print can tear across a wrap.
template <int N>
class RingBuffer {
 public:
  void push(int16_t v) {
    buf_[head_] = v;
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }

  int size() const { return count_; }
  static constexpr int capacity() { return N; }
  bool full() const { return count_ == N; }

  // age 0 is the newest sample, age 1 the one before it, and so on.
  int16_t recent(int age) const {
    if (age < 0 || age >= count_) return 0;
    int idx = head_ - 1 - age;
    while (idx < 0) idx += N;
    return buf_[idx];
  }

  // Copies the newest `n` samples into `out` in chronological order, so
  // out[0] is the oldest of the run and out[n-1] the newest. Returns the count
  // actually written, which is less than n if the buffer has not filled yet.
  int copyNewest(int n, int16_t* out) const {
    if (n > count_) n = count_;
    for (int i = 0; i < n; i++) {
      out[i] = recent(n - 1 - i);
    }
    return n;
  }

  void clear() {
    head_ = 0;
    count_ = 0;
  }

 private:
  int16_t buf_[N] = {0};
  int head_ = 0;
  int count_ = 0;
};
