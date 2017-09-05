#pragma once

#include <string>

namespace vkc {
class CubeApplication;
class CubeRenderer;

enum display_mode_type {
  DISPLAY_MODE_AUTO = 0,
  DISPLAY_MODE_KMS,
  DISPLAY_MODE_XCB,
  DISPLAY_MODE_WAYLAND,
};

class VikDisplayMode {
public:
  std::string name;

  VikDisplayMode() {}
  virtual ~VikDisplayMode() {}

  virtual int init(CubeApplication* app, CubeRenderer *vc) = 0;
  virtual void loop(CubeApplication* app, CubeRenderer *vc) = 0;
};
}
