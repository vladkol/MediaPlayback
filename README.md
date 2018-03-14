 # Windows 10 Media Playback for Unity 1.5 (alpha)

Media Playback plugin for [Unity](https://unity3d.com/) on Windows 10 Fall Creators Update and beyond. 

It gives you access to a broad range of media playback features: 
* Local files, progressive and Adaptive Streaming (HLS, DASH) playback 
* Regular and 360 videos, stereoscopic (3D) and monoscopic   
* All formats, codecs and media containers [supported by Windows 10](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/supported-codecs#video-codec--format-support) 
* Subtitles 
* Ambisonic Audio 

The plugin is built on top of [MediaPlayer](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/play-audio-and-video-with-mediaplayer) Universal Windows Platform API. 
Primarily targeting [Windows Mixed Reality](https://developer.microsoft.com/en-us/windows/mixed-reality/mixed_reality), it also supports Windows Standalone (desktop) apps built with Unity. 
  
MediaPlaybackUnity is a Unity project covering 3 key scenarios: 
* Regular video playback with Adaptive Streaming, MediaPlayback.unity scene  
* 360 video playback (stereo/mono), MediaPlayback360.unity scene
* 360 video playback (stereo/mono) on Skybox, Skyboz360Stereo.unity scene 
There is also a demo scene showing how to play video files from so called 'known folders' on Universal Windows Platform (Video library in this case). The scene is in MediaPlaybackUnity\Assets\Scenes\UWPTest folder. 

Supported Unity versions: 
* 5.6.3 
* 2017.2.1
* 2017.3.x

## New in this version (1.5 alpha) 
* The texture pipeline has been redesigned for supporting Stereoscopic videos with correspondding metadata 
* New 360VideoShader (360 Video/360 XR Stereo Panorama) for rendiring monoscopic and stereoscopic videos in stereoscopic mode on a single mesh for both eyes in Mixed Reality  
* Skybox rendering support, including new stereoscopic 360VideoSkyboxShader for Skybox (360 Video/360 XR Skybox)
* Ambisonic audio support on Windows 10 "RS4" 

## Breaking Changes in version 1.5 
* The plugin now recreates the playback texture every time frame resolution changes. We added TextureUpdated event for you to handle these changes. 
* Playback component doesn't adjust material's shader parameters related to the frame layout and aspecy ratio automatically. It's not supposed to be handled in shaders. 
* The plugin now detects sterescopic videos based on the metadata (ST3D box). Once detected, we render all frames to the video texture in over/under layout. 

## Properties and events 
* Renderer targetRenderer - Renderer component to the object the frame will be rendered to. If null (none), other paramaters are ignored - you are expected to handle texture changes in TextureUpdated event handler. 
* string targetRendererTextureName - Texture to update on the Target Renderer (must be material's shader variable name). If empty, and targetRenderer is not null, mainTexture will be updated 
* string isStereoShaderParameterName - If material's shader has a variable that handles stereoscopic vs monoscopic video, put its name here (must be a float, 0 - monoscopic, 1 - stereoscopic). 
* public bool forceStereo - If true, the material's shader will be forced to render frames as stereoscopic (assuming isStereoShaderParameterName is not empty) 
* forceStationaryXROnPlayback - switches to XR Stationary tracking mode, and resets the rotation when starts playing a video. Once playback stops, switches back to RoomScale if that mode was active before the playback 

### Runtime properties 
* bool isStereo - true if current video is detected as stereoscopic by its metadata (ST3D box). **forceStereo doesn't affect this property** 
* hardware4KDecodingSupported - true if hardware video decoding is supported for resolutions 4K and higher 
* uint currentPlaybackTextureWidth / currentPlaybackTextureHeight - current frame resolution 
* Texture2D currentVideoTexture - current video texture 
* PlaybackState State - current playback state 

### Events 
* TextureUpdated (object sender, Texture2D newVideoTexture, bool isStereoscopic) - video texture has been updated. isStereoscopic is true if either the video is stereoscopic by its metadata, **or forceStreo is true** 
* PlaybackStateChanged (object sender, ChangedEventArgs<PlaybackState> args) - playback state has been changed 
* PlaybackFailed (object sender, long hresult) - playback faled 
* SubtitleItemEntered (object sender, string subtitlesTrackId, string textCueId, string language, string[] textLines) - text subtitle cue entered (must be shown) 
* SubtitleItemExited (object sender, string subtitlesTrackId, string textCueId) - text subtitle cue exited (must be hidden) 

## How to use 
1. Download [MediaPlaybackDemo release package](https://github.com/vladkol/MediaPlayback/releases) or open MediaPlaybackUnity project from a cloned repo.
2. Look how MediaPlayback.unity and MediaPlayback360.unity are structured. If you just want to play a video in your scene, use Playback and MediaPlaybackRunner components. 

## How to build
Unity project already has all plugin binaries prebuilt. 
For building the plugin, use [Visual Studio 2017](https://www.visualstudio.com/downloads/) with Windows Desktop, Universal Windows Platform and C++ toolsets installed. It also requires [Windows 10 Fall Creators update SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk).

* Open **MediaPlayback/MediaPlayback.sln** 
* Build **Desktop** and **UWP** projects 

If built successfully, **MediaPlayback\Unity\MediaPlayback\** should have all Unity files required. *CopyMediaPlaybackDLLsToUnityProject.cmd* script copies plugin binary files to Unity project's Plugins folder.

## Rendering stereoscopic videos 
360VideoShader and 360VideoSkyboxShader are based on Unity's [SkyboxPanoramicShader](https://github.com/Unity-Technologies/SkyboxPanoramicShader). 
They currently dont't support 180-degree videos. 

If you want to render to Skybox, handle TextureUpdated event on Playback object. Look at MediaPlaybackUnity/Assets/MediaPlayback/Scrips/MediaSkybox.cs script. 
There is a sample scene for Skybox rendering, in MediaPlaybackUnity/Assets/Scenes. 
The video texture is Y-flipped, make sure you handle it in the shader. 

When rendering stereoscopic videos, the native plugin forces over/under frame layout, so the video texture always comes to the shader as over/under frame. 
In your custom shaders, if you want to handle 180-degree videos or single-frame cubemaps, they all usually have no corresponding metadata, and must be handled in the shader based on the custom medatada. 
