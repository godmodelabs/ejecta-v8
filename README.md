# About

This library ports the excellent EjectaJS
(https://github.com/phoboslab/Ejecta) to Android, replacing
JavascriptCore with v8 in the process. Think of it as a Javascript engine
with Canvas and AJAX but no DOM. If you want fast rendering of a
cross-platform Canvas-based JS application this is for - if you have a web
application or your JS code accesses the DOM in any way a WebView will be a
better choice.

Ejecta-v8 is published under the MIT Open Source License (http://opensource.org/licenses/mit-license.php), as is Ejecta.

You can use our sample app under https://github.com/godmodelabs/ejecta-v8-sample
to get a feel for the library.

This library needs a precompiled, statically linked version of v8 and libuv to link
against. Currently this is tested against 6.5.254.28 (the version used in Chrome 65). See also *Updating v8*.

# Getting v8 & uv libraries

## Precompiled from dropbox
1. Download the precompiled libs from [CloudApp](https://cl.ly/db30380c3537)
2. Unpack into libs/

The libs/v8 folder should look like this:
```bash
# pwd
~/git/ejecta-v8/libs/v8
# ls
arm64-v8a   armeabi-v7a x86         x86_64
```

The folders in livs/uv should be the same.

## Building libuv from sources

Coming soon.

## Building v8 from sources

We need NDK r15c. We assume it to be in $NDKPATH:

`export ANDROID_NDK_HOME=~/bin/android-ndk-r14b` (or wherever you installed it to). Don't forget to add it to the path:
`export PATH=$PATH:$ANDROID_NDK_HOME`

For steps 1 and 2 see also the [v8 project documentation](https://github.com/v8/v8/wiki/Using%20Git).

1. [get depot_tools](https://www.chromium.org/developers/how-tos/install-depot-tools)
2. Get the sources: `fetch v8`
3. `cd v8`
4. Checkout correct revision: `git checkout origin/6.5.254.28`
5. `echo "target_os = ['android']" >> ../.gclient && gclient sync --nohooks` as [described on v8 wiki](https://github.com/v8/v8/wiki/D8%20on%20Android)
6. If compiling on Linux, add missing deps: `sudo apt-get install libc6-dev-i386 g++-multilib` (because compiling errors with some missed libraries)
7. `mkdir out.gn`
8. Create GN build configuration for each ABI. Here for a debug armv7a build: `gn gen out.gn/arm.debug --args='is_debug=true is_clang=true is_component_build=true target_os="android" v8_enable_i18n_support=false v8_target_cpu="arm" target_cpu="arm" host_cpu="x64" v8_use_snapshot=false symbol_level=1 v8_enable_verify_heap=true'`
..* For a release x86 (32 bit) build with a custom NDK (I couldn't successfully compile with the patched NDK shipped with v8): gn gen out.gn/x86.release --args='is_debug=false is_clang=true is_component_build=true target_os="android" v8_enable_i18n_support=false v8_target_cpu="x86" target_cpu="x86" host_cpu="x64" android_ndk_version="r15c" android_ndk_major_version=15 v8_use_snapshot=false v8_enable_verify_heap=true android_ndk_root="$ANDROID_NDK_HOME"
..* You can replace arm with arm64, x86 or x64.
9. Build with ninja: `ninja -C out.gn/arm.debug d8`
10. Go into output folder: `cd out.gn/arm.debug/obj`
11. Create a static archive from the resulting object files: `$ANDROID_NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ar rcvs libv8.a v8/*.o v8_base/*.o v8_libbase/*.o v8_libsampler/*.o v8_nosnapshot/*.o v8_libplatform/*.o v8_libsampler/*.o v8_init/*.o v8_initializers/*.o`
..* For arm64, use `$ANDROID_NDK_HOME/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-ar rcvs libv8.a v8/*.o v8_base/*.o v8_libbase/*.o v8_libsampler/*.o v8_nosnapshot/*.o v8_libplatform/*.o v8_libsampler/*.o v8_init/*.o v8_initializers/*.o`
..* For x86 use `$ANDROID_NDK_HOME/toolchains/x86-4.9/prebuilt/linux-x86_64/bin/i686-linux-android-ar rcvs libv8.a v8/*.o v8_base/*.o v8_libbase/*.o v8_libsampler/*.o v8_nosnapshot/*.o v8_libplatform/*.o v8_libsampler/*.o v8_init/*.o v8_initializers/*.o`
..* For X86-64 use `$ANDROID_NDK_HOME/toolchains/x86_64-4.9/prebuilt/linux-x86_64/bin/x86_64-linux-android-a rcvs libv8.a v8/*.o v8_base/*.o v8_libbase/*.o v8_libsampler/*.o v8_nosnapshot/*.o v8_libplatform/*.o v8_libsampler/*.o v8_init/*.o v8_initializers/*.o`
12. Optionally strip debug symbols. I have never gotten meaningful stack
traces out of these anyway, and it speeds linking times up considerably and
saves about one GB of space per ABI: `$ANDROID_NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip --strip-debug libv8.a`
13. Copy resulting libv8.a into the folder where ndk-build expects it: `cp
libv8.a $PATH_TO_YOUR_EJECTAV8_CHECKOUT/bgjslibrary/jni/libs/$ABI` where
$ABI is either armeabi-v7a, arm64-v8a, x86 or x86_64.

Steps 8-14 can be repeated as mentioned for each ABI you want to support. Step 8 can set is_debug to false of course. 

# Building the sample

Set up at least NDK r15c and make sure it is in the search path.

./gradlew assembleDebug

# Updating v8

Find good v8 version: [Gist that explains
it](https://gist.github.com/domenic/aca7774a5d94156bfcc1).
See the [list of v8 release information](https://omahaproxy.appspot.com/)


# ARMv7 NEON

I opted to compile the arm version with support for NEON instructions, as non-NEON devices are becoming very rare. Make sure to either compile
without this option if supporting Tegra 2 is important to you, or disable these devices in the Play Store:
[List of non-neon devices](https://forum.unity3d.com/threads/failure-to-initialize-your-hardware-does-not-support-this-application-sorry.311613/#post-2139480)
