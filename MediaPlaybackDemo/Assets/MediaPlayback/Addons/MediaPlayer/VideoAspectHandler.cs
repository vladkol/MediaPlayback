using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class VideoAspectHandler : MonoBehaviour
{
    public MediaPlayer.Playback player;

    // Use this for initialization
    void Start ()
    {
		if(player == null)
        {
            player = GetComponent<MediaPlayer.Playback>();
        }

        if(player != null)
        {
            player.PlaybackStateChanged += PlaybackStateChangedHandler;
        }
	}

    private void PlaybackStateChangedHandler(object sender, MediaPlayer.ChangedEventArgs<MediaPlayer.PlaybackState> args)
    {
        var newState = args.CurrentState;

        if (newState == MediaPlayer.PlaybackState.Playing)
        {
            var w = player.GetVideoWidth();
            var h = player.GetVideoHeight();

            if(w != 0 && h != 0)
            {
                var scale = gameObject.transform.localScale;
                scale.y = scale.x * h / w;
                gameObject.transform.localScale = scale;
            }
        }
    }
}
