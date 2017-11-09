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
using UnityEngine;

public class PlayerTime : MonoBehaviour {

    public TextMesh textMesh = null;
    public UnityEngine.UI.Text uiText = null;

    public MediaPlayer.Playback player;

	// Use this for initialization
	void Start () {
		
	}
	
	// Update is called once per frame
	void Update ()
    {
        string posText = "--:--:--.--";

        if (player != null && player.State != MediaPlayer.PlaybackState.None)
        {
            long position = player.GetPosition();

            TimeSpan t = new TimeSpan(position);

            posText = string.Format("{0:D2}:{1:D2}:{2:D2}.{3:D3}", t.Hours, t.Minutes, t.Seconds, t.Milliseconds);
        }

        if(textMesh != null)
            textMesh.text = posText;
        if (uiText != null)
            uiText.text = posText;
    }
}
