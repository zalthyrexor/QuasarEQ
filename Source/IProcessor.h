#pragma once

#include <span>

class IProcessor {
public:
  virtual ~IProcessor() = default;
  virtual void process(std::span<float> span) = 0;
};
