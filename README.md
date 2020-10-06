# DSPManager Lib

This lib source is from Cyanogenmod OS's DSPManager.
But the old source ver can not be compiled for new ROM source,
so I try my best to solve it. But I don't know what do some functions
mean... Well, this source can be compiled and works on Android 4.0 ~ 8.0.

# DSPInt

This branch is named 'DSPInt', and it supports to below android 9.0.
On android 8 or below, the audio is used PCM16 by default.
Actually DSPFloat aslo support for below 9 version, but the effect may not
the same as DSPInt.

# Compile it

Well I just wrote down how i complied it

You need to place these files to ROM source

1. Create a directory named like "DSPManager"
2. Place the source files to the folder you just created
3. Move the folder to ROMSource/packages/apps/
4. Open terminal and cd to your ROMSource
5. Setup your compiling env: . build/envsetup.sh
6. Use 'lunch' command to setup your device(maybe you need a device tree)
7. Then use 'mmm' command: mma ./packages/apps/"The floder name you just moved"
8. Wait till the end

