/*
 * controls.h - small control hierarchy shared by the editor UI.
 *
 * Every control owns its rectangle and registers its own hit area, so the
 * drawing code no longer has to duplicate the bounds in ui.addHit calls.
 */
#ifndef CONTROLS_H
#define CONTROLS_H

#include <string>

#include "ui.h"

class Font;

// Hit id for inert backgrounds and modal blockers. It is larger than every
// action range, so it can never be mistaken for a control action.
constexpr int kUiBlockId = 30000;

struct Rect {
	float x0 = 0.0f;
	float y0 = 0.0f;
	float x1 = 0.0f;
	float y1 = 0.0f;

	Rect() = default;
	Rect(float x0, float y0, float x1, float y1);

	bool contains(float x, float y) const;
	float width() const
	{
		return x1 - x0;
	}
	float height() const
	{
		return y1 - y0;
	}
};

class Control {
  public:
	Control(int id, const Rect &rect) : m_id(id), m_rect(rect) {}
	virtual ~Control() = default;

	int id() const
	{
		return m_id;
	}
	const Rect &rect() const
	{
		return m_rect;
	}
	void setRect(const Rect &rect)
	{
		m_rect = rect;
	}

	bool contains(float x, float y) const
	{
		return m_rect.contains(x, y);
	}
	void registerHit(FlatRenderer &ui) const
	{
		ui.addHit(m_id, m_rect.x0, m_rect.y0, m_rect.x1, m_rect.y1);
	}

	bool focused() const
	{
		return m_focused;
	}
	void setFocused(bool focused)
	{
		m_focused = focused;
	}

  protected:
	void drawFocus(FlatRenderer &ui) const;

	int m_id;
	Rect m_rect;
	bool m_focused = false;
};

class Panel : public Control {
  public:
	using Control::Control;
	void draw(FlatRenderer &ui) const;
};

struct ButtonStyle {
	float r = 0.20f;
	float g = 0.24f;
	float b = 0.28f;
	float outlineR = 0.45f;
	float outlineG = 0.50f;
	float outlineB = 0.55f;
	float activeR = 0.30f;
	float activeG = 0.48f;
	float activeB = 0.62f;
};

class Button : public Control {
  public:
	Button(int id, const Rect &rect, std::string label, bool active,
	       const ButtonStyle &style = ButtonStyle());

	void drawBackground(FlatRenderer &ui) const;
	void drawLabel(FlatRenderer &ui, const Font &font) const;

	const std::string &label() const
	{
		return m_label;
	}

  private:
	std::string m_label;
	bool m_active;
	ButtonStyle m_style;
};

class TextField : public Control {
  public:
	using Control::Control;
	void drawBackground(FlatRenderer &ui) const;
};

class Slider : public Control {
  public:
	using Control::Control;
	void draw(FlatRenderer &ui, float value) const;
};

// An inert clickable region, used for list rows, palette swatches and modal
// blockers that have custom drawing.
class HitRegion : public Control {
  public:
	using Control::Control;
};

#endif // CONTROLS_H
