using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
using TMPro;

// Hover highlight for panel buttons, matching the mockup's :hover rules
// (scroll buttons turn yellow, toolbar button turns cyan, both with the
// border sprite and the label recolored together).
public class UiHoverColor : MonoBehaviour, IPointerEnterHandler, IPointerExitHandler
{
    public Color hoverColor = new Color(0.953f, 0.902f, 0f);
    public Graphic[] graphics;
    public TextMeshProUGUI[] texts;

    private Color[] gColors;
    private Color[] tColors;

    void Awake()
    {
        if (graphics != null)
        {
            gColors = new Color[graphics.Length];
            for (int i = 0; i < graphics.Length; i++)
                if (graphics[i] != null) gColors[i] = graphics[i].color;
        }
        if (texts != null)
        {
            tColors = new Color[texts.Length];
            for (int i = 0; i < texts.Length; i++)
                if (texts[i] != null) tColors[i] = texts[i].color;
        }
    }

    public void OnPointerEnter(PointerEventData eventData)
    {
        if (graphics != null)
            foreach (var g in graphics)
                if (g != null) g.color = hoverColor;
        if (texts != null)
            foreach (var t in texts)
                if (t != null) t.color = hoverColor;
    }

    public void OnPointerExit(PointerEventData eventData)
    {
        if (graphics != null)
            for (int i = 0; i < graphics.Length; i++)
                if (graphics[i] != null) graphics[i].color = gColors[i];
        if (texts != null)
            for (int i = 0; i < texts.Length; i++)
                if (texts[i] != null) texts[i].color = tColors[i];
    }
}
