/*
 * filedialog.h - modal-style file/folder picker panel.
 */
#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "controls.h"

class Font;
class Mat4;

class FileDialog {
  public:
	enum class Mode { None, Map, Textures, Project, MapTexture };
	using SelectHandler = std::function<void(Mode, const std::string &path)>;

	void setSelectHandler(SelectHandler handler);
	void open(Mode mode, const std::string &dir);
	void close();

	bool active() const
	{
		return m_mode != Mode::None;
	}
	Mode mode() const
	{
		return m_mode;
	}

	// Draws the panel, its rows and its buttons, and registers all hit areas.
	void draw(FlatRenderer &ui, const Mat4 &screenMatrix, const Font &smallFont,
	          const Font &largeFont, int viewTop, int focusedControl);

	// Returns true when the action belongs to the file dialog.
	bool handleClick(int action);

  private:
	void refresh();
	const char *title() const;

	Mode m_mode = Mode::None;
	std::string m_dir;
	int m_offset = 0;
	std::vector<std::pair<std::string, bool>> m_entries;
	SelectHandler m_select;
};

#endif // FILEDIALOG_H
