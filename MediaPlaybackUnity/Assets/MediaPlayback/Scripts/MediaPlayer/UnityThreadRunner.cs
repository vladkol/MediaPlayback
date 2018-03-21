using System.Collections;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;


public delegate void UnityThreadCallback();

public class UnityThreadRunner : MonoBehaviour
{
    private class CallbackShot
    {
        public int requestingThreadId = -1;
        public UnityThreadCallback callback;
        public ManualResetEvent done;

        public CallbackShot(int requestingThreadId, UnityThreadCallback callback, bool signalEvent)
        {
            this.requestingThreadId = requestingThreadId;
            this.callback = callback;

            if (signalEvent)
                done = new ManualResetEvent(false);
        }
    }

    private static UnityThreadRunner _instance;
    private static Mutex _lock = new Mutex();
    private static int appThreadId = -1;

    private static Queue<CallbackShot> UpdateCalls = new Queue<CallbackShot>();
    private static Queue<CallbackShot> LateUpdatedCalls = new Queue<CallbackShot>();
    private static Queue<CallbackShot> FixedUpdatedCalls = new Queue<CallbackShot>();

    private bool bThis = false;

    [RuntimeInitializeOnLoadMethod]
    private static void RunnerInitialize()
    {
        appThreadId = Thread.CurrentThread.ManagedThreadId;
        GameObject gameObject = new GameObject();
        gameObject.name = "UnityThreadRunner";
        _instance = gameObject.AddComponent<UnityThreadRunner>();
        _instance.bThis = true;
    }

    private void Awake()
    {
        if (_instance == null)
        {
            _instance = this;
            bThis = true;
        }
    }

	// Update is called once per frame
	void Update ()
    {
        
		while(UpdateCalls.Count > 0)
        {
            var call = UpdateCalls.Dequeue();

            try
            {
                call.callback.Invoke();
            }
            finally
            {
                if (call.done != null)
                {
                    call.done.Set();
                }
            }
            
        }
	}

    private void OnDestroy()
    {
        if (bThis)
        {
            try
            {
                _lock.WaitOne();
                _instance = null;
                appThreadId = -1;
                UpdateCalls.Clear();
                LateUpdatedCalls.Clear();
                FixedUpdatedCalls.Clear();
            }
            finally
            {
                _lock.ReleaseMutex();
            }
        }
    }

    public static void CallOnUpdate(UnityThreadCallback callback, bool waitUntilDone)
    {
        int currentThread = Thread.CurrentThread.ManagedThreadId;
        if (currentThread == appThreadId)
        {
            callback.Invoke();
        }
        else
        {
            try
            {
                _lock.WaitOne();
                var callbackCall = new CallbackShot(Thread.CurrentThread.ManagedThreadId, callback, waitUntilDone);
                UpdateCalls.Enqueue(callbackCall);
                if(waitUntilDone)
                {
                    callbackCall.done.WaitOne();
                    callbackCall.done.Close();
                }
            }
            finally
            {
                _lock.ReleaseMutex();
            }
        }
    }


}
