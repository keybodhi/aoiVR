Shader "Jarvis/Holographic"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "white" {}
        _Color ("Color Tint", Color) = (0, 0.8, 1, 1)
        _EdgeGlow ("Edge Glow", Range(0, 1)) = 0.3
        _ScanlineSpeed ("Scanline Speed", Range(0, 5)) = 1.0
        _Alpha ("Alpha", Range(0, 1)) = 0.85
    }

    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" "RenderPipeline"="UniversalPipeline" }
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off
        Cull Off

        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            struct Attributes
            {
                float4 positionOS : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct Varyings
            {
                float4 positionCS : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            TEXTURE2D(_MainTex);
            SAMPLER(sampler_MainTex);
            float4 _MainTex_ST;
            half4 _Color;
            half _EdgeGlow;
            half _ScanlineSpeed;
            half _Alpha;

            Varyings vert(Attributes input)
            {
                Varyings output;
                output.positionCS = TransformObjectToHClip(input.positionOS.xyz);
                output.uv = TRANSFORM_TEX(input.uv, _MainTex);
                return output;
            }

            half4 frag(Varyings input) : SV_Target
            {
                half4 tex = SAMPLE_TEXTURE2D(_MainTex, sampler_MainTex, input.uv);
                half distToEdge = min(min(input.uv.x, 1 - input.uv.x), min(input.uv.y, 1 - input.uv.y));
                half edgeGlow = pow(saturate(1 - distToEdge * 4), 2) * _EdgeGlow;

                half scanline = sin(input.uv.y * 200 + _Time.y * _ScanlineSpeed) * 0.5 + 0.5;
                scanline = lerp(0.85, 1.0, scanline);

                half4 final = tex * _Color;
                final.rgb += edgeGlow * _Color.rgb;
                final.rgb *= scanline;
                final.a = _Alpha;

                return final;
            }
            ENDHLSL
        }
    }
}
