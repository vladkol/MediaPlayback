using System;
using System.Text;
using UnityEngine;

namespace UnityEngine.EventSystems
{
    // Gaze Input Module by Peter Koch <peterept@gmail.com>
    using UnityEngine;
    using UnityEngine.EventSystems;
    using System.Collections.Generic;

    // To use:
    // 1. Drag onto your EventSystem game object.
    // 2. Make sure your Canvas is in world space and has a GraphicRaycaster (should by default).
    // 3. If you have multiple cameras then make sure to drag your VR (center eye) camera into the canvas.
    public class GazeBasicInputModule : PointerInputModule
    {
        public enum Mode { Click = 0, Gaze };
        public Mode mode;

        [Header("Click Settings")]
        public string ClickInputName = "Submit";
        [Header("Gaze Settings")]
        public float GazeTimeInSeconds = 2f;

        public RaycastResult CurrentRaycast;

        private PointerEventData pointerEventData;
        private GameObject currentLookAtHandler;
        private float currentLookAtHandlerClickTime;

        [SerializeField]
        private bool m_ForceModuleActive;

        private bool vrPresent
        {
            get
            {
#if UNITY_2017_2_OR_NEWER
                return UnityEngine.XR.XRDevice.isPresent;
#else
                return UnityEngine.VR.VRDevice.isPresent;
#endif
            }
        }

        public bool forceModuleActive
        {
            get { return m_ForceModuleActive; }
            set { m_ForceModuleActive = value; }
        }

        public override bool IsModuleSupported()
        {
            return vrPresent || m_ForceModuleActive;
        }

        public override bool ShouldActivateModule()
        {
            if (!base.ShouldActivateModule())
                return false;

            if (vrPresent || m_ForceModuleActive)
                return true;

            return false;
        }


        protected override void Start()
        {
            base.Start();

            if(ShouldActivateModule())
            {
                var sim = GetComponent<StandaloneInputModule>();
                if(sim != null)
                {
                    sim.DeactivateModule();
                    sim.enabled = false;
                }
            }
        }


        public override void Process()
        {
            HandleLook();
            HandleSelection();
        }

        void HandleLook()
        {
            if (pointerEventData == null)
            {
                pointerEventData = new PointerEventData(eventSystem);
            }
            // fake a pointer always being at the center of the screen
#if UNITY_2017_2_OR_NEWER
            pointerEventData.position = new Vector2(UnityEngine.XR.XRSettings.eyeTextureWidth / 2, UnityEngine.XR.XRSettings.eyeTextureHeight / 2);
#else
            pointerEventData.position = new Vector2(UnityEngine.VR.VRSettings.eyeTextureWidth / 2, UnityEngine.VR.VRSettings.eyeTextureHeight / 2);
#endif
            pointerEventData.delta = Vector2.zero;
            List<RaycastResult> raycastResults = new List<RaycastResult>();
            eventSystem.RaycastAll(pointerEventData, raycastResults);
            CurrentRaycast = pointerEventData.pointerCurrentRaycast = FindFirstRaycast(raycastResults);
            ProcessMove(pointerEventData);
        }

        void HandleSelection()
        {
            if (pointerEventData.pointerEnter != null)
            {
                // if the ui receiver has changed, reset the gaze delay timer
                GameObject handler = ExecuteEvents.GetEventHandler<IPointerClickHandler>(pointerEventData.pointerEnter);
                if (currentLookAtHandler != handler)
                {
                    currentLookAtHandler = handler;
                    currentLookAtHandlerClickTime = Time.realtimeSinceStartup + GazeTimeInSeconds;
                }

                // if we have a handler and it's time to click, do it now
                if (currentLookAtHandler != null &&
                    (mode == Mode.Gaze && Time.realtimeSinceStartup > currentLookAtHandlerClickTime) ||
                    (mode == Mode.Click && Input.GetButtonDown(ClickInputName)))
                {
                    ExecuteEvents.ExecuteHierarchy(currentLookAtHandler, pointerEventData, ExecuteEvents.pointerClickHandler);
                    currentLookAtHandlerClickTime = float.MaxValue;
                }
            }
            else
            {
                currentLookAtHandler = null;
            }
        }


    }
}