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
    public class Playback : MonoBehaviour
    {
        // state handling
        public Action<object, ChangedEventArgs<PlaybackState>> PlaybackStateChanged;
        public Action<object, long> PlaybackFailed;
        public MediaPlayer.ActionRef<MediaPlayer.PlayReadyLicenseData> DRMLicenseRequested;

        // texture size
        public uint TextureWidth = 4096;
        public uint TextureHeight = 4096;

        public bool UseFFMPEG = false;
        public bool SoftwareDecode = false;

        public bool usePlayReadyDRM = false;

        public PlaybackState State
        {
            get { return this.currentState; }
            private set
            {
                if (this.currentState != value)
                {
                    this.previousState = this.currentState;
                    this.currentState = value;
                    if (this.PlaybackStateChanged != null)
                    {
                        var args = new ChangedEventArgs<PlaybackState>(this.previousState, this.currentState);

#if UNITY_WSA_10_0
                        UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                        {
                            this.PlaybackStateChanged(this, args);
                        }, false);
#else
                        this.PlaybackStateChanged(this, args);
#endif
                    }
                }
            }
        }

        private IntPtr pluginInstance = IntPtr.Zero;
        private GCHandle thisObject;

        private string currentItem = string.Empty;

        private PlaybackState currentState = PlaybackState.None;
        private PlaybackState previousState = PlaybackState.None;

        private Texture2D playbackTexture;
        private Plugin.StateChangedCallback stateCallback;

        private bool loaded = false;
        private Plugin.MEDIA_DESCRIPTION currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();

        private PlayReadyLicenseData playReadyLicense;


        public void Load(string uriOrPath)
        {
            Stop();

            string uriStr = uriOrPath.Trim();

            if (Uri.IsWellFormedUriString(uriOrPath, UriKind.Absolute))
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

            if(usePlayReadyDRM)
            {
                InitializePlayReady();
            }

            loaded = (0 == CheckHR(Plugin.LoadContent(pluginInstance, UseFFMPEG, SoftwareDecode, uriStr)));
            if(loaded)
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

        public Texture2D GetVideoTexture()
        {
            return playbackTexture;
        }

        internal void PlayWithFFmpeg(string selectedItem, bool softwareDecode)
        {
            CheckHR(Plugin.LoadContent(pluginInstance, true, softwareDecode, selectedItem));     // hardware decode video. 
                                                                        // if using a non-supported type change to true (eg. Ogg)
            CheckHR(Plugin.Play(pluginInstance));
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

            if (playbackTexture != null)
            {
                byte[] dummyData = new byte[playbackTexture.width * playbackTexture.height * 4];

                Texture2D dummyTex = new Texture2D(playbackTexture.width, playbackTexture.height, playbackTexture.format, false);
                dummyTex.LoadRawTextureData(dummyData);
                dummyTex.Apply();
                Graphics.CopyTexture(dummyTex, playbackTexture);
                Destroy(dummyTex);
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


        public void SetPlayReadyDRMLicense(PlayReadyLicenseData licenseData)
        {
            this.playReadyLicense = licenseData;
        }

        // only works when exported as a UWP app. Doesn't work in Unity Editor 
        private void InitializePlayReady()
        {
            Plugin.SetDRMLicenseCallback(pluginInstance, MediaPlayback_DRMLicenseRequested);
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

        IEnumerator Start()
        {
            yield return StartCoroutine("CallPluginAtEndOfFrames");
        }

        private void OnEnable()
        {
            // create callback
            thisObject = GCHandle.Alloc(this, GCHandleType.Normal);
            IntPtr thisObjectPtr = GCHandle.ToIntPtr(thisObject);

            this.stateCallback = new Plugin.StateChangedCallback(MediaPlayback_Changed);

            // create media playback
            CheckHR(Plugin.CreateMediaPlayback(this.stateCallback, thisObjectPtr, out pluginInstance));

            // create native texture for playback
            IntPtr nativeTexture = IntPtr.Zero;
            CheckHR(Plugin.CreatePlaybackTexture(pluginInstance, this.TextureWidth, this.TextureHeight, out nativeTexture));

            // create the unity texture2d 
            this.playbackTexture = Texture2D.CreateExternalTexture((int)this.TextureWidth, (int)this.TextureHeight, TextureFormat.BGRA32, false, false, nativeTexture);

            // set texture for the shader
            GetComponent<Renderer>().material.mainTexture = this.playbackTexture;
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

            if(thisObject.Target != null)
            {
                thisObject.Free();
                thisObject.Target = null;
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
            UnityEngine.WSA.Application.InvokeOnAppThread(() =>
        {
            thisObject.OnStateChanged(args);
        }, false);
#else
            thisObject.OnStateChanged(args);
#endif
        }

        private void OnStateChanged(Plugin.PLAYBACK_STATE args)
        {
            var stateType = (Plugin.StateType)Enum.ToObject(typeof(Plugin.StateType), args.type);

            switch (stateType)
            {
                case Plugin.StateType.StateType_None:
                    var newState0 = (PlaybackState)Enum.ToObject(typeof(PlaybackState), args.state);
                    if(newState0 == PlaybackState.Ended || newState0 == PlaybackState.None)
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                    loaded = false;
                    break;
                case Plugin.StateType.StateType_StateChanged:
                    var newState = (PlaybackState)Enum.ToObject(typeof(PlaybackState), args.state);
                    if(newState == PlaybackState.None)
                    {
                        if(State == PlaybackState.Playing || State == PlaybackState.Paused)
                        {
                            Debug.Log("Video ended");
                            newState = PlaybackState.Ended;
                        }
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                        loaded = false;
                    }
                    else if(newState == PlaybackState.Ended)
                    {
                        currentMediaDescription = new Plugin.MEDIA_DESCRIPTION();
                        loaded = false;
                    }
                    else if(newState != PlaybackState.Buffering && args.description.width != 0 && args.description.height != 0)
                    {
                        currentMediaDescription.duration = args.description.duration;
                        currentMediaDescription.width = args.description.width;
                        currentMediaDescription.height = args.description.height;
                        currentMediaDescription.isSeekable = args.description.isSeekable;
                    }
                    this.State = newState;
                    Debug.Log("Playback State: " + stateType.ToString() + " - " + this.State.ToString());
                    break;
                case Plugin.StateType.StateType_Opened:
                    currentMediaDescription.duration = args.description.duration;
                    currentMediaDescription.width = args.description.width;
                    currentMediaDescription.height = args.description.height;
                    currentMediaDescription.isSeekable = args.description.isSeekable;
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
                default:
                    break;
            }
        }


        [AOT.MonoPInvokeCallback(typeof(Plugin.DRMLicenseRequestedCallback))]
        private static void MediaPlayback_DRMLicenseRequested(IntPtr thisObjectPtr)
        {
            if (thisObjectPtr == IntPtr.Zero)
            {
                Debug.LogError("MediaPlayback_DRMLicenseRequested: requires thisObjectPtr.");
                return;
            }

            var handle = GCHandle.FromIntPtr(thisObjectPtr);
            Playback thisObject = handle.Target as Playback;
            if (thisObject == null)
            {
                Debug.LogError("MediaPlayback_DRMLicenseRequested: thisObjectPtr is not null, but seems invalid.");
                return;
            }

#if UNITY_WSA_10_0
            UnityEngine.WSA.Application.InvokeOnAppThread(() =>
            {
                thisObject.OnDRMLicenseRequested();
            }, true);
#else
            thisObject.OnDRMLicenseRequested();
#endif
        }


        private void OnDRMLicenseRequested()
        {
            if (playReadyLicense == null)
                playReadyLicense = new PlayReadyLicenseData();

            if (DRMLicenseRequested != null)
            {
                DRMLicenseRequested(this, ref playReadyLicense);
            }

            Plugin.SetDRMLicense(pluginInstance, playReadyLicense.playReadyLicenseUrl, playReadyLicense.playReadyChallengeCustomData);
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
            };

            [StructLayout(LayoutKind.Sequential, Pack = 8)]
            public struct MEDIA_DESCRIPTION
            {
                public UInt32 width;
                public UInt32 height;
                public Int64 duration;
                public byte isSeekable;

                public override string ToString()
                {
                    StringBuilder sb = new StringBuilder();
                    sb.AppendLine("width: " + width);
                    sb.AppendLine("height: " + height);
                    sb.AppendLine("duration: " + duration);
                    sb.AppendLine("canSeek: " + isSeekable);

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
            public delegate void DRMLicenseRequestedCallback(IntPtr thisObjectPtr);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "CreateMediaPlayback")]
            internal static extern long CreateMediaPlayback(StateChangedCallback callback, IntPtr playbackObject, out IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseMediaPlayback")]
            internal static extern void ReleaseMediaPlayback(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "CreatePlaybackTexture")]
            internal static extern long CreatePlaybackTexture(IntPtr pluginInstance, UInt32 width, UInt32 height, out System.IntPtr playbackTexture);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadContent")]
            internal static extern long LoadContent(IntPtr pluginInstance, bool useFFmpeg, bool decodeVideo, [MarshalAs(UnmanagedType.BStr)] string sourceURL);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Play")]
            internal static extern long Play(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Pause")]
            internal static extern long Pause(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Stop")]
            internal static extern long Stop(IntPtr pluginInstance);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetDurationAndPosition")]
            internal static extern long GetDurationAndPosition(IntPtr pluginInstance, ref long duration, ref long position);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "Seek")]
            internal static extern long Seek(IntPtr pluginInstance, long position);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetVolume")]
            internal static extern long SetVolume(IntPtr pluginInstance, double volume);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetMediaPlayer")]
            internal static extern long GetMediaPlayer(IntPtr pluginInstance, out IntPtr ppvUnknown);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetDRMLicense")]
            internal static extern long SetDRMLicense(IntPtr pluginInstance, [MarshalAs(UnmanagedType.BStr)] string licenseServiceURL, [MarshalAs(UnmanagedType.BStr)] string licenseCustomChallendgeData);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetDRMLicenseCallback")]
            internal static extern long SetDRMLicenseCallback(IntPtr pluginInstance, DRMLicenseRequestedCallback callback);

            // Unity plugin
            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetTimeFromUnity")]
            internal static extern void SetTimeFromUnity(float t);

            [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
            internal static extern IntPtr GetRenderEventFunc();
        }
    }
}