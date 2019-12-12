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

This library needs a precompiled, statically linked version of v8 to link
against. Currently this is tested against 7.2.502.24. See also *Updating v8*.

It also needs a precompiled, statically linked version of libuv.

# Getting v8 & uv libraries

## Precompiled from DropBox
1. Download the precompiled libs from [DropBox](https://www.dropbox.com/s/d27e0xe4ozytu0l/v8-7.9.317.zip?dl=0)
2. Unpack and copy libs/ into libs/ and include/ into include/

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

##Important notes:
1. Use 64 bit linux, was used ubuntu-18.04.3-live-server-amd64.iso,  `sudo apt-get -y install git cmake python unzip pkg-config clang-8 wget`
2. Do not configure & compile using root
3. Do not configure & compile using script
4. Do not use `d8`, custom `libc++`, manual linking(std namespace must use stable, static ABI, i.e. `::__ndk::` internal namespace. NOT a  `::__::` or  `::__1::` or  `::__Cr::`).
5. Make sure to add/use extra args when configuring & compiling to same `host_cpu` and `target_cpu`, in this example it's `x64`

## Building v8 from sources

We need NDK r20b. We assume it to be in $NDKPATH:

`export ANDROID_NDK_HOME=~/android-ndk-r20b` (or wherever you installed it to). Don't forget to add it to the path:
`export PATH=$PATH:$ANDROID_NDK_HOME`

For steps 1 and 2 see also the [v8 project documentation](https://github.com/v8/v8/wiki/Using%20Git).

1. [get depot_tools](https://www.chromium.org/developers/how-tos/install-depot-tools)
2. Get the sources: `fetch v8`
3. `cd v8`
4. Checkout correct revision: `git checkout 7.9.317.29`
5. `echo "target_os = ['android']" >> ../.gclient && gclient sync --nohooks` as [described on v8 wiki](https://github.com/v8/v8/wiki/D8%20on%20Android)
6. Create GN build configuration for each ABI.
### x64
`gn gen out.gn/x64.release --args='host_cpu="x64" is_clang=true is_component_build=false is_debug=false is_official_build=true strip_debug_info=true symbol_level=0 target_cpu="x64" target_os="android" treat_warnings_as_errors=false v8_enable_i18n_support=false v8_enable_verify_heap=true v8_target_cpu="x64" v8_use_external_startup_data=false use_thin_lto=false use_glib=false android_ndk_root="${ANDROID_NDK_HOME}" libcxx_abi_unstable=false use_sysroot=false use_custom_libcxx=false v8_static_library=true v8_monolithic=true'`
`ninja -C out.gn/x64.release v8_monolith`
### arm
`gn gen out.gn/arm.release --args='host_cpu="x64" is_clang=true is_component_build=false is_debug=false is_official_build=true strip_debug_info=true symbol_level=0 target_cpu="arm" target_os="android" treat_warnings_as_errors=false v8_enable_i18n_support=false v8_enable_verify_heap=true v8_target_cpu="arm" v8_use_external_startup_data=false use_thin_lto=false use_glib=false android_ndk_root="${ANDROID_NDK_HOME}" use_custom_libcxx=false v8_static_library=true v8_monolithic=true'`
`ninja -C out.gn/arm.release v8_monolith`
### arm64
`gn gen out.gn/arm64.release --args='host_cpu="x64" is_clang=true is_component_build=false is_debug=false is_official_build=true strip_debug_info=true symbol_level=0 target_cpu="arm64" target_os="android" treat_warnings_as_errors=false v8_enable_i18n_support=false v8_enable_verify_heap=true v8_target_cpu="arm64" v8_use_external_startup_data=false use_thin_lto=false use_glib=false android_ndk_root="${ANDROID_NDK_HOME}" use_custom_libcxx=false v8_static_library=true v8_monolithic=true'`
`ninja -C out.gn/arm64.release v8_monolith`
### x86
`gn gen out.gn/x86.release --args='host_cpu="x64" is_clang=true is_component_build=false is_debug=false is_official_build=true strip_debug_info=true symbol_level=0 target_cpu="x86" target_os="android" treat_warnings_as_errors=false v8_enable_i18n_support=false v8_enable_verify_heap=true v8_target_cpu="x86" v8_use_external_startup_data=false use_thin_lto=false use_glib=false android_ndk_root="${ANDROID_NDK_HOME}" use_custom_libcxx=false v8_static_library=true v8_monolithic=true'`
`ninja -C out.gn/x86.release v8_monolith`
7. Copy and rename resulting libv8_monolith.a into the folder where ndk-build expects it: `cp libv8_monolith.a $PATH_TO_YOUR_EJECTAV8_CHECKOUT/bgjslibrary/jni/libs/$ABI/libv8.a` where
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
