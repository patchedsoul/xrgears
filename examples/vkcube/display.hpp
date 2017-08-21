#pragma once

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
    VikDisplayMode() {}
    ~VikDisplayMode() {}

    virtual int init(CubeApplication* app, CubeRenderer *vc) {
	return 0;
    }

    virtual void main_loop(CubeApplication* app, CubeRenderer *vc) {}
};
