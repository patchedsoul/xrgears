![Screenshot](https://gitlab.com/lubosz/xrgears/raw/master/doc/screenshot.jpg "Screenshot")

# xrgears

xrgears is a stand alone VR demo using OpenHMD for tracking and Vulkan
with several window system back ends for rendering.

The demo is able to utilize full screen extended mode on XCB and 2 Wayland backends using different protocols.

Direct Mode is achieved through drm-lease utilizing `VK_EXT_direct_mode_display` and related extensions.

It can also run directly on KMS, which is currently just implemented for Intel and could be impossible on the current release due to regression issues in mesa. The KMS back end is taken from [vkcube](https://github.com/krh/vkcube).

This project is a highly modified fork of [Sascha Willem's excellent Vulkan Samples](https://github.com/SaschaWillems/Vulkan).

The Vulkan code is organized in a header only C++ library, which is named `vitamin-k`.

# Dependencies

OpenHMD, Vulkan, Assimp, GLM / GLI

## Platform libraries required for the various back ends

### XCB
xcb, xcb-keysyms, xcb-randr

### Wayland
wayland-client,wayland-scanner,wayland-protocols

### DRM
gbm, libdrm

# Clone

This repo needs to be cloned recursively

```
git clone --recursive URL
```

If you forgot that you can fetch the assets module like so

```
git submodule init
git submodule update
```

# Build

```
cmake .
make -j9
```

Meson build WIP.

# Commands

```
Options:
  -s, --size WxH           Size of the output window (default: 1280x720)
  -f, --fullscreen         Run fullscreen. Optinally specify display and mode.
  -d, --display D          Display to fullscreen on. (default: 0)
  -m, --mode M             Mode for fullscreen (used in wayland-shell only) (default: 0)
  -w, --window WS          Window system to use (default: auto)
                           [xcb, wayland, wayland-shell, kms, direct]
  -g, --gpu GPU            GPU to use (default: 0)
      --hmd HMD            HMD to use (default: 0)
      --format F           Color format to use (default: VK_FORMAT_B8G8R8A8_UNORM)
      --presentmode M      Present mode to use (default: VK_PRESENT_MODE_FIFO_KHR)

      --list-gpus          List available GPUs
      --list-displays      List available displays
      --list-hmds          List available HMDs
      --list-formats       List available color formats
      --list-presentmodes  List available present modes

      --disable-overlay    Disable text overlay
      --mouse-navigation   Use mouse instead of HMD for camera control.
      --distortion         HMD lens distortion (default: panotools)
                           [none, panotools, vive]
  -v, --validation         Run Vulkan validation
  -h, --help               Show this help
```

# Examples

### Run on XCB display 1 in full screen
```
./bin/xrgears -w xcb -d 1
```

### Run on wayland, using VIVE1 distortion shader
```
./bin/xrgears -w wayland --distortion vive
```

### Enumerate displays with XCB for direct backend

```
./bin/xrgears --list-displays --window direct
```

### Run on first non-desktop output in direct mode with VIVE1 distortion.
```
./bin/xrgears --window direct --distortion vive
```

### Run cube example using the format VK_FORMAT_B8G8R8A8_SRGB
```
./bin/cube --format VK_FORMAT_B8G8R8A8_SRGB
```

### Enumerate available present modes
```
./bin/cube --list-presentmodes
```

# License
MIT
