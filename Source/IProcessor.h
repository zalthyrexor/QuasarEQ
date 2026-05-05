#pragma once

#include <span>

class IProcessor {
public:
  virtual ~IProcessor() = default;
  virtual void process(std::initializer_list<std::span<float>> spans) = 0;
};
