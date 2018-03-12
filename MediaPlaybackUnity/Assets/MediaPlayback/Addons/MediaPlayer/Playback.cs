//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace MediaPlayer
{
    public enum PlaybackState
    {
        None = 0,
        Opening,
        Buffering,
        Playing,
        Paused,
        Ended,
        NA = 255
    };

    public class ChangedEventArgs<T>
    {
        public T PreviousState;
        public T CurrentState;

        public ChangedEventArgs(T previousState, T currentState)
        {
            this.PreviousState = previousState;
            this.CurrentState = currentState;
        }
    }

    public enum TargetRenderingMode
    {
        Mono = 0, 
        Stereo,
    }

    public class TargetRenderer
    {
        public Renderer targetRenderer;
        public bool isStereo = false;
        public bool forceStereo = false;

        public string mainTextureName = "_MainTex";
        public string isStereoParameterName = "_isStereo";
    }

    public class Playback : MonoBehaviour
    {
        // state handling
        public delegate void PlaybackStateChangedHandler(object sender, ChangedEventArgs<PlaybackState> args);
        public delegate void PlaybackFailedHandler (object sender, long hresult);
        public delegate void TextureUpdatedHandler(object sender);
        public delegate void SubtitleItemEnteredHandler(object sender, string subtitlesTrackId, string textCueId, string language, string[] textLines);
        public delegate void SubtitleItemExitedHandler(object sender, string subtitlesTrackId, string textCueId);

        public event PlaybackStateChangedHandler PlaybackStateChanged;
        public event PlaybackFailedHandler PlaybackFailed;
        public event TextureUpdatedHandler TextureUpdated;
        public event SubtitleItemEnteredHandler SubtitleItemEntered;
        public event SubtitleItemExitedHandler SubtitleItemExited;

        public Renderer targetRenderer;
        public string targetRendererTextureName = "_MainTex";
        public string isStereoShaderParameterName = "_isStereo";
        public bool forceStereo = false;

        public bool isStereo
        {
            get
            {
                return isStereoVideo;
            }
        }

        public bool hardware4KDecodingSupported
        {
            get
            {
                return hw4KDecodingSupported;
            }
        }

        // texture size
        public uint currentPlaybackTextureWidth
        {
            get
            {
                return textureWidth;
            }
        }
        public uint currentPlaybackTextureHeight
        {
            get
            {
                return textureHeight;
            }
        }


        [Tooltip("Recenters the camera when playback starts.")]
        public bool forceStationaryXROnPlayback = true;


        public PlaybackState State
        {
            get { return this.currentState; }
            private set
            {
                if (this.currentState != value)
                {
                    this.previousState = this.currentState;
                    this.currentState = value;
                    var args = new ChangedEventArgs<PlaybackState>(this.previousState, this.currentState);

#if UNITY_WSA_10_0
                    if (!UnityEngine.WSA.Application.RunningOnAppThread())
                    {
                        UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                        {
                            TriggerPlaybackStateChangedEvent(args);

                        }, false);
                    }
                    else
                    {
                        TriggerPlaybackStateChangedEvent(args);
                    }
#else
                    TriggerPlaybackStateChangedEvent(args);
#endif
                }
            }
        }


        private uint textureWidth = 0;
        private uint textureHeight = 0;
        private Texture2D playbackTexture = null;
        private bool needToUpdateTexture = false;

        private bool isStereoVideo = false;

        private IntPtr pluginInstance = IntPtr.Zero;
        private GCHandle thisObject;
        private bool hw4KDecodingSupported = true;

        private string currentItem = string.Empty;

        private PlaybackState currentState = PlaybackState.None;
        private PlaybackState previousState = PlaybackState.None;

        private Plugin.StateChangedCallback stateCallback = new Plugin.StateChangedCallback(MediaPlayback_Changed);
        private Plugin.SubtitleItemEnteredCallback subtitleEnteredCallback = new Plugin.SubtitleItemEnteredCallback(MediaPlayback_SubtitleItemEntered);
        private Plugin.SubtitleItemExitedCallback subtitleExitedCallback = new Plugin.SubtitleItemExitedCallback(MediaPlayback_SubtitleItemExited);

        private bool loaded = false;
        private Plugin.MEDIA_DESCRIPTION currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();

        private bool needToGoBackToRoomScale = false;

        public void Load(string uriOrPath)
        {
            Stop();

            string uriStr = uriOrPath.Trim();

            if (uriStr.ToLower().StartsWith("file:///") || Uri.IsWellFormedUriString(uriOrPath, UriKind.Absolute))
            {
                uriStr = uriOrPath;
            }
            else if (System.IO.Path.IsPathRooted(uriOrPath))
            {
                uriStr = "file:///" + uriOrPath;
            }
            else
            {
                uriStr = "file:///" + System.IO.Path.Combine(Application.streamingAssetsPath, uriOrPath);
            }


            needToGoBackToRoomScale = false;
            bool isXR =
#if UNITY_2017_2_OR_NEWER
                UnityEngine.XR.XRDevice.isPresent;
#else
                UnityEngine.VR.VRDevice.isPresent;
#endif
            if(isXR && forceStationaryXROnPlayback)
            {
#if UNITY_2017_2_OR_NEWER
                needToGoBackToRoomScale = UnityEngine.XR.XRDevice.GetTrackingSpaceType() == UnityEngine.XR.TrackingSpaceType.RoomScale;
                UnityEngine.XR.XRDevice.SetTrackingSpaceType(UnityEngine.XR.TrackingSpaceType.Stationary);
#else
                needToGoBackToRoomScale = UnityEngine.VR.VRDevice.GetTrackingSpaceType() == UnityEngine.VR.TrackingSpaceType.RoomScale;
                UnityEngine.VR.VRDevice.SetTrackingSpaceType(UnityEngine.VR.TrackingSpaceType.Stationary);
#endif
            }

            bool isStationaryXR = isXR & 
#if UNITY_2017_2_OR_NEWER
                UnityEngine.XR.XRDevice.GetTrackingSpaceType() == UnityEngine.XR.TrackingSpaceType.Stationary;
#else
                UnityEngine.VR.VRDevice.GetTrackingSpaceType() == UnityEngine.VR.TrackingSpaceType.Stationary;
#endif
            if (isStationaryXR && forceStationaryXROnPlayback)
            {
#if UNITY_2017_2_OR_NEWER
                UnityEngine.XR.InputTracking.Recenter();
#else
                UnityEngine.VR.InputTracking.Recenter();
#endif
            }

            loaded = (0 == CheckHR(Plugin.LoadContent(pluginInstance, uriStr)));
            if (loaded)
            {
                currentItem = uriOrPath;
            }
        }

        public void Play()
        {
            Play(null); // play or resume already loaded item
        }

        public void Play(string itemToPlay)
        {
            string item = string.IsNullOrEmpty(itemToPlay) ? string.Empty : itemToPlay.Trim();

            if (!string.IsNullOrEmpty(item) && currentItem != item)
            {
                Load(item);
            }

            if (loaded)
            {
                CheckHR(Plugin.Play(pluginInstance));
            }
        }


        public void Pause()
        {
            CheckHR(Plugin.Pause(pluginInstance));
        }

        public void Stop()
        {
            Plugin.Stop(pluginInstance);
            currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
            State = PlaybackState.None;
            currentItem = string.Empty;

            if (needToGoBackToRoomScale)
            {
                needToGoBackToRoomScale = false;
#if UNITY_2017_2_OR_NEWER
                UnityEngine.XR.InputTracking.Recenter();
                UnityEngine.XR.XRDevice.SetTrackingSpaceType(UnityEngine.XR.TrackingSpaceType.RoomScale);
#else
                UnityEngine.VR.InputTracking.Recenter();
                UnityEngine.VR.VRDevice.SetTrackingSpaceType(UnityEngine.VR.TrackingSpaceType.RoomScale);
#endif
            }

            if (playbackTexture != null)
            {
                try
                {
                    byte[] dummyData = new byte[playbackTexture.width * playbackTexture.height * 4];

                    Texture2D dummyTex = new Texture2D(playbackTexture.width, playbackTexture.height, playbackTexture.format, false);
                    dummyTex.LoadRawTextureData(dummyData);
                    dummyTex.Apply();
                    Graphics.CopyTexture(dummyTex, playbackTexture);
                    Destroy(dummyTex);
                }
                catch { }
            }
        }

        public void UpdateTexture(uint newTextureWidth, uint newTextureHeight)
        {
            textureWidth = newTextureWidth;
            textureHeight = newTextureHeight;

            // create native texture for playback
            IntPtr nativeTexture = IntPtr.Zero;
            byte isStereoscopicByte = 0;
            CheckHR(Plugin.GetPlaybackTexture(pluginInstance, out nativeTexture, out isStereoscopicByte));

            bool isStereoscopic = isStereoscopicByte > 0 ? true : false;

            if (nativeTexture != IntPtr.Zero)
            {
                var oldTexture = playbackTexture;

                if (playbackTexture == null || playbackTexture.width != (int)this.textureWidth || playbackTexture.height != (int)this.textureHeight)
                {
                    // create a new Unity texture2d 
                    this.playbackTexture = Texture2D.CreateExternalTexture((int)this.textureWidth, (int)this.textureHeight, TextureFormat.BGRA32, false, false, nativeTexture);
                }
                else
                {
                    // we can just update our old Unity texture 
                    oldTexture = null; 
                    this.playbackTexture.UpdateExternalTexture(nativeTexture);
                }

                if (targetRenderer != null)
                {
                    targetRenderer.material.SetTexture(targetRendererTextureName, playbackTexture);
                    isStereoVideo = isStereoscopic;
                    if (!string.IsNullOrEmpty(isStereoShaderParameterName))
                    {
                        bool setStereoscopic = forceStereo || isStereoscopic;
                        targetRenderer.material.SetFloat(isStereoShaderParameterName, setStereoscopic ? 1.0f : 0.0f);
                    }
                }

                if (TextureUpdated != null)
                {

#if UNITY_WSA_10_0
                    if (!UnityEngine.WSA.Application.RunningOnAppThread())
                    {
                        UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                        {
                            TextureUpdated(this);
                        }, true);
                    }
                    else
                    {
                        TextureUpdated(this);
                    }
#else
                    TextureUpdated(this);
#endif
                }

                if (oldTexture != null)
                {
                    try
                    {
                        Destroy(oldTexture);
                    }
                    catch { }
                }

            }

        }


        public long GetDuration()
        {
            long duration = 0;
            long position = 0;

            CheckHR(Plugin.GetDurationAndPosition(pluginInstance, ref duration, ref position));
            return duration;
        }


        public long GetPosition()
        {
            long duration = 0;
            long position = 0;

            CheckHR(Plugin.GetDurationAndPosition(pluginInstance, ref duration, ref position));
            return position;
        }

        public void Seek(long position)
        {
            CheckHR(Plugin.Seek(pluginInstance, position));
        }

        public void SetVolume(float volume)
        {
            CheckHR(Plugin.SetVolume(pluginInstance, volume));
        }


        public uint GetVideoWidth()
        {
            return currentMediaDescription.width;
        }

        public uint GetVideoHeight()
        {
            return currentMediaDescription.height;
        }

        public bool IsReady()
        {
            return (loaded && !string.IsNullOrEmpty(currentItem));
        }

        public uint GetSublitlesTracksCount()
        {
            uint count = 0;
            Plugin.GetSubtitlesTracksCount(pluginInstance, out count);
            return count;
        }

        public void GetSubtitlesTrack(uint index, out string id, out string title, out string language)
        {
            IntPtr _id;
            IntPtr _title;
            IntPtr _language;

            Plugin.GetSubtitlesTrack(pluginInstance, index, out _id, out _title, out _language);

            id = Marshal.PtrToStringUni(_id);
            title = Marshal.PtrToStringUni(_title);
            language = Marshal.PtrToStringUni(_language);
        }

        IEnumerator Start()
        {
            yield return StartCoroutine("CallPluginAtEndOfFrames");
        }


        private void Update()
        {
            if(needToUpdateTexture)
            {
                needToUpdateTexture = false;
                UpdateTexture(currentMediaDescription.width, currentMediaDescription.height);
            }
        }

        private void OnEnable()
        {
            // create callback
            thisObject = GCHandle.Alloc(this, GCHandleType.Normal);
            IntPtr thisObjectPtr = GCHandle.ToIntPtr(thisObject);

            // create media playback
            CheckHR(Plugin.CreateMediaPlayback(this.stateCallback, thisObjectPtr, out pluginInstance));

            Plugin.IsHardware4KDecodingSupported(pluginInstance, out hw4KDecodingSupported);

            Debug.LogFormat("MediaPlayback has been created. Hardware decoding of 4K+ is {0}.", hw4KDecodingSupported ? "supported" : "not supported");

            CheckHR(Plugin.SetSubtitlesCallbacks(pluginInstance, subtitleEnteredCallback, subtitleExitedCallback));
        }

        private void OnDisable()
        {
            loaded = false;
            if (pluginInstance != IntPtr.Zero)
            {
                if (currentState == PlaybackState.Playing)
                {
                    try
                    {
                        Stop();
                    }
                    catch { }
                }
                Plugin.ReleaseMediaPlayback(pluginInstance);
                pluginInstance = IntPtr.Zero;
            }

            if (thisObject.Target != null)
            {
                try
                {
                    thisObject.Free();
                }
                catch { }
            }

            if (playbackTexture != null)
            {
                try
                {
                    Destroy(playbackTexture);
                    playbackTexture = null;
                }
                catch { }
            }
        }

        [AOT.MonoPInvokeCallback(typeof(Plugin.StateChangedCallback))]
        private static void MediaPlayback_Changed(IntPtr thisObjectPtr, Plugin.PLAYBACK_STATE args)
        {
            if (thisObjectPtr == IntPtr.Zero)
            {
                Debug.LogError("MediaPlayback_Changed: requires thisObjectPtr.");
                return;
            }

            var handle = GCHandle.FromIntPtr(thisObjectPtr);
            Playback thisObject = handle.Target as Playback;
            if (thisObject == null)
            {
                Debug.LogError("MediaPlayback_Changed: thisObjectPtr is not null, but seems invalid.");
                return;
            }

#if UNITY_WSA_10_0
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    thisObject.OnStateChanged(args);
                }, false);
            }
            else
            {
                thisObject.OnStateChanged(args);
            }
