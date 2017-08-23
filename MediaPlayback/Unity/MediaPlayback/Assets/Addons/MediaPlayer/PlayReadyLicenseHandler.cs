using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayReadyLicenseHandler : MonoBehaviour
{
    //use http://test.playready.microsoft.com/service/rightsmanager.asmx?PlayRight=1&UseSimpleNonPersistentLicense=1 as license server URL and empty custom data for Microsoft Test DRM-ed Streams
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
