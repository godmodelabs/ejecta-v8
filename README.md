ejecta-v8
=========

This is a port of @phoboslabs EjectaJS to C++ which uses v8 instead of JavaScriptCore. 
Think of it as a simple browser. It only offers Canvas and a simple AJAX interface in Javascript,
but it is super fast and embeddable. No HTML, DOM or CSS is included - it is for JS only.

The original (non-JIT, iOS-only) code can be found at https://github.com/phoboslab/ejecta.
This port focuses on Android for now but could easily run on all OS that support OpenGL or OpenGL ES.

We have included pre-built shared libraries for v8, as compiling for Android can be a hassle. Further work is needed
to support newer versions of v8, as many things have changed there (see TODO).

You can download the sample app from Play Store: https://play.google.com/store/apps/details?id=ag.boersego.bgjs.sample


Building
--------
You need gradle and the NDK to build the library and the sample. To build the library do

cd library
ndk-build
gradle assembleX86Debug
gradle assembleArmDebug

Then you can build the sample.

To-Do
-----
*Fix support for newer versions of v8
*Enable loading of more than one script file!
*Get rid of android.js
