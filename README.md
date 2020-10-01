# DSPManager Lib

This lib source is from Cyanogenmod OS's DSPManager.
But the old source ver can not be compiled for new ROM source,
so I try my best to solve it. But I don't know what do some functions
mean... Well, this source can be compiled and works on Android 4.0 ~ 10.0.

# DSPFloat

This branch is named 'DSPFloat', and it supports to above android 9.0.
On android 9 or above, the audio is used PCMFloat by default, and I don't
know how to convert it to PCM16.

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

