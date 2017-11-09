# MediaPlayback

Media Playback plugin for [Unity](https://unity3d.com/). 

It supports Adaptive Streaming (HLS, DASH), subtitles, stereoscopic 360 video playback, and other features provided by [Universal Windows Platform](https://docs.microsoft.com/en-us/windows/uwp/get-started/universal-application-platform-guide). The plugin is built on top of [MediaPlayer](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/play-audio-and-video-with-mediaplayer) Universal Windows Platform API.

It may use FFMPEG as streamning and decoding backend. FFMPEG usage and dependencies are driven by **NO_FFMPEG** preprocessor definition in **pch.h**. 
FFMPEG usage is **off** by default. 

If you want to build it with FFMPEG support, first you need to build ffmpeg with OpenSSL support using [vcpkg](https://github.com/Microsoft/vcpkg): 
```
git clone https://github.com/Microsoft/vcpkg 
cd vcpkg
.\bootstrap-vcpkg.bat (to build the VC package manager)
.\vcpkg integrate install (**from an admin command prompt** to have Visual Studio integration)
.\vcpkg install ffmpeg:x86-uwp ffmpeg:x64-uwp ffmpeg:x86-windows ffmpeg:x64-windows
```
* Open **MediaPlayback/MediaPlayback.sln** 
* Build **Desktop** and **UWP** projects 

MediaPlaybackDemo Unity project has all binaries precompiled (no FFMPEG binaries though).

If built successfully, **MediaPlayback\Unity\MediaPlayback\** should have all Unity files required.

**Changes in C# files in the demo project (MediaPlaybackDemo) stay only there. MediaPlayback solution has its own "master" copy of C# files (UnityAddonFiles solution folder).**
