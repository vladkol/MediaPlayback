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

}