#else
            thisObject.OnStateChanged(args);
#endif
        }


        [AOT.MonoPInvokeCallback(typeof(Plugin.SubtitleItemEnteredCallback))]
        private static void MediaPlayback_SubtitleItemEntered(IntPtr thisObjectPtr, [MarshalAs(UnmanagedType.LPWStr)] string subtitleTrackId, 
                                                                                    [MarshalAs(UnmanagedType.LPWStr)] string textCueId,
                                                                                    [MarshalAs(UnmanagedType.LPWStr)] string language,
                                                                                    IntPtr textLinesPtr,
                                                                                    uint linesCount)
        {
            if (thisObjectPtr == IntPtr.Zero)
            {
                Debug.LogError("MediaPlayback_SubtitleItemEntered: requires thisObjectPtr.");
                return;
            }

            var handle = GCHandle.FromIntPtr(thisObjectPtr);
            Playback thisObject = handle.Target as Playback;
            if (thisObject == null)
            {
                Debug.LogError("MediaPlayback_SubtitleItemEntered: thisObjectPtr is not null, but seems invalid.");
                return;
            }

            IntPtr[] ptrArray = new IntPtr[linesCount];
            string[] textLines = new string[linesCount];

            Marshal.Copy(textLinesPtr, ptrArray, 0, (int)linesCount);

            for (int i=0; i < textLines.Length; i++)
            {
                string line = Marshal.PtrToStringUni(ptrArray[i]);
                textLines[i] = line;
            }

#if UNITY_WSA_10_0
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    thisObject.OnSubtitleItemEntered(subtitleTrackId, textCueId, language, textLines);
                }, true);
            }
            else
            {
                thisObject.OnSubtitleItemEntered(subtitleTrackId, textCueId, language, textLines);
            }
