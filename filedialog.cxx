/*
 * filedialog.cxx - file/folder picker panel.
 */
#include "filedialog.h"

#include <cstdio>
#include <utility>

#include "fileutils.h"
#include "font.h"

namespace {
constexpr int kRows = 12;
constexpr int kPrevAction = 3000;
constexpr int kNextAction = 3001;
constexpr int kUpAction = 3002;
constexpr int kCloseAction = 3003;
constexpr int kItemActionBase = 3010;
} // namespace

void FileDialog::setSelectHandler(SelectHandler handler)
{
	m_select = std::move(handler);
}

void FileDialog::open(Mode mode, const std::string &dir)
{
	m_mode = mode;
	m_dir = dir;
	m_offset = 0;
	refresh();
}

void FileDialog::close()
{
	m_mode = Mode::None;
	m_dir.clear();
	m_offset = 0;
	m_entries.clear();
}

void FileDialog::refresh()
{
	m_entries = listDirectory(m_dir);
}

const char *FileDialog::title() const
{
	switch (m_mode)
	{
	case Mode::Map:
		return "Open map";
	case Mode::Project:
		return "Open project";
	case Mode::MapTexture:
		return "Open map texture";
	case Mode::Textures:
	default:
		return "Open textures";
	}
}

void FileDialog::draw(FlatRenderer &ui, const Mat4 &screenMatrix, const Font &smallFont,
                      const Font &largeFont, int viewTop, int focusedControl)
{
	if (!active())
		return;

	refresh();

	const float panelW = 340.0f;
	const float panelH = 356.0f;
	const float x0 = 10.0f;
	const float y1 = (float)viewTop - 10.0f;
	const float y0 = y1 - panelH;
	const float rowHeight = 22.0f;
	const float top = y1 - 34.0f;

	std::vector<Button> dialogButtons;
	auto addButton = [&](int id, const Rect &rect, const char *label) {
		Button button(id, rect, label, false);
		button.setFocused(focusedControl == id);
		button.registerHit(ui);
		dialogButtons.push_back(std::move(button));
	};

	ui.begin();
	Panel panel(kUiBlockId, Rect(x0, y0, x0 + panelW, y1));
	panel.draw(ui);
	panel.registerHit(ui);

	int shown = 0;
	for (size_t i = (size_t)m_offset; i < m_entries.size() && shown < kRows; ++i, ++shown)
	{
		float y = top - (float)(shown + 1) * rowHeight;
		bool isDir = m_entries[i].second;
		ui.quad(x0 + 6, y, x0 + panelW - 6, y + rowHeight - 2, isDir ? 0.20f : 0.16f,
		        isDir ? 0.28f : 0.19f, isDir ? 0.40f : 0.25f, 1.0f);
		HitRegion row(kItemActionBase + shown, Rect(x0 + 6, y, x0 + panelW - 6, y + rowHeight - 2));
		row.registerHit(ui);
	}

	addButton(kPrevAction, Rect(x0 + 6, y0 + 6, x0 + 36, y0 + 26), "<");
	addButton(kNextAction, Rect(x0 + 40, y0 + 6, x0 + 70, y0 + 26), ">");
	addButton(kUpAction, Rect(x0 + 74, y0 + 6, x0 + 108, y0 + 26), "Up");
	addButton(kCloseAction, Rect(x0 + panelW - 40, y0 + 6, x0 + panelW - 6, y0 + 26), "Close");

	ui.flush(screenMatrix, 0);

	ui.begin();
	for (size_t i = 0; i < dialogButtons.size(); ++i)
		dialogButtons[i].drawBackground(ui);
	ui.flush(screenMatrix, 0);

	ui.begin();
	ui.text(largeFont, title(), x0 + 8, y1 - 16, 1.0f, 1.0f, 1.0f, 1.0f);
	ui.flush(screenMatrix, largeFont.ok() ? largeFont.atlasTexture() : 0);

	ui.begin();
	ui.text(smallFont, m_dir.c_str(), x0 + 8, y1 - 30, 0.7f, 0.78f, 0.85f, 1.0f);

	for (size_t i = (size_t)m_offset; i < m_entries.size() && (int)(i - (size_t)m_offset) < kRows;
	     ++i)
	{
		int row = (int)(i - (size_t)m_offset);
		float y = top - (float)(row + 1) * rowHeight;
		char label[256];
		snprintf(label, sizeof(label), "%s%s", m_entries[i].first.c_str(),
		         m_entries[i].second ? "/" : "");
		ui.text(smallFont, label, x0 + 12, y + 6, 0.95f, 0.95f, 0.95f, 1.0f);
	}

	char count[64];
	snprintf(count, sizeof(count), "%d entries", (int)m_entries.size());
	ui.text(smallFont, count, x0 + 8, y0 + 34, 0.6f, 0.7f, 0.8f, 1.0f);
	ui.flush(screenMatrix, smallFont.ok() ? smallFont.atlasTexture() : 0);

	ui.begin();
	for (size_t i = 0; i < dialogButtons.size(); ++i)
		dialogButtons[i].drawLabel(ui, smallFont);
	ui.flush(screenMatrix, smallFont.ok() ? smallFont.atlasTexture() : 0);
}

bool FileDialog::handleClick(int action)
{
	if (!active())
		return false;

	if (action == kPrevAction)
	{
		m_offset = m_offset > kRows ? m_offset - kRows : 0;
		return true;
	}
	if (action == kNextAction)
	{
		if (m_offset + kRows < (int)m_entries.size())
			m_offset += kRows;
		return true;
	}
	if (action == kUpAction)
	{
		std::string parent = parentDirectory(m_dir);
		if (!parent.empty())
		{
			m_dir = parent;
			m_offset = 0;
			refresh();
		}
		return true;
	}
	if (action == kCloseAction)
	{
		close();
		return true;
	}

	if (action < kItemActionBase || action >= kItemActionBase + kRows)
		return false;

	int slot = action - kItemActionBase;
	int index = m_offset + slot;
	if (index < 0 || index >= (int)m_entries.size())
		return true;

	const std::string &name = m_entries[index].first;
	bool isDir = m_entries[index].second;
	std::string full = joinPath(m_dir, name);

	if (isDir)
	{
		// In project mode a folder that holds a project is selected instead of
		// being entered, so clicking a project opens it.
		if (m_mode == Mode::Project)
		{
			FILE *probe = fopen(joinPath(full, "map.smf").c_str(), "rb");
			if (probe)
			{
				fclose(probe);
				Mode mode = m_mode;
				close();
				if (m_select)
					m_select(mode, full);
				return true;
			}
		}

		m_dir = full;
		m_offset = 0;
		refresh();
		return true;
	}

	Mode mode = m_mode;
	close();
	if (m_select)
		m_select(mode, full);
	return true;
}
