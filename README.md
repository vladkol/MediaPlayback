 # Media Playback for Unity on Windows 10

Media Playback plugin for [Unity](https://unity3d.com/) on Windows 10 Fall Creators Update. 

It supports a bread range of media playback features: 
* Local files, progressive and Adaptive Streaming (HLS, DASH) 
* Regular and 360 videos, Stereoscopic (3D) and monoscopic  
* Subtitles 

The plugin is built on top of [MediaPlayer](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/play-audio-and-video-with-mediaplayer) Universal Windows Platform API. 
Primarily targeting [Windows Mixed Reality](https://developer.microsoft.com/en-us/windows/mixed-reality/mixed_reality), it also supports Windows Standalone (desktop) apps built with Unity. 
  
MediaPlaybackDemo is a demo project covering 3 key scenarios: 
* Regular video playback with Adaptive Streaming, MediaPlayback.unity scene.  
* 360 video playback (mono), MediaPlayback360Mono.unity scene. 
* 360 video playback (stereo), MediaPlayback360Stereo.unity scene.  
The demo project already has all plugin binaries prebuilt.  

Supported Unity versions: 
* 5.6.3 
* 2017.2.1 
* 2017.3 (preliminary) 

## How to use 
1. Download MediaPlaybackDemo release package or open MediaPlaybackDemo project from a cloned repo.
2. Look how MediaPlayback.unity and MediaPlayback360Stereo.unity are structured. If you just want to play a video in your scene, use Playback and MediaPlaybackRunner components. 

## How to build
For building the plugin, use [Visual Studio 2017](https://www.visualstudio.com/downloads/) with Windows Desktop, Universal Windows Platform and C++ toolsets installed. It also requires [Windows 10 Fall Creators update SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk).

* Open **MediaPlayback/MediaPlayback.sln** 
* Build **Desktop** and **UWP** projects 

If built successfully, **MediaPlayback\Unity\MediaPlayback\** should have all Unity files required. *CopyMediaPlaybackDLLsToDemoProject.cmd* script copies plugin binary files to the demo project.

**Changes in C# files in the demo project (MediaPlaybackDemo) stay only there. MediaPlayback solution has its own "master" copy of C# files (UnityAddonFiles solution folder).** 
If you change files in either location, make sure you copy them across both folders. 