/*
 * Undo/redo support.
 *
 * Edits are grouped into strokes: one stroke is everything that happened
 * between pressing and releasing the paint button. Every stroke stores the
 * previous values of the map entries it touched, so undo restores them.
 */
#ifndef UNDOSTACK_H
#define UNDOSTACK_H

#include <vector>

enum EditType {
	EDIT_HEIGHT = 0, // one float per index, index is a height map entry
	EDIT_METAL = 1,  // one float per index, index is a metal map entry
	EDIT_PAINT = 2   // four floats per index, index is a paint texel
};

struct Stroke {
	int type = EDIT_HEIGHT;
	std::vector<int> index;
	std::vector<float> oldValue;

	bool empty() const
	{
		return index.empty();
	}
};

class UndoStack {
  public:
	// A new edit: pushes it onto the undo stack and clears the redo stack.
	void commit(const Stroke &stroke)
	{
		if (stroke.empty())
			return;
		undo.push_back(stroke);
		trim();
		redo.clear();
	}

	// Used by redo: puts a stroke back on the undo stack without clearing redo.
	void pushUndo(const Stroke &stroke)
	{
		if (stroke.empty())
			return;
		undo.push_back(stroke);
		trim();
	}

	void pushRedo(const Stroke &stroke)
	{
		if (stroke.empty())
			return;
		redo.push_back(stroke);
		if (redo.size() > maxStrokes)
			redo.erase(redo.begin());
	}

	bool canUndo() const
	{
		return !undo.empty();
	}
	bool canRedo() const
	{
		return !redo.empty();
	}
	size_t undoSize() const
	{
		return undo.size();
	}
	size_t redoSize() const
	{
		return redo.size();
	}

	Stroke undoLast()
	{
		Stroke s = undo.back();
		undo.pop_back();
		return s;
	}

	Stroke redoLast()
	{
		Stroke s = redo.back();
		redo.pop_back();
		return s;
	}

	void clear()
	{
		undo.clear();
		redo.clear();
	}

  private:
	// Bound both the number of strokes and the memory they use: a full map
	// stroke can hold hundreds of thousands of values.
	static const size_t maxStrokes = 128;
	static const size_t maxFloats = 4000000;
	std::vector<Stroke> undo;
	std::vector<Stroke> redo;

	size_t storedFloats() const
	{
		size_t total = 0;
		for (size_t i = 0; i < undo.size(); ++i)
			total += undo[i].index.size() + undo[i].oldValue.size();
		return total;
	}

	void trim()
	{
		while (undo.size() > maxStrokes || (undo.size() > 1 && storedFloats() > maxFloats))
			undo.erase(undo.begin());
	}
};

#endif // UNDOSTACK_H
