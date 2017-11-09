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

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayReadyLicenseHandler : MonoBehaviour
{
    //use http://test.playready.microsoft.com/service/rightsmanager.asmx?cfg=(playright:true,persist:false,playenablers:(AE092501-A9E3-46F6-AFBE-628577DCDF55)) as license server URL and empty custom data for Microsoft Test DRM-ed Streams
    public string playReadyLicenseServiceUri;
    public string playReadyCustomChallendgeData;
    public MediaPlayer.Playback mediaPlayer;

    // Use this for initialization
    void Start ()
    {
	    if(mediaPlayer == null)
        {
            mediaPlayer = GetComponent<MediaPlayer.Playback>();
        }

        if(mediaPlayer != null)
        {
            mediaPlayer.DRMLicenseRequested += DRMLicenseRequested;
        }
	}

    void DRMLicenseRequested(object sender, ref MediaPlayer.PlayReadyLicenseData licenseData)
    {
        if (!string.IsNullOrEmpty(playReadyLicenseServiceUri))
        {
            Debug.Log("PlayReady License Requested. Using " + playReadyLicenseServiceUri);

            licenseData.playReadyLicenseUrl = playReadyLicenseServiceUri;
            licenseData.playReadyChallengeCustomData = playReadyCustomChallendgeData;
        }
    }

    
}