#else
            thisObject.OnSubtitleItemEntered(subtitleTrackId, textCueId, language, textLines);
#endif
        }


        [AOT.MonoPInvokeCallback(typeof(Plugin.SubtitleItemExitedCallback))]
        private static void MediaPlayback_SubtitleItemExited(IntPtr thisObjectPtr, IntPtr subtitleTrackId, IntPtr textCueId)
        {
            if (thisObjectPtr == IntPtr.Zero)
            {
                Debug.LogError("MediaPlayback_SubtitleItemExited: requires thisObjectPtr.");
                return;
            }

            var handle = GCHandle.FromIntPtr(thisObjectPtr);
            Playback thisObject = handle.Target as Playback;
            if (thisObject == null)
            {
                Debug.LogError("MediaPlayback_SubtitleItemExited: thisObjectPtr is not null, but seems invalid.");
                return;
            }

            string trackId = Marshal.PtrToStringUni(subtitleTrackId);
            string cueId = Marshal.PtrToStringUni(textCueId);

#if UNITY_WSA_10_0
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    thisObject.OnSubtitleItemExited(trackId, cueId);
                }, true);
            }
            else
            {
                thisObject.OnSubtitleItemExited(trackId, cueId);
            }
#else
            thisObject.OnSubtitleItemExited(trackId, cueId);
