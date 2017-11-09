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
using System.Collections.Generic;
using System.IO;
using UnityEngine;

[RequireComponent(typeof(MediaPlayer.Playback))]
public class MediaPlaybackRunner : MonoBehaviour {

    public string mediaURI = string.Empty;
    public bool autoPlay = true;

    MediaPlayer.Playback _player;

    private void Awake()
    {
        _player = GetComponent<MediaPlayer.Playback>();
    }

    // Use this for initialization
    void Start () {
		if(autoPlay)
        {
            Play();
        }
	}
	
	// Update is called once per frame
	void Update () {
		
	}

    public void Play()
    {
        if(!string.IsNullOrEmpty(mediaURI))
        {
            string uriStr = mediaURI;

            if (Uri.IsWellFormedUriString(mediaURI, UriKind.Absolute))
            {
                uriStr = mediaURI;
            }
            else if (Path.IsPathRooted(mediaURI))
            {
                uriStr = "file:///" + mediaURI;
            }
            else
            {
                uriStr = "file:///" + Path.Combine(Application.streamingAssetsPath, mediaURI);
            }

            _player.Play(uriStr);
        }
    }

    public MediaPlayer.Playback GetMediaPlayer()
    {
        return _player;
    }
}
