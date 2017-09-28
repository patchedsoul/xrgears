#pragma once

#include "vikRendererVks.hpp"

namespace vik {

class RendererTextOverlay : public RendererVks {
public:
  RendererTextOverlay(Settings *s, Window *w) : RendererVks(s, w) {}
  ~RendererTextOverlay() {}
};
}
