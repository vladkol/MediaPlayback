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

Shader "360 Video/360 XR Skybox"
{
	Properties
	{
		[NoScaleOffset]_MainTex("Spherical (HDR)", 2D) = "white" {}
		_Tint("Tint Color", Color) = (.5, .5, .5, .5)
		[Gamma] _Exposure("Exposure", Range(0, 8)) = 1.0
		[Toggle] _isStereo("Stereoscopic Mode", Float) = 0
	}

	SubShader
	{
		Tags{ "Queue" = "Background" "RenderType" = "Background" "PreviewType" = "Skybox" }
		Cull Off ZWrite Off

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
				UNITY_VERTEX_INPUT_INSTANCE_ID
			};

			struct v2f 
			{
				float4 vertex : SV_POSITION;
				float3 texcoord : TEXCOORD0;
				UNITY_VERTEX_OUTPUT_STEREO
			};

			sampler2D _MainTex;
			float4 _MainTex_ST;
			half4 _MainTex_HDR;

			half4 _Tint;
			half _Exposure;
			half _isStereo; 

			inline float2 ToRadialCoords(float3 coords)
			{
				float3 normalizedCoords = normalize(coords);
				float latitude = acos(normalizedCoords.y);
				float longitude = atan2(normalizedCoords.z, normalizedCoords.x);
				float2 sphereCoords = float2(longitude, latitude) * float2(0.5 / UNITY_PI, 1.0 / UNITY_PI);
				return float2(0.5, 1.0) - sphereCoords;
			}

			v2f vert(appdata_t v)
			{
				v2f o;
				
				UNITY_SETUP_INSTANCE_ID(v);
				UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

				o.vertex = UnityObjectToClipPos(v.vertex);
				float3 texcoord = v.vertex.xyz;
				texcoord.y = - texcoord.y;

				o.texcoord = texcoord;

				return o;
			}

			fixed4 frag(v2f i) : SV_Target
			{
				float4 layout3DScaleAndOffset = float4(0.25,0,1,1);
				
				if (_isStereo > 0)
				{
					layout3DScaleAndOffset = float4(0.25, unity_StereoEyeIndex,1,0.5);
				}

				float2 tc = ToRadialCoords(i.texcoord);
				tc.x = fmod(tc.x, 1);
				tc = (tc + layout3DScaleAndOffset.xy) * layout3DScaleAndOffset.zw;

				half4 texHDR = _MainTex_HDR;
				half4 texCol = tex2D(_MainTex, tc);

				half3 color = DecodeHDR(texCol, texHDR);

				color = color * _Tint.rgb * unity_ColorSpaceDouble.rgb;
				color *= _Exposure;

				return half4(color, 1);
			}

			ENDCG
		}
	}
}