#endif
        }


        private void OnSubtitleItemEntered(string subtitleTrackId, string textCueId, string language, string[] textLines)
        {
            if (SubtitleItemEntered != null)
            {
                SubtitleItemEntered(this, subtitleTrackId, textCueId, language, textLines);
            }
        }

        private void OnSubtitleItemExited(string subtitleTrackId, string textCueId)
        {
            if (SubtitleItemExited != null)
            {
                SubtitleItemExited(this, subtitleTrackId, textCueId);
            }
        }


        private void TriggerPlaybackStateChangedEvent(ChangedEventArgs<PlaybackState> args)
        {
            if (this.PlaybackStateChanged != null)
            {

                try
                {
                    this.PlaybackStateChanged(this, args);
                }
                catch { }
            }
        }


        private void OnStateChanged(Plugin.PLAYBACK_STATE args)
        {
            var stateType = (Plugin.StateType)Enum.ToObject(typeof(Plugin.StateType), args.type);

            switch (stateType)
            {
                case Plugin.StateType.StateType_None:
                    var newState0 = (PlaybackState)Enum.ToObject(typeof(PlaybackState), args.state);
                    if (newState0 == PlaybackState.Ended || newState0 == PlaybackState.None)
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                    loaded = false;
                    Debug.Log("Playback State: " + stateType.ToString());
                    break;

                case Plugin.StateType.StateType_NewFrameTexture:
                    currentMediaDescription.duration = args.description.duration;
                    currentMediaDescription.width = args.description.width;
                    currentMediaDescription.height = args.description.height;
                    currentMediaDescription.isSeekable = args.description.isSeekable;
                    currentMediaDescription.isStereoscopic = args.description.isStereoscopic;
                    needToUpdateTexture = true;
                    break;

                case Plugin.StateType.StateType_StateChanged:
                    var newState = (PlaybackState)Enum.ToObject(typeof(PlaybackState), args.state);
                    if (newState == PlaybackState.None)
                    {
                        if (State == PlaybackState.Playing || State == PlaybackState.Paused)
                        {
                            Debug.Log("Video ended");
                            newState = PlaybackState.Ended;
                        }
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                        loaded = false;
                    }
                    else if (newState == PlaybackState.Ended)
                    {
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                        loaded = false;
                    }
                    else if (newState != PlaybackState.Buffering && args.description.width != 0 && args.description.height != 0)
                    {
                        currentMediaDescription.duration = args.description.duration;
                        currentMediaDescription.width = args.description.width;
                        currentMediaDescription.height = args.description.height;
                        currentMediaDescription.isSeekable = args.description.isSeekable;
                        currentMediaDescription.isStereoscopic = args.description.isStereoscopic;
                    }
                    this.State = newState;
                    Debug.Log("Playback State: " + stateType.ToString() + " - " + this.State.ToString());
                    break;
                case Plugin.StateType.StateType_Opened:
                    currentMediaDescription.duration = args.description.duration;
                    currentMediaDescription.width = args.description.width;
                    currentMediaDescription.height = args.description.height;
                    currentMediaDescription.isSeekable = args.description.isSeekable;
                    currentMediaDescription.isStereoscopic = args.description.isStereoscopic;
                    Debug.Log("Media Opened: " + args.description.ToString());
                    break;
                case Plugin.StateType.StateType_Failed:
                    loaded = false;
                    CheckHR(args.hresult);
                    if (this.PlaybackFailed != null)
                    {
                        PlaybackFailed(this, args.hresult);
                    }
                    this.State = PlaybackState.None;
                    loaded = false;
                    break;
                case Plugin.StateType.StateType_GraphicsDeviceShutdown:
                    Debug.LogWarning("Graphics device was lost!");
                    break;
                case Plugin.StateType.StateType_GraphicsDeviceReady:
                    Debug.LogWarning("Graphics device was restored! Recreating the playback texture!");
                    break;
                default:
                    break;
            }
        }


        private IEnumerator CallPluginAtEndOfFrames()
        {
            while (true)
            {
                // Wait until all frame rendering is done
                yield return new WaitForEndOfFrame();

                // Set time for the plugin
                Plugin.SetTimeFromUnity(Time.timeSinceLevelLoad);

                // Issue a plugin event with arbitrary integer identifier.
                // The plugin can distinguish between different
                // things it needs to do based on this ID.
                // For our simple plugin, it does not matter which ID we pass here.
                GL.IssuePluginEvent(Plugin.GetRenderEventFunc(), 1);
            }
        }

        public static long CheckHR(long hresult)
        {
            if (hresult != 0)
            {
                Debug.Log("Media Failed: HRESULT = 0x" + hresult.ToString("X", System.Globalization.NumberFormatInfo.InvariantInfo));
            }
            return hresult;
        }

        private static class Plugin
        {
            public enum StateType
            {
                StateType_None = 0,
                StateType_Opened,
                StateType_StateChanged,
                StateType_Failed,
                StateType_NewFrameTexture,
                StateType_GraphicsDeviceShutdown,
                StateType_GraphicsDeviceReady
            };

            [StructLayout(LayoutKind.Sequential, Pack = 8)]
            public struct MEDIA_DESCRIPTION
            {
                public UInt32 width;
                public UInt32 height;
                public Int64 duration;
                public byte isSeekable;
                public byte isStereoscopic;

                public override string ToString()
                {
                    StringBuilder sb = new StringBuilder();
                    sb.AppendLine("width: " + width);
                    sb.AppendLine("height: " + height);
                    sb.AppendLine("duration: " + duration);
                    sb.AppendLine("canSeek: " + isSeekable);
                    sb.AppendLine("isStereoscopic: " + isStereoscopic);

                    return sb.ToString();
                }
            };

            [StructLayout(LayoutKind.Sequential, Pack = 8)]
            public struct PLAYBACK_STATE
            {
                public UInt32 type;
                public UInt32 state;
                public Int64 hresult;
                public MEDIA_DESCRIPTION description;
            };

            public delegate void StateChangedCallback(IntPtr thisObjectPtr, PLAYBACK_STATE args);
            public delegate void SubtitleItemEnteredCallback(IntPtr thisObjectPtr,  [MarshalAs(UnmanagedType.LPWStr)] string subtitleTrackId, 
                                                                                    [MarshalAs(UnmanagedType.LPWStr)] string textCueId, 
                                                                                    [MarshalAs(UnmanagedType.LPWStr)] string language,
                                                                                    IntPtr textLinesPtr, 
                                                                                    uint linesCount);
            public delegate void SubtitleItemExitedCallback(IntPtr thisObjectPtr, IntPtr subtitleTrackId, IntPtr textCueId);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "CreateMediaPlayback")]
            internal static extern long CreateMediaPlayback(StateChangedCallback callback, IntPtr playbackObject, out IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseMediaPlayback")]
            internal static extern void ReleaseMediaPlayback(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadContent")]
            internal static extern long LoadContent(IntPtr pluginInstance, [MarshalAs(UnmanagedType.LPWStr)] string sourceURL);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Play")]
            internal static extern long Play(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Pause")]
            internal static extern long Pause(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Stop")]
            internal static extern long Stop(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetPlaybackTexture")]
            internal static extern long GetPlaybackTexture(IntPtr pluginInstance, out IntPtr playbackTexture, out byte isStereoscopic);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetDurationAndPosition")]
            internal static extern long GetDurationAndPosition(IntPtr pluginInstance, ref long duration, ref long position);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Seek")]
            internal static extern long Seek(IntPtr pluginInstance, long position);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetVolume")]
            internal static extern long SetVolume(IntPtr pluginInstance, double volume);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "IsHardware4KDecodingSupported")]
            internal static extern long IsHardware4KDecodingSupported(IntPtr pluginInstance, out bool hwDecoding4KSupported);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetMediaPlayer")]
            internal static extern long GetMediaPlayer(IntPtr pluginInstance, out IntPtr ppvUnknown);


            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetSubtitlesCallbacks")]
            internal static extern long SetSubtitlesCallbacks(IntPtr pluginInstance, SubtitleItemEnteredCallback enteredCallback, SubtitleItemExitedCallback exitedCallback);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetSubtitlesTracksCount")]
            internal static extern long GetSubtitlesTracksCount(IntPtr pluginInstance, [Out] out uint count);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetSubtitlesTrack")]
            internal static extern long GetSubtitlesTrack(IntPtr pluginInstance, uint index, out IntPtr trackId, out IntPtr trackLabel, out IntPtr trackLanuguage);


            // Unity plugin
            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetTimeFromUnity")]
            internal static extern void SetTimeFromUnity(float t);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
            internal static extern IntPtr GetRenderEventFunc();
        }
    }
}
