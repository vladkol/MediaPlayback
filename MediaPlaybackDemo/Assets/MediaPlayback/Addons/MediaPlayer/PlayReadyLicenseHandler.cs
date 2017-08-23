using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayReadyLicenseHandler : MonoBehaviour
{
    //use https://playready.directtaps.net/svc/live/root/rightsmanager.asmx as license server URL and empty custom data for Microsoft Test DRM-ed Streams
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
