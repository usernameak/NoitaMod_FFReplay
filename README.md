FFReplay - Noita MP4 replay video encoder
=========================================

This mod modifies the built-in replay recorder in Noita, swapping a slow and dumb GIF encoder out for an FFmpeg-based MP4 encoder.

**THIS MOD REQUIRES UNSAFE MODS TO BE ENABLED IN THE GAME**

Building from source
--------------------

Requires Microsoft Visual C++ 2022.

```bash
cd native_source
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFFREPLAY_BUILD_INTO_PARENT_DIR=ON .
cmake --build cmake-build-release
```

