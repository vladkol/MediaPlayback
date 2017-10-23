using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;

public class PlayerSubtitles : MonoBehaviour
{
    System.Globalization.CultureInfo ci;

    public TextMesh textMesh = null;
    public UnityEngine.UI.Text uiText = null;
    public string twoLetterLanguageName = "en";

    public MediaPlayer.Playback player;

    private string lastId = string.Empty;
    private bool reportedSubtitles = false;

    // Use this for initialization
    void Start ()
    {
        player.PlaybackStateChanged += PlaybackStateChangedHandler;
        player.SubtitleItemEntered += OnSubtitleItemEntered;
        player.SubtitleItemExited += OnSubtitleItemExited;
    }

    private void PlaybackStateChangedHandler(object sender, MediaPlayer.ChangedEventArgs<MediaPlayer.PlaybackState> args)
    {
        var newState = args.CurrentState;

        if (newState == MediaPlayer.PlaybackState.None || newState == MediaPlayer.PlaybackState.Ended)
        {
            SetText(string.Empty);
            lastId = string.Empty;
            reportedSubtitles = false;
        }
        else if(newState == MediaPlayer.PlaybackState.Playing && !reportedSubtitles)
        { 
            uint count = player.GetSublitlesTracksCount();

            for(uint i=0; i < count; i++)
            {
                string id, title, language;

                player.GetSubtitlesTrack(i, out id, out title, out language);

                Debug.LogFormat("Subtitle track id={0}, title='{1}', language={2}", id, title, language);
            }

            reportedSubtitles = true;
        }
    }


    private void OnSubtitleItemEntered(object sender, string subtitleTrackId, string textCueId, string language, string[] textLines)
    {
        if (lastId != string.Empty)
            return;

        if (!string.IsNullOrEmpty(twoLetterLanguageName) && string.Compare(twoLetterLanguageName, language, true) != 0)
            return;

        StringBuilder builder = new StringBuilder();
        if(textLines != null && textLines.Length > 0)
        {
            for(int i=0; i < textLines.Length; i++)
            {
                if (i == textLines.Length - 1)
                    builder.Append(textLines[i]);
                else
                    builder.AppendLine(textLines[i]);
            }
        }

        SetText(builder.ToString());
    }

    private void OnSubtitleItemExited(object sender, string subtitleTrackId, string textCueId)
    {
        if(textCueId == lastId)
        {
            SetText(string.Empty);
            lastId = string.Empty;
        }
    }

    private void SetText(string text)
    {
        if (textMesh != null)
            textMesh.text = text;
        if (uiText != null)
            uiText.text = text;
    }
}
