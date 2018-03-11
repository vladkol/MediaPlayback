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

Shader "360 Video/360 XR Stereo Panorama"
{
	Properties
	{
		_MainTex("Spherical (HDR)", 2D) = "white" {}
		_Tint("Tint Color", Color) = (.5, .5, .5, .5)
		[Gamma] _Exposure("Exposure", Range(0, 8)) = 1.0
		[Toggle] _isStereo("Stereoscopic Mode", Float) = 0
	}

	SubShader
	{
		Tags{ "RenderType" = "Opaque" }
		Cull front
		LOD 100

		Pass
		{
			CGPROGRAM

#pragma vertex vert
#pragma fragment frag
#pragma target 2.0

#include "UnityCG.cginc"

			struct appdata_t 
			{
				float4 vertex : POSITION;
				float2 texcoord : TEXCOORD0;
				UNITY_VERTEX_INPUT_INSTANCE_ID
			};

			struct v2f 
			{
				float4 vertex : SV_POSITION;
				half2 texcoord : TEXCOORD0;
				UNITY_VERTEX_OUTPUT_STEREO
			};

			sampler2D _MainTex;
			float4 _MainTex_ST;
			half4 _MainTex_HDR;

			half4 _Tint;
			half _Exposure;
			half _isStereo; 

			v2f vert(appdata_t v)
			{
				v2f o;
				
				UNITY_SETUP_INSTANCE_ID(v);
				UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

				o.vertex = UnityObjectToClipPos(v.vertex);
				v.texcoord.x = 1 - v.texcoord.x;

				half2 texcoord = v.texcoord;

				if (_isStereo > 0)
				{
					if (unity_StereoEyeIndex > 0)
						texcoord.y = 0.5 + texcoord.y / 2;
					else
						texcoord.y = texcoord.y / 2;

					o.texcoord = TRANSFORM_TEX(texcoord, _MainTex);
				}

				return o;
			}

			fixed4 frag(v2f i) : SV_Target
			{
				half4 texHDR = _MainTex_HDR;
				half4 texCol = tex2D(_MainTex, i.texcoord);

				half3 color = DecodeHDR(texCol, texHDR);

				color = color * _Tint.rgb * unity_ColorSpaceDouble.rgb;
				color *= _Exposure;

				return half4(color, 1);
			}

			ENDCG
		}
	}
}