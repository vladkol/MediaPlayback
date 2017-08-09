using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class StreamSwitcher : MonoBehaviour
{
    public string[] streams;

	// Use this for initialization
	void Start ()
    {
		
	}

    private void OnGUI()
    {
        if (streams != null && streams.Length > 0)
        {
            for (int i = 0; i < streams.Length; i++)
            {
                GUIContent content = new GUIContent(string.Format("Stream {0}", i + 1), streams[i]);
                if (GUI.Button(new Rect(1, 28 + i * 50, 100, 40), content))
                {
                    PlayStream(i);
                }
            }
            GUI.Label(new Rect(1, 1, 1000, 24), GUI.tooltip);
        }

        MediaPlayer.Playback player = gameObject.GetComponent<MediaPlayer.Playback>();
        if(player != null && player.State != MediaPlayer.PlaybackState.None && player.State != MediaPlayer.PlaybackState.Ended)
        {
            if (player.State == MediaPlayer.PlaybackState.Playing || player.State == MediaPlayer.PlaybackState.Paused)
            {
                if (player.State == MediaPlayer.PlaybackState.Playing)
                {
                    if (GUI.Button(new Rect(110, 28, 100, 40), "Pause"))
                    {
                        player.Pause();
                    }
                }
                else
                {
                    if (GUI.Button(new Rect(110, 28, 100, 40), "Resume"))
                    {
                        player.Play();
                    }
                }

                if (GUI.Button(new Rect(110, 80, 100, 40), "Stop"))
                {
                    player.Stop();
                }
            }
        }
    }

    // Update is called once per frame
    void Update ()
    {
		
	}

    bool PlayStream(int index)
    {
        if (index >= 0 && streams != null && streams.Length > index)
        {
            MediaPlayer.Playback player = gameObject.GetComponent<MediaPlayer.Playback>();
            if (player != null)
            {
                Debug.LogFormat("Playing {0}", streams[index]);

                player.Play(streams[index]);
                return true;
            }
        }

        return false;
    }
}
