#pragma once

#include <array>
#include <JuceHeader.h>
#include <span>

template <typename T, int Capacity = 32>
struct Fifo {
  T* getWriteBuffer() {
    auto write = fifo.write(1);
    if (write.blockSize1 > 0) {
      return &buffers[write.startIndex1];
    }
    return nullptr;
  }
  void finishedWrite() {
    fifo.finishedWrite(1);
  }
  T* getReadBuffer() {
    auto read = fifo.read(1);
    if (read.blockSize1 > 0) {
      return &buffers[read.startIndex1];
    }
    return nullptr;
  }
  void finishedRead() {
    fifo.finishedRead(1);
  }
  int getNumAvailableForReading() const {
    return fifo.getNumReady();
  }
  T& getBufferAt(int index) {
    return buffers[index];
  }
private:
  std::array<T, Capacity> buffers;
  juce::AbstractFifo fifo {Capacity};
};

template <typename T, int Capacity>
struct SimpleFifo {
  void prepare(int size) {
    for (auto& buffer : buffers) {
      buffer.assign(size, 0.0f);
    }
    fifo.reset();
  }
  bool push(const std::vector<T>& data) {
    auto write = fifo.write(1);
    if (write.blockSize1 > 0) {
      buffers[write.startIndex1] = data;
      return true;
    }
    return false;
  }
  bool pull(std::vector<T>& target) {
    auto read = fifo.read(1);
    if (read.blockSize1 > 0) {
      target = buffers[read.startIndex1];
      return true;
    }
    return false;
  }
  int getNumAvailable() const {
    return fifo.getNumReady();
  }

private:
  std::array<std::vector<T>, Capacity> buffers;
  juce::AbstractFifo fifo {Capacity};
};

struct SampleFifo {
  void prepare(int bufferSize) {
    samplesToFill.assign(bufferSize, 0.0f);
    fifo.prepare(bufferSize);
    writeIndex = 0;
  }
  void update(std::span<const float> source) {
    for (const float sample : source) {
      if (writeIndex >= samplesToFill.size()) {
        fifo.push(samplesToFill);
        writeIndex = 0;
      }
      samplesToFill[writeIndex++] = sample;
    }
  }
  int getNumAvailable() const {
    return fifo.getNumAvailable();
  }
  bool pull(std::vector<float>& target) {
    return fifo.pull(target);
  }
private:
  int writeIndex = 0;
  std::vector<float> samplesToFill;
  SimpleFifo<float, 16> fifo;
};
