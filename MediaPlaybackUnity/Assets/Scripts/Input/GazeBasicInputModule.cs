//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

using System;
using System.Text;
using UnityEngine;

namespace UnityEngine.EventSystems
{
    using UnityEngine;
    using UnityEngine.EventSystems;
    using System.Collections.Generic;

    public class GazeBasicInputModule : PointerInputModule
    {
        public enum Mode { SubmitOrClick = 0, Gaze };
        public Mode mode;

        [Header("Click Settings")]
        public string SubmitInputName = "Submit";
        [Header("Gaze Settings")]
        public float GazingTime = 2f;

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
                    currentLookAtHandlerClickTime = Time.realtimeSinceStartup + GazingTime;
                }

                // if we have a handler and it's time to click, do it now
                if (currentLookAtHandler != null &&
                    (mode == Mode.Gaze && Time.realtimeSinceStartup > currentLookAtHandlerClickTime) ||
                    (mode == Mode.SubmitOrClick && Input.GetButtonDown(SubmitInputName)))
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