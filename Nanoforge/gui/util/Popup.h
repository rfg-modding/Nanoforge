#pragma once
#include "common/Typedefs.h"
#include "common/String.h"

class GuiState;

//Result of the popup in the current frame
enum class PopupResult
{
	None, //No option selected. Popup still open.
	Ok,
	Cancel,
	Yes,
};

enum class PopupType
{
	Ok,
	YesCancel,
};

//Helper for dear imgui popups. They're a bit of pain to code so this makes it a bit simpler
class Popup
{
public:
	Popup(std::string_view title, std::string_view content, PopupType type, bool modal);

	//Should be called every frame it's parent UI is visible. Will draw the popup if it's open. Returns 
	PopupResult Update(GuiState* state);
	string Title() const { return title_; }
	bool IsOpen() const { return open_; }
	void Open() { open_ = true; firstFrame_ = true; }
	void Close() { open_ = false; }

private:
	string title_;
	string content_;
	bool open_ = false;
	PopupType type_ = PopupType::Ok;
	bool modal_ = true;
	bool firstFrame_ = false;
};