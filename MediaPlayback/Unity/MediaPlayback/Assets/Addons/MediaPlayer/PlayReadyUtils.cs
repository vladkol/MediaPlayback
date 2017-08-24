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


    public static class PlayReadyUtils
    {
#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
        private static uint MSPR_E_CONTENT_ENABLING_ACTION_REQUIRED = 0x8004B895;
        
        public static async System.Threading.Tasks.Task<bool> ReactiveIndivRequest(Windows.Media.Protection.PlayReady.PlayReadyIndividualizationServiceRequest IndivRequest, Windows.Media.Protection.MediaProtectionServiceCompletion CompletionNotifier)
        {
            bool bResult = false;
            Exception exception = null;

            try
            {
                await IndivRequest.BeginServiceRequest();
            }
            catch (Exception ex)
            {
                exception = ex;
            }
            finally
            {
                if (exception == null)
                {
                    bResult = true;
                }
                else
                {
                    System.Runtime.InteropServices.COMException comException = exception as System.Runtime.InteropServices.COMException;
                    if (comException != null && (uint)comException.HResult == MSPR_E_CONTENT_ENABLING_ACTION_REQUIRED)
                    {
                        IndivRequest.NextServiceRequest();
                    }
                }
            }

            if (CompletionNotifier != null)
                CompletionNotifier.Complete(bResult);

            return bResult;
        }

        public static async System.Threading.Tasks.Task ProActiveIndivRequest()
        {
            Windows.Media.Protection.PlayReady.PlayReadyIndividualizationServiceRequest indivRequest = new Windows.Media.Protection.PlayReady.PlayReadyIndividualizationServiceRequest();
            bool bResultIndiv = await ReactiveIndivRequest(indivRequest, null);
        }

#endif
    }
}
