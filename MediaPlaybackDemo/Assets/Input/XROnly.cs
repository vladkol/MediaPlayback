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

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class XROnly : MonoBehaviour
{

	void Awake ()
    {
#if UNITY_2017_2_OR_NEWER
        bool hasMR = UnityEngine.XR.XRDevice.isPresent;
#else
        bool hasMR = UnityEngine.VR.VRDevice.isPresent;
#endif
        if(!hasMR)
        {
            this.gameObject.SetActive(false);
        }
    }
}
	

