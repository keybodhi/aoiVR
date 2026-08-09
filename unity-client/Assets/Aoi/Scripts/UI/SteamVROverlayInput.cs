using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
using Valve.VR;

public class SteamVROverlayInput : BaseInput
{
    [HideInInspector] public int textureWidth = 1024;
    [HideInInspector] public int textureHeight = 1024;

    private Vector2 mousePos;
    private bool mousePressed;
    private PointerEventData activeDrag = null;
    private GameObject dragTarget = null;

    public void HandleVREvent(VREvent_t vrEvent)
    {
        switch ((EVREventType)vrEvent.eventType)
        {
            case EVREventType.VREvent_MouseMove:
                mousePos.x = vrEvent.data.mouse.x;
                mousePos.y = textureHeight - vrEvent.data.mouse.y;
                UpdateDrag();
                break;

            case EVREventType.VREvent_MouseButtonDown:
                mousePressed = true;
                ProcessManualClick();
                break;

            case EVREventType.VREvent_MouseButtonUp:
                mousePressed = false;
                EndDrag();
                break;

            case EVREventType.VREvent_ScrollSmooth:
                ApplyScroll(vrEvent.data.scroll.xdelta, vrEvent.data.scroll.ydelta);
                break;

            case EVREventType.VREvent_ScrollDiscrete:
                ApplyScroll(vrEvent.data.scroll.xdelta, vrEvent.data.scroll.ydelta);
                break;
        }
    }

    // Forward vertical scroll to a ScrollRect under the cursor, if any.
    void ApplyScroll(float dx, float dy)
    {
        var es = EventSystem.current;
        if (es == null || Mathf.Approximately(dy, 0f)) return;
        var scrollRect = FindScrollRectAt(mousePos);
        if (scrollRect == null) return;
        var v = scrollRect.verticalNormalizedPosition;
        scrollRect.verticalNormalizedPosition = Mathf.Clamp01(v + dy * 0.05f);
    }

    ScrollRect FindScrollRectAt(Vector2 pos)
    {
        var es = EventSystem.current;
        if (es == null) return null;
        var pe = new PointerEventData(es) { position = pos };
        var results = new System.Collections.Generic.List<RaycastResult>();
        es.RaycastAll(pe, results);
        foreach (var r in results)
        {
            var sr = r.gameObject.GetComponentInParent<ScrollRect>();
            if (sr != null) return sr;
        }
        return null;
    }

    // Drag support for ScrollRect: while the trigger is held, update the drag
    // delta so the ScrollRect moves with the laser.
    void UpdateDrag()
    {
        if (activeDrag == null || dragTarget == null) return;
        activeDrag.position = mousePos;
        ExecuteEvents.Execute(dragTarget, activeDrag, ExecuteEvents.dragHandler);
    }

    public void EndDrag()
    {
        if (activeDrag != null && dragTarget != null)
        {
            ExecuteEvents.Execute(dragTarget, activeDrag, ExecuteEvents.endDragHandler);
        }
        activeDrag = null;
        dragTarget = null;
    }

    // Reset a press whose MouseButtonUp was never delivered (SteamVR dashboard
    // stole the pointer while the trigger was held).
    public void CancelPress()
    {
        mousePressed = false;
        EndDrag();
    }

    // Directly raycast and fire UI events at the laser position.
    // (EventSystem's StandaloneInputModule reads input earlier in the frame,
    // before PollNextOverlayEvent sets state, so the event path is unreliable.
    // We fire events ourselves and keep GetMouseButton* disabled to avoid double-trigger.)
    void ProcessManualClick()
    {
        var es = EventSystem.current;
        if (es == null) return;
        var pe = new PointerEventData(es)
        {
            position = mousePos,
            button = PointerEventData.InputButton.Left,
            clickCount = 1,
        };
        var results = new System.Collections.Generic.List<RaycastResult>();
        es.RaycastAll(pe, results);
        foreach (var r in results)
        {
            if (r.gameObject == null) continue;
            if (r.gameObject.GetComponentInParent<ScrollRect>() != null && r.gameObject.GetComponent<Button>() == null)
            {
                // Pressed on a scroll area: begin a drag to scroll.
                dragTarget = r.gameObject;
                activeDrag = pe;
                ExecuteEvents.Execute(dragTarget, activeDrag, ExecuteEvents.initializePotentialDrag);
                ExecuteEvents.Execute(dragTarget, activeDrag, ExecuteEvents.beginDragHandler);
                return;
            }
            ExecuteEvents.Execute(r.gameObject, pe, ExecuteEvents.pointerClickHandler);
        }
    }

    // Simulate a click at an overlay UV coordinate (0..1). The orchestrator calls
    // this when its custom laser beam hits the panel and the controller trigger
    // is pressed. UV (0,0) is bottom-left, so flip Y to match Unity's top-left.
    public void SimulateClick(float u, float v)
    {
        mousePos.x = u * textureWidth;
        mousePos.y = (1f - v) * textureHeight;
        ProcessManualClick();
    }

    // Update the pointer to the laser's current UV while the trigger is held.
    // This keeps an active ScrollRect drag tracking the laser movement.
    public void UpdatePointer(float u, float v)
    {
        mousePos.x = u * textureWidth;
        mousePos.y = (1f - v) * textureHeight;
        UpdateDrag();
    }

    public override Vector2 mousePosition
    {
        get
        {
            if (AoiOrchestrator.DesktopMode) return (Vector2)Input.mousePosition;
            if (AoiOrchestrator.WindowMirrorActive)
            {
                // Window coords -> panel RT coords: the WorldSpace canvas is
                // raycast through the RT camera (1024x1024), whose projection is
                // Y-flipped for the VR overlay — flip the mouse Y back so a click
                // on the lower half hits the lower button (▲▼ orientation).
                var mp = (Vector2)Input.mousePosition;
                if (Screen.width <= 0 || Screen.height <= 0) return mp;
                return new Vector2(
                    mp.x * AoiOrchestrator.PanelTexWidth / (float)Screen.width,
                    (Screen.height - mp.y) * AoiOrchestrator.PanelTexHeight / (float)Screen.height);
            }
            return mousePos;
        }
    }
    public override bool mousePresent => true;

    public override bool GetMouseButtonDown(int button)
    {
        if (AoiOrchestrator.DesktopMode || AoiOrchestrator.WindowMirrorActive)
            return button == 0 && Input.GetMouseButtonDown(0);
        return false; // click handled manually in HandleVREvent
    }

    public override bool GetMouseButtonUp(int button)
    {
        if (AoiOrchestrator.DesktopMode || AoiOrchestrator.WindowMirrorActive)
            return button == 0 && Input.GetMouseButtonUp(0);
        return false;
    }

    public override bool GetMouseButton(int button)
    {
        if (AoiOrchestrator.DesktopMode || AoiOrchestrator.WindowMirrorActive) return button == 0 && Input.GetMouseButton(0);
        return button == 0 && mousePressed;
    }
}
