using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class StreamSwitcher : MonoBehaviour
{
    public string[] streams;

    private GUIContent[] buttons;

    // Use this for initialization
    void Start ()
    {
        if (streams != null && streams.Length > 0)
        {
            buttons = new GUIContent[streams.Length];
            for (int i = 0; i < streams.Length; i++)
            {
                var c = new GUIContent(string.Format("Stream {0}", i + 1), streams[i]);
                buttons[i] = c;
            }
        }
    }

    private void OnGUI()
    {
        if (buttons == null || buttons.Length == 0)
            return;

        MediaPlayer.Playback player = gameObject.GetComponent<MediaPlayer.Playback>();

        GUILayout.Label(GUI.tooltip);

        for (int i = 0; i < streams.Length; i++)
        {
            if (GUI.Button(new Rect(1, 28 + i * 50, 100, 40), buttons[i]))
            {
                PlayStream(i);
            }
        }
        
        if (player == null || player.State == MediaPlayer.PlaybackState.None || player.State == MediaPlayer.PlaybackState.Ended)
            return;

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
