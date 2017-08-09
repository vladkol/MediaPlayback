#if UNITY_STANDALONE || UNITY_WSA_10_0 || UNITY_EDITOR

using MediaPlayer;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using UnityEngine.UI;


public class MediaPlayerCtrlWin : MonoBehaviour 
{
    public delegate void VideoEnd();
    public delegate void VideoReady();
    public delegate void VideoError(MEDIAPLAYER_ERROR errorCode, MEDIAPLAYER_ERROR errorCodeExtra);
    public delegate void VideoFirstFrameReady();
    public delegate void VideoResize();
    public delegate void VideoInterrupt();

    public enum MEDIAPLAYER_ERROR
    {
        MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200,
        MEDIA_ERROR_IO = -1004,
        MEDIA_ERROR_MALFORMED = -1007,
        MEDIA_ERROR_TIMED_OUT = -110,
        MEDIA_ERROR_UNSUPPORTED = -1010,
        MEDIA_ERROR_SERVER_DIED = 100,
        MEDIA_ERROR_UNKNOWN = 1
    }

    public enum MEDIAPLAYER_STATE
    {
        NOT_READY,
        READY,
        END,
        PLAYING,
        PAUSED,
        STOPPED,
        ERROR
    }

    public enum MEDIA_SCALE
    {
        SCALE_X_TO_Y,
        SCALE_X_TO_Z,
        SCALE_Y_TO_X,
        SCALE_Y_TO_Z,
        SCALE_Z_TO_X,
        SCALE_Z_TO_Y,
        SCALE_X_TO_Y_2
    }



    public string m_strFileName;
    public GameObject[] m_TargetMaterial = null;
    public bool m_bFullScreen = false;
    public bool m_bSupportRockchip = true;
    public bool m_bPC_FastMode = false;
    public Shader m_shaderYUV;
    public MEDIA_SCALE m_ScaleValue;
    public GameObject[] m_objResize = null;
    public bool m_bLoop = false;
    public bool m_bAutoPlay = true;

    public VideoResize OnResize;
    public VideoReady OnReady;
    public VideoEnd OnEnd;
    public VideoError OnVideoError;
    public VideoFirstFrameReady OnVideoFirstFrameReady;

    // privates 

    private bool m_bInitialized = false;
    private bool m_pausedForReadyState = false;
    private MEDIAPLAYER_STATE m_currentState = MEDIAPLAYER_STATE.NOT_READY;

    private bool m_bNeedResume = false;
    private string m_currentFile = string.Empty;
    private bool m_gotFirstFrame = false;
    private bool m_calledFirstFrame = false;

    private int m_currentWidth = 0;
    private int m_currentHeight = 0;
    private Texture2D m_videoTexture = null;

    private Playback _playback;
    // end of privates


    #region public methods
    // public methods

    public void Load(string strFileName)
    {
        m_strFileName = strFileName;
        m_currentState = MEDIAPLAYER_STATE.NOT_READY;

        m_bInitialized = false;
        m_currentFile = m_strFileName;
        m_currentWidth = 0;
        m_currentHeight = 0;

        string uriStr = string.Empty;

        if (Uri.IsWellFormedUriString(m_currentFile, UriKind.Absolute))
        {
            uriStr = m_strFileName;
        }
        else if (Path.IsPathRooted(m_currentFile))
        {
            uriStr = "file:///" + m_strFileName;
        }
        else
        {
            uriStr = "file:///" + Path.Combine(Application.streamingAssetsPath, m_strFileName);
        }

        _playback.Load(uriStr);

        if (_playback.IsReady() && OnReady != null)
            OnReady();

        if (_playback.IsReady() && m_bAutoPlay)
            Play();
    }


    public void Play()
    {
        if (_playback != null && m_currentFile != m_strFileName)
        {
            Load(m_strFileName);
        }

        _playback.Play();
    }

    public void Stop()
    {
        if(_playback != null)
            _playback.Stop();
    }

    public void Pause()
    {
        _playback.Pause();
    }

    public MEDIAPLAYER_STATE GetCurrentState()
    {
        return m_currentState;
    }

    public Texture2D GetVideoTexture()
    {
        //return _playback.GetVideoTexture();
        return m_videoTexture;
    }

