using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MediaSkybox : MonoBehaviour
{
    public MediaPlayer.Playback mediaPlayer;

	void OnEnable ()
    {
        if (mediaPlayer == null)
            mediaPlayer = GetComponent<MediaPlayer.Playback>();
        if(mediaPlayer != null)
        {
            mediaPlayer.TextureUpdated += MediaPlayer_TextureUpdated;
        }
	}

    private void MediaPlayer_TextureUpdated(object sender, Texture2D newVideoTexture, bool isStereoscopic)
    {
        RenderSettings.skybox.SetTexture("_MainTex", newVideoTexture);
        RenderSettings.skybox.SetFloat("_isStereo", isStereoscopic ? 1 : 0); 
    }

    void OnDisable ()
    {
		if(mediaPlayer != null)
        {
            mediaPlayer.TextureUpdated -= MediaPlayer_TextureUpdated;
        }
	}
}
