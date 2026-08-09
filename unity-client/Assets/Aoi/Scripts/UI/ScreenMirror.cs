using UnityEngine;

// Renders the panel's render texture into the Unity game view (the desktop
// window), so the window is never just black even though the UI itself lives
// in a VR overlay. Attached to an always-enabled camera with cullingMask 0
// (renders nothing itself; OnRenderImage replaces the output with the panel).
public class ScreenMirror : MonoBehaviour
{
    public RenderTexture source;

    void OnRenderImage(RenderTexture src, RenderTexture dest)
    {
        if (source != null)
        {
            // The panel RT is Y-flipped for the VR overlay (UICamera's flipped
            // projection); flip it back here so the desktop window shows the
            // panel right-side up.
            Graphics.Blit(source, dest, new Vector2(1f, -1f), new Vector2(0f, 1f));
        }
        else
        {
            Graphics.Blit(src, dest);
        }
    }
}
