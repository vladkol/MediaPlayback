 # Windows 10 Media Playback for Unity 1.5 (alpha)

Media Playback plugin for [Unity](https://unity3d.com/) on Windows 10 Fall Creators Update and beyond. 

It gives you access to a broad range of media playback features: 
* Local files, progressive and Adaptive Streaming (HLS, DASH) playback 
* Regular and 360 videos, stereoscopic (3D) and monoscopic   
* All formats, codecs and media containers [supported by Windows 10](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/supported-codecs#video-codec--format-support) 
* Subtitles 
* Ambisonic Audio 

## New in this version (1.5 alpha): 
* The texture pipeline has been redesigned for supporting Stereoscopic videos with correspondding metadata 
* New 360VideoShader for rendiring monoscopic and stereoscopic videos in stereoscopic mode on a single mesh for both eyes in Mixed Reality  
* Ambisonic audio support on Windows 10 "RS4" 

The plugin is built on top of [MediaPlayer](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/play-audio-and-video-with-mediaplayer) Universal Windows Platform API. 
Primarily targeting [Windows Mixed Reality](https://developer.microsoft.com/en-us/windows/mixed-reality/mixed_reality), it also supports Windows Standalone (desktop) apps built with Unity. 
  
MediaPlaybackUnity is a Unity project covering 3 key scenarios: 
* Regular video playback with Adaptive Streaming, MediaPlayback.unity scene.  
* 360 video playback (mono), MediaPlayback360.unity scene. 
The demo project already has all plugin binaries prebuilt.  

Supported Unity versions: 
* 5.6.3 
* 2017.2.1
* 2017.3.x

## How to use 
1. Download [MediaPlaybackDemo release package](https://github.com/vladkol/MediaPlayback/releases) or open MediaPlaybackDemo project from a cloned repo.
2. Look how MediaPlayback.unity and MediaPlayback360Stereo.unity are structured. If you just want to play a video in your scene, use Playback and MediaPlaybackRunner components. 

## How to build
For building the plugin, use [Visual Studio 2017](https://www.visualstudio.com/downloads/) with Windows Desktop, Universal Windows Platform and C++ toolsets installed. It also requires [Windows 10 Fall Creators update SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk).

* Open **MediaPlayback/MediaPlayback.sln** 
* Build **Desktop** and **UWP** projects 

If built successfully, **MediaPlayback\Unity\MediaPlayback\** should have all Unity files required. *CopyMediaPlaybackDLLsToUnityProject.cmd* script copies plugin binary files to Unity project's Plugins folder.

360VideoShader is based on Unity's [SkyboxPanoramicShader](https://github.com/Unity-Technologies/SkyboxPanoramicShader). 
360VideoShader currently doesn't support 180-degree videos. 

If you want to render to Skybox, handle TextureUpdated event on Playback object: 
``` csharp
		// subscribe to TextureUpdated 
        var player = GetComponent<MediaPlayer.Playback>();
        player.TextureUpdated += Player_TextureUpdated;

	    private void Player_TextureUpdated(object sender, Texture2D newVideoTexture, bool isStereoscopic)
		{
			Material skyboxMaterial = RenderSettings.skybox;

			// Update your skybox here! 
			// if isStereoscopic is true, assume over/under frame layout 
			// You may need to call DynamicGI.UpdateEnvironment() after that (https://docs.unity3d.com/ScriptReference/RenderSettings-skybox.html)
		}
```

When rendering stereoscopic videos, the native plugin forces over/under frame layout, so the video texture always comes to the shader as over/under frame. 
In your custom shaders, if you want to handle 180-degree videos or single-frame cubemaps, they all usually have no corresponding metadata, and must be handled in the shader based on the custom medatada. 