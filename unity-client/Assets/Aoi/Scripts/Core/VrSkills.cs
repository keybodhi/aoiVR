using System;
using UnityEngine;
using Valve.VR;

// VR skills: agent-callable OpenVR capabilities (mirrors OpenVR Advanced
// Settings' approach). Skills are executed on the Unity main thread and reply
// through the vr_skill_response channel.
//
// set_brightness: dims the VR view with a black overlay following the HMD
// (Advanced Settings uses the same technique: dimmer.png + SetOverlayAlpha,
// alpha = pow(1 - brightness, 1/3) for perceptual linearity). Only dimming is
// supported (brightness 0..1, 1 = original).
public static class VrSkills
{
    const string kBrightnessKey = "aoi.brightness";
    const string kBrightnessName = "Aoi Brightness";

    static ulong s_brightnessOverlay = OpenVR.k_ulOverlayHandleInvalid;
    static RenderTexture s_blackRT;
    static bool s_brightnessInit;

    // Brightness floor: never dim below 40% (avoid full black).
    const float kMinBrightness = 0.4f;

    // Called from AoiOrchestrator on the main thread with the raw vr_skill
    // payload. Returns the result JSON for the agent.
    public static string ApplySkill(string skill, string payloadJson)
    {
        try
        {
            switch (skill)
            {
                case "set_brightness":
                {
                    // AI-facing parameter is 0..1 (1 = original brightness =
                    // overlay off, 0 = dimmest). Redirect to the physical
                    // range [0.4, 1.0]: 0 -> 0.4 (max dim), 1 -> 1.0 (off).
                    float ai = Mathf.Clamp01(ExtractFloat(payloadJson, "brightness", 1f));
                    float b = kMinBrightness + (1f - kMinBrightness) * ai;
                    if (!SetBrightness(b))
                        return "{\"ok\":false,\"error\":\"OpenVR overlay unavailable\"}";
                    return "{\"ok\":true,\"message\":\"brightness " + ai.ToString("0.##") + "\"}";
                }
                default:
                    return "{\"ok\":false,\"error\":\"unknown vr skill: " + EscapeJson(skill) + "\"}";
            }
        }
        catch (Exception e)
        {
            return "{\"ok\":false,\"error\":\"" + EscapeJson(e.Message) + "\"}";
        }
    }

    static bool SetBrightness(float b)
    {
        if (!EnsureBrightnessOverlay()) return false;
        if (b >= 0.999f)
        {
            OpenVR.Overlay.HideOverlay(s_brightnessOverlay);
            return true;
        }
        // Perceptual linearity: the overlay alpha is the cube root of the
        // dimming amount (same curve as OpenVR Advanced Settings).
        float alpha = Mathf.Pow(1f - b, 1f / 3f);
        OpenVR.Overlay.SetOverlayAlpha(s_brightnessOverlay, alpha);
        return OpenVR.Overlay.ShowOverlay(s_brightnessOverlay) == EVROverlayError.None;
    }

    static bool EnsureBrightnessOverlay()
    {
        if (s_brightnessInit) return s_brightnessOverlay != OpenVR.k_ulOverlayHandleInvalid;
        s_brightnessInit = true;
        try
        {
            if (OpenVR.Overlay == null) return false;
            ulong handle = OpenVR.k_ulOverlayHandleInvalid;
            var err = OpenVR.Overlay.CreateOverlay(kBrightnessKey, kBrightnessName, ref handle);
            if (err != EVROverlayError.None || handle == OpenVR.k_ulOverlayHandleInvalid) return false;
            s_brightnessOverlay = handle;

            // Pure black texture (2x2 is enough for a solid fill).
            s_blackRT = new RenderTexture(2, 2, 0, RenderTextureFormat.ARGB32);
            s_blackRT.Create();
            var oldActive = RenderTexture.active;
            RenderTexture.active = s_blackRT;
            GL.Clear(true, true, Color.black);
            RenderTexture.active = oldActive;

            var texture = new Texture_t
            {
                handle = s_blackRT.GetNativeTexturePtr(),
                eType = ETextureType.DirectX,
                eColorSpace = EColorSpace.Auto,
            };
            OpenVR.Overlay.SetOverlayTexture(handle, ref texture);
            // Big plane in front of the HMD (9m wide = ~3x, fully covers the
            // FOV at 1.5m), drawn above everything else.
            OpenVR.Overlay.SetOverlayWidthInMeters(handle, 9f);
            OpenVR.Overlay.SetOverlaySortOrder(handle, 100);
            var m = new HmdMatrix34_t();
            m.m0 = 1f; m.m5 = 1f; m.m10 = 1f; m.m11 = -1.5f; // identity + 1.5m forward
            OpenVR.Overlay.SetOverlayTransformTrackedDeviceRelative(
                handle, OpenVR.k_unTrackedDeviceIndex_Hmd, ref m);
            OpenVR.Overlay.HideOverlay(handle);
            return true;
        }
        catch (Exception e)
        {
            Debug.LogError("[VrSkills] brightness overlay init failed: " + e.Message);
            return false;
        }
    }

    // Minimal JSON field extraction (matches AoiOrchestrator's ExtractJsonField).
    static float ExtractFloat(string json, string field, float fallback)
    {
        if (string.IsNullOrEmpty(json)) return fallback;
        string needle = "\"" + field + "\"";
        int i = json.IndexOf(needle, StringComparison.Ordinal);
        if (i < 0) return fallback;
        i += needle.Length;
        int colon = json.IndexOf(':', i);
        if (colon < 0) return fallback;
        int start = colon + 1;
        while (start < json.Length && (json[start] == ' ' || json[start] == '\t')) start++;
        int end = start;
        while (end < json.Length && (char.IsDigit(json[end]) || json[end] == '.' || json[end] == '-' ||
                                     json[end] == 'e' || json[end] == 'E' || json[end] == '+'))
            end++;
        if (end == start) return fallback;
        float v;
        return float.TryParse(json.Substring(start, end - start),
                              System.Globalization.NumberStyles.Float,
                              System.Globalization.CultureInfo.InvariantCulture, out v)
            ? v : fallback;
    }

    static string EscapeJson(string s)
    {
        if (string.IsNullOrEmpty(s)) return "";
        return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", " ");
    }
}
