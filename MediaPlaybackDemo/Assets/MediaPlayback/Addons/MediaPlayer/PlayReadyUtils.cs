using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
using System.Runtime.InteropServices.WindowsRuntime;
#endif

namespace MediaPlayer
{
    public class PlayReadyLicenseData
    {
        public string playReadyLicenseUrl;
        public string playReadyChallengeCustomData;

        public PlayReadyLicenseData()
        {
            playReadyLicenseUrl = string.Empty;
            playReadyChallengeCustomData = null;
        }
    }

    public delegate void ActionRef<T>(object sender, ref T item);

}
