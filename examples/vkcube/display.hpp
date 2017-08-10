#pragma once

class CubeApplication;

enum display_mode_type {
   DISPLAY_MODE_AUTO = 0,
   DISPLAY_MODE_KMS,
   DISPLAY_MODE_XCB,
};

class VikDisplayMode {
public:
    VikDisplayMode() {}
    ~VikDisplayMode() {}

    virtual int init(CubeApplication* app, VikRenderer *vc) {
	return 0;
    }

    virtual void main_loop(CubeApplication* app, VikRenderer *vc) {}
};
