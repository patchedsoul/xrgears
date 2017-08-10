#pragma once

enum display_mode_type {
   DISPLAY_MODE_AUTO = 0,
   DISPLAY_MODE_KMS,
   DISPLAY_MODE_XCB,
};

class VikDisplayMode {
public:
    VikDisplayMode() {}
    ~VikDisplayMode() {}



    virtual int init(struct vkcube *vc) {
	return 0;
    }

    virtual void main_loop(struct vkcube *vc) {}
};