    public void SetVolume(float fVolume)
    {
        _playback.SetVolume(fVolume);
    }

    public int GetSeekPosition()
    {
        return (int)(_playback.GetPosition() / 10000);
    }

    public void SeekTo(int iSeek)
    {
        _playback.Seek((long)iSeek * 10000);
    }

    public void SetSpeed(float fSpeed)
    {
    }

    public int GetDuration()
    {
        return (int)(_playback.GetDuration() / 10000);
    }

    public float GetSeekBarValue()
    {
        float d = GetDuration();
        if (d > 0)
        {
            d = ((float)GetSeekPosition()) / d;
        }

        return d;
    }

    public void SetSeekBarValue(float fValue)
    {
        float d = GetDuration();
        if (d > 0)
        {
            d = d * fValue;
        }
        SeekTo((int)d);
    }

    public int GetCurrentSeekPercent()
    {
        var d = GetDuration();

        if (d == 0)
            return 0;

        return GetSeekPosition() / GetDuration();
    }

    public int GetVideoWidth()
    {
        return (int)_playback.GetVideoWidth();
    }

    public int GetVideoHeight()
    {
        return (int)_playback.GetVideoHeight();
    }

    public void UnLoad()
    {
        Stop();
        m_currentFile = string.Empty;
        m_currentState = MEDIAPLAYER_STATE.NOT_READY;
    }

    public void DeleteVideoTexture()
    {
        if (m_videoTexture != null)
        {
            var t = m_videoTexture;
            m_videoTexture = null;
            Destroy(t);
        }
    }

    public void ResizeTexture()
    {
        UpdateMaterials();
    }

