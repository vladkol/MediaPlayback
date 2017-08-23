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
    }

    public delegate void ActionRef<T>(object sender, ref T item);

    public class CommonLicenseRequest
    {
#if UNITY_WSA_10_0 && ENABLE_WINMD_SUPPORT
        private string lastErrorMessage;
        private Windows.Web.Http.HttpClient httpClient;
        
        public string GetLastErrorMessage()
        {
            return lastErrorMessage;
        }

        public CommonLicenseRequest(Windows.Web.Http.HttpClient client = null)
        {
            if (client == null)
                httpClient = new Windows.Web.Http.HttpClient();
            else
                httpClient = client;
            lastErrorMessage = string.Empty;
        }
        /// <summary>
        /// Invoked to acquire the PlayReady license.
        /// </summary>
        /// <param name="licenseServerUri">License Server URI to retrieve the PlayReady license.</param>
        /// <param name="httpRequestContent">HttpContent including the Challenge transmitted to the PlayReady server.</param>
        public async virtual System.Threading.Tasks.Task<Windows.Web.Http.IHttpContent> AcquireLicenseAsync(Uri licenseServerUri, Windows.Web.Http.IHttpContent httpRequestContent)
        {
            try
            {
                httpClient.DefaultRequestHeaders.Add("msprdrm_server_redirect_compat", "false");
                httpClient.DefaultRequestHeaders.Add("msprdrm_server_exception_compat", "false");

                Windows.Web.Http.HttpResponseMessage response = await httpClient.PostAsync(licenseServerUri, httpRequestContent);
                response.EnsureSuccessStatusCode();

                if (response.StatusCode == Windows.Web.Http.HttpStatusCode.Ok)
                {
                    lastErrorMessage = string.Empty;
                    return response.Content;
                }
                else
                {
                    lastErrorMessage = "AcquireLicense - Http Response Status Code: " + response.StatusCode.ToString();
                }
            }
            catch (Exception exception)
            {
                lastErrorMessage = exception.Message;
                return null;
            }
            return null;
        }
#endif
    }


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


        public static async System.Threading.Tasks.Task LicenseAcquisitionRequest(Windows.Media.Protection.PlayReady.PlayReadyLicenseAcquisitionServiceRequest licenseRequest, Windows.Media.Protection.MediaProtectionServiceCompletion CompletionNotifier,
                                            string Url,
                                            string ChallengeCustomData)
        {
            bool bResult = false;
            string ExceptionMessage = string.Empty;

            try
            {
                if (!string.IsNullOrEmpty(Url))
                {
                    if (!string.IsNullOrEmpty(ChallengeCustomData))
                    {
                        System.Text.UTF8Encoding encoding = new System.Text.UTF8Encoding();
                        byte[] b = encoding.GetBytes(ChallengeCustomData);
                        licenseRequest.ChallengeCustomData = Convert.ToBase64String(b, 0, b.Length);
                    }

                    Windows.Media.Protection.PlayReady.PlayReadySoapMessage soapMessage = licenseRequest.GenerateManualEnablingChallenge();

                    byte[] messageBytes = soapMessage.GetMessageBody();
                    Windows.Web.Http.HttpBufferContent httpContent = new Windows.Web.Http.HttpBufferContent(messageBytes.AsBuffer());

                    Windows.Foundation.Collections.IPropertySet propertySetHeaders = soapMessage.MessageHeaders;

                    foreach (string strHeaderName in propertySetHeaders.Keys)
                    {
                        string strHeaderValue = propertySetHeaders[strHeaderName].ToString();

                        if (strHeaderName.Equals("Content-Type", StringComparison.OrdinalIgnoreCase))
                        {
                            httpContent.Headers.ContentType = Windows.Web.Http.Headers.HttpMediaTypeHeaderValue.Parse(strHeaderValue);
                        }
                        else
                        {
                            httpContent.Headers.Add(strHeaderName.ToString(), strHeaderValue);
                        }
                    }

                    CommonLicenseRequest licenseAcquision = new CommonLicenseRequest();

                    Windows.Web.Http.IHttpContent responseHttpContent =
                        await licenseAcquision.AcquireLicenseAsync(new Uri(Url), httpContent);

                    if (responseHttpContent != null)
                    {
                        Exception exResult = licenseRequest.ProcessManualEnablingResponse(
                                                 (await responseHttpContent.ReadAsBufferAsync()).ToArray());

                        if (exResult != null)
                        {
                            throw exResult;
                        }
                        bResult = true;
                    }
                    else
                    {
                        ExceptionMessage = licenseAcquision.GetLastErrorMessage();
                    }
                }
                else
                {
                    await licenseRequest.BeginServiceRequest();
                    bResult = true;
                }
            }
            catch (Exception e)
            {
                ExceptionMessage = e.Message;
            }

            CompletionNotifier.Complete(bResult);
        }
#endif
    }
}
