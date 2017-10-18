using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;


#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
using Windows.Storage;
using Windows.Storage.AccessCache;
using System.Threading.Tasks;
#endif

public class VideoLibraryTest : MonoBehaviour
{
    MediaPlaybackRunner runner;

	// Use this for initialization
	void Start ()
    {
        runner = gameObject.GetComponent<MediaPlaybackRunner>();
        if (runner == null)
            return;

#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
        PlayFirstFileFromVideosLibrary();
#endif


    }

#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
    // Update is called once per frame
    async Task PlayFirstFileFromVideosLibrary()
    {
        var files = await KnownFolders.VideosLibrary.GetFilesAsync();
        if(files != null && files.Count > 0)
        {
            var file = files[0];
            var token = string.Format("{0}{1}", DateTime.UtcNow.ToFileTime(), file.Name);
            StorageApplicationPermissions.FutureAccessList.AddOrReplace(token, file);

            runner.mediaURI = "file-access:///" + token;
            if (!UnityEngine.WSA.Application.RunningOnAppThread())
            {
                UnityEngine.WSA.Application.InvokeOnAppThread(() =>
                {
                    runner.Play();
                }, false);
            }
            else
            {
                runner.Play();
            }
        }
    }
#endif
}
