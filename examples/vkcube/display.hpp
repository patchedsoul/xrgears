#pragma once

class VikDisplayMode {
public:
    VikDisplayMode() {}
    ~VikDisplayMode() {}

    virtual int init(struct vkcube *vc) {
	return 0;
    }

    virtual void main_loop(struct vkcube *vc) {}
};
