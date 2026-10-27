/*
 * controls.cxx - shared drawing and hit registration for the editor controls.
 */
#include "controls.h"

#include <utility>

#include "font.h"

Rect::Rect(float x0, float y0, float x1, float y1) : x0(x0), y0(y0), x1(x1), y1(y1) {}

bool Rect::contains(float x, float y) const
{
	return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

void Control::drawFocus(FlatRenderer &ui) const
{
	if (!m_focused)
		return;
	ui.outline(m_rect.x0 - 2.0f, m_rect.y0 - 2.0f, m_rect.x1 + 2.0f, m_rect.y1 + 2.0f, 2.0f, 1.0f,
	           0.78f, 0.25f, 1.0f);
}

void Panel::draw(FlatRenderer &ui) const
{
	ui.quad(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 0.16f, 0.18f, 0.22f, 1.0f);
	ui.outline(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 1.0f, 0.40f, 0.44f, 0.50f, 1.0f);
}

Button::Button(int id, const Rect &rect, std::string label, bool active, const ButtonStyle &style)
    : Control(id, rect), m_label(std::move(label)), m_active(active), m_style(style)
{}

void Button::drawBackground(FlatRenderer &ui) const
{
	float r = m_active ? m_style.activeR : m_style.r;
	float g = m_active ? m_style.activeG : m_style.g;
	float b = m_active ? m_style.activeB : m_style.b;
	ui.quad(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, r, g, b, 1.0f);
	ui.outline(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 1.0f, m_style.outlineR, m_style.outlineG,
	           m_style.outlineB, 1.0f);
	drawFocus(ui);
}

void Button::drawLabel(FlatRenderer &ui, const Font &font) const
{
	if (m_label.empty())
		return;
	float tw = font.textWidth(m_label.c_str());
	ui.text(font, m_label.c_str(), m_rect.x0 + (m_rect.width() - tw) * 0.5f,
	        m_rect.y0 + (m_rect.height() - font.ascent()) * 0.5f, 1.0f, 1.0f, 1.0f, 1.0f);
}

void TextField::drawBackground(FlatRenderer &ui) const
{
	if (m_focused)
	{
		ui.quad(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 0.24f, 0.27f, 0.31f, 1.0f);
		ui.outline(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 2.0f, 1.0f, 0.78f, 0.25f, 1.0f);
	} else
	{
		ui.quad(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 0.22f, 0.24f, 0.28f, 1.0f);
		ui.outline(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 1.0f, 0.45f, 0.50f, 0.55f, 1.0f);
	}
}

void Slider::draw(FlatRenderer &ui, float value) const
{
	value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
	ui.quad(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 0.18f, 0.20f, 0.24f, 1.0f);
	ui.quad(m_rect.x0, m_rect.y0, m_rect.x0 + m_rect.width() * value, m_rect.y1, 0.35f, 0.55f,
	        0.75f, 1.0f);
	ui.outline(m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1, 1.0f, 0.40f, 0.45f, 0.50f, 1.0f);
	drawFocus(ui);
}