    public void Resize()
    {
        if (m_currentState != MEDIAPLAYER_STATE.PLAYING || !_playback.IsReady())
            return;

        if (GetVideoWidth() <= 0 || GetVideoHeight() <= 0)
        {
            return;
        }

        if (m_objResize != null)
        {
            int iScreenWidth = Screen.width;
            int iScreenHeight = Screen.height;

            float fRatioScreen = (float)iScreenHeight / (float)iScreenWidth;
            int iWidth = GetVideoWidth();
            int iHeight = GetVideoHeight();

            float fRatio = (float)iHeight / (float)iWidth;
            float fRatioResult = fRatioScreen / fRatio;

            for (int i = 0; i < m_objResize.Length; i++)
            {
                if (m_objResize[i] == null)
                    continue;

                if (m_bFullScreen)
                {
                    m_objResize[i].transform.localScale = new Vector3(20.0f / fRatioScreen, 20.0f / fRatioScreen, 1.0f);
                    if (fRatio < 1.0f)
                    {
                        if (fRatioScreen < 1.0f)
                        {
                            if (fRatio > fRatioScreen)
                            {
                                m_objResize[i].transform.localScale *= fRatioResult;
                            }
                        }

                        m_ScaleValue = MEDIA_SCALE.SCALE_X_TO_Y;
                    }
                    else
                    {
                        if (fRatioScreen > 1.0f)
                        {
                            if (fRatio >= fRatioScreen)
                            {

                                m_objResize[i].transform.localScale *= fRatioResult;
                            }
                        }
                        else
                        {
                            m_objResize[i].transform.localScale *= fRatioResult;

                        }

                        m_ScaleValue = MEDIA_SCALE.SCALE_X_TO_Y;
                    }
                }



                if (m_ScaleValue == MEDIA_SCALE.SCALE_X_TO_Y)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x
                        , m_objResize[i].transform.localScale.x * fRatio
                        , m_objResize[i].transform.localScale.z);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_X_TO_Y_2)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x
                        , m_objResize[i].transform.localScale.x * fRatio / 2.0f
                        , m_objResize[i].transform.localScale.z);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_X_TO_Z)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x
                        , m_objResize[i].transform.localScale.y
                        , m_objResize[i].transform.localScale.x * fRatio);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_Y_TO_X)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.y / fRatio
                        , m_objResize[i].transform.localScale.y
                        , m_objResize[i].transform.localScale.z);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_Y_TO_Z)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x
                        , m_objResize[i].transform.localScale.y
                        , m_objResize[i].transform.localScale.y / fRatio);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_Z_TO_X)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.z * fRatio
                        , m_objResize[i].transform.localScale.y
                        , m_objResize[i].transform.localScale.z);
                }
                else if (m_ScaleValue == MEDIA_SCALE.SCALE_Z_TO_Y)
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x
                        , m_objResize[i].transform.localScale.z * fRatio
                        , m_objResize[i].transform.localScale.z);
                }
                else
                {
                    m_objResize[i].transform.localScale
                    = new Vector3(m_objResize[i].transform.localScale.x, m_objResize[i].transform.localScale.y, m_objResize[i].transform.localScale.z);
                }
            }

        }

        if (OnResize != null)
            OnResize();
    }

    // end of public methods
    #endregion



    private void Awake()
    {
        m_currentState = MEDIAPLAYER_STATE.NOT_READY;
        this.m_bSupportRockchip = false;

        if (this.m_TargetMaterial != null)
        {
            for (int i = 0; i < this.m_TargetMaterial.Length; i++)
            {
                if (this.m_TargetMaterial[i] != null)
                {
                    if (this.m_TargetMaterial[i].GetComponent<MeshFilter>() != null)
                    {
                        Vector2[] uv = this.m_TargetMaterial[i].GetComponent<MeshFilter>().mesh.uv;
                        for (int j = 0; j < uv.Length; j++)
                        {
                            uv[j] = new Vector2(uv[j].x, 1f - uv[j].y);
                        }
                        this.m_TargetMaterial[i].GetComponent<MeshFilter>().mesh.uv = uv;
                    }
                    if (this.m_TargetMaterial[i].GetComponent<RawImage>() != null)
                    {
                        this.m_TargetMaterial[i].GetComponent<RawImage>().uvRect = new Rect(0f, 1f, 1f, -1f);
                    }
                }
            }
        }

        _playback = gameObject.GetComponent<Playback>();
        if (_playback == null)
            _playback = gameObject.AddComponent<Playback>();

        _playback.PlaybackStateChanged += _playback_PlaybackStateChanged;
        _playback.TextureWidth = 4096;
        _playback.TextureHeight = 4096;
    }


    void OnEnable()
    {
        if (_playback != null && m_currentState == MEDIAPLAYER_STATE.PAUSED)
        {
            this.Play();
        }
    }

    void Start()
    {
        if (!string.IsNullOrEmpty(m_strFileName))
        {
            Load(m_strFileName);
        }
    }


    void Update()
    {
        if(m_currentState == MEDIAPLAYER_STATE.PLAYING && _playback.IsReady())
        {
            var w = GetVideoWidth();
            var h = GetVideoHeight();

            if(m_currentWidth != w || m_currentHeight != h)
            {
                m_currentWidth = w;
                m_currentHeight = h;
                UpdateMaterials();
                Resize();
            }

            var texture = _playback.GetVideoTexture();
            if(texture != null && m_videoTexture != null)
            {
                int vW = m_videoTexture.width;
                int vH = m_videoTexture.height;

                int srcX = 0;
                int srcY = (texture.height - vH)/2;
                int srcWidth = texture.width;
                int srcHeight = m_videoTexture.height;

                if(vW < vH)
                {
                    srcY = 0;
                    srcX = (texture.width - vW) / 2;
                    srcWidth = m_videoTexture.width;
                    srcHeight = texture.height;
                }

                Graphics.CopyTexture(texture, 0, 0, srcX, srcY, srcWidth, srcHeight, m_videoTexture, 0, 0, 0, 0);
                //Graphics.ConvertTexture(texture, m_videoTexture);
            }
        }
    }

    private void UpdateMaterials()
    {
        if (m_TargetMaterial != null)
        {
            for (int i = 0; i < m_TargetMaterial.Length; i++)
            {
                if (m_TargetMaterial[i] == null)
                    continue;

                var mr = m_TargetMaterial[i].GetComponent<MeshRenderer>();
                if (mr != null)
                {
                    if (mr.material.shader.name.IndexOf("YUV") != -1)
                    {
                        var shader = Shader.Find("Unlit/Texture");
                        var material = new Material(shader);

                        mr.material = material;
                    }

                    int vW = (int)_playback.GetVideoWidth();
                    int vH = (int)_playback.GetVideoHeight();
                    var vt = _playback.GetVideoTexture();
                    int vtW = vt.width;
                    int vtH = vt.height;

                    if(vW >= vH)
                    {
                        vtH = (int)((double)vH / vW * vtW);
                    }
                    else
                    {
                        vtW = (int)((double)vW / vH * vtH);
                    }

                    

                    DeleteVideoTexture();
                    m_videoTexture = new Texture2D(vtW, vtH, TextureFormat.BGRA32, false);
                    m_videoTexture.filterMode = FilterMode.Bilinear;
                    m_videoTexture.wrapMode = TextureWrapMode.Clamp;
                    m_videoTexture.Apply();
                    var t = m_videoTexture;

                    //var t = _playback.GetVideoTexture();

                    if (mr.material.mainTexture != t)
                    {
                        mr.material.mainTexture = t;
                    }
                }

            }
        }
    }

    private void _playback_PlaybackStateChanged(object sender, ChangedEventArgs<PlaybackState> state)
    {
        switch(state.CurrentState)
        {
            case PlaybackState.None:
                m_currentState = MEDIAPLAYER_STATE.NOT_READY;
                m_currentWidth = 0;
                m_currentHeight = 0;
                break;
            case PlaybackState.Playing:
                m_currentState = MEDIAPLAYER_STATE.PLAYING;
                break;
            case PlaybackState.Paused:
                m_currentState = MEDIAPLAYER_STATE.PAUSED;
                break;
            case PlaybackState.Ended:
                m_currentState = MEDIAPLAYER_STATE.END;
                m_currentWidth = 0;
                m_currentHeight = 0;
                if(OnEnd != null)
                    OnEnd();
                break;
            default:
                break;
        }

        
    }

    private void _playback_PlaybackFailed(object sender, long errorCode)
    {
        OnError(MEDIAPLAYER_ERROR.MEDIA_ERROR_UNKNOWN, MEDIAPLAYER_ERROR.MEDIA_ERROR_UNKNOWN);
    }


    void OnApplicationQuit()
    {
    }

    private void OnDestroy()
    {
        Stop();
        DeleteVideoTexture();
    }

    void OnDisable()
    {
        if (this.GetCurrentState() == MEDIAPLAYER_STATE.PLAYING)
        {
            this.Pause();
        }
    }


    private void OnError(MEDIAPLAYER_ERROR iCode, MEDIAPLAYER_ERROR iCodeExtra)
    {
        string strError = "";

        switch (iCode)
        {
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
                strError = "MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK";
                break;
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_SERVER_DIED:
                strError = "MEDIA_ERROR_SERVER_DIED";
                break;
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_UNKNOWN:
                strError = "MEDIA_ERROR_UNKNOWN";
                break;
            default:
                strError = "Unknown error " + iCode;
                break;
        }

        strError += " ";

        switch (iCodeExtra)
        {
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_IO:
                strError += "MEDIA_ERROR_IO";
                break;
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_MALFORMED:
                strError += "MEDIA_ERROR_MALFORMED";
                break;
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_TIMED_OUT:
                strError += "MEDIA_ERROR_TIMED_OUT";
                break;
            case MEDIAPLAYER_ERROR.MEDIA_ERROR_UNSUPPORTED:
                strError += "MEDIA_ERROR_UNSUPPORTED";
                break;
            default:
                strError = "Unknown error " + iCode;
                break;
        }

        m_currentState = MEDIAPLAYER_STATE.ERROR;

        UnityEngine.Debug.LogError(strError);

        if (OnVideoError != null)
        {
            OnVideoError(iCode, iCodeExtra);
        }
    }


    private void OnApplicationPause(bool bPause)
    {
        UnityEngine.Debug.Log("ApplicationPause : " + bPause);
        if (bPause)
        {
            if (m_currentState == MEDIAPLAYER_STATE.PLAYING)
            {
                Pause();
                this.m_bNeedResume = true;
            }
        }
        else
        {
            if (this.m_bNeedResume)
            {
                if(_playback != null && m_currentState == MEDIAPLAYER_STATE.PAUSED)
                    Play();
                this.m_bNeedResume = false;
            }
        }
    }


    
}

#endif