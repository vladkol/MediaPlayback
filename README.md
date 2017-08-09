# MediaPlayback

HEVC URL replacement fix is driven by **LS_HEVC_FIX** preprocessor definition in **pch.h** 

FFMPEG usage and dependencies are driven by **NO_FFMPEG** preprocessor definition in **pch.h** 

If you want to build it with FFMPEG support, first you need to build ffmpeg with OpenSSL support using [vcpkg](https://github.com/Microsoft/vcpkg). 
Due to some [specific changes](https://blogs.msdn.microsoft.com/gillesk/2017/06/21/building-ffmpeg-for-windows-with-httpstls-support/) required, *please use [this private fork of vcpkg](https://github.com/vladkol/vcpkg)*: 

```
git clone https://github.com/vladkol/vcpkg 
cd vcpkg
.\bootstrap-vcpkg.bat (to build the VC package manager)
.\vcpkg integrate install (**from an admin command prompt** to have Visual Studio integration)
.\vcpkg install ffmpeg:x86-uwp ffmpeg:x64-uwp ffmpeg:x86-windows ffmpeg:x64-windows
```
* Open **MediaPlayback/MediaPlayback.sln** 
* Build **Desktop** and **UWP** projects 

If built successfully, **MediaPlayback\Unity\MediaPlayback\** should have all Unity files required.

**Changes in C# files in the demo project stay only there. MediaPlayback solution has its own "master" copy of C# files.**
