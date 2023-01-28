#include "Popup.h"
#include "gui/GuiState.h"
#include <imgui.h>

Popup::Popup(std::string_view title, std::string_view content, PopupType type, bool modal) : title_(title), content_(content), type_(type), modal_(modal)
{

}

PopupResult Popup::Update(GuiState* state)
{
	if (open_)
		ImGui::OpenPopup(title_.c_str());
	else
		return PopupResult::None;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 8.0f }); //Manually set padding since the parent window might be a document with padding disabled
	PopupResult popupResult = PopupResult::None;
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f)); //Auto center
	bool openResult = modal_ ? ImGui::BeginPopupModal(title_.c_str(), &open_) : ImGui::BeginPopup(title_.c_str());
	if (openResult)
	{
		ImGui::TextWrapped(content_.c_str());
		if (type_ == PopupType::Ok)
		{
			if (firstFrame_) ImGui::SetKeyboardFocusHere(); //Auto focus on default option on first frame so you don't need to switch to the mouse
			if (ImGui::Button("Ok") || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
			{
				popupResult = PopupResult::Ok;
				ImGui::CloseCurrentPopup();
				Close();
			}
		}
		else if (type_ == PopupType::YesCancel)
		{
			if (firstFrame_) ImGui::SetKeyboardFocusHere(); //Auto focus on default option on first frame so you don't need to switch to the mouse
			if (ImGui::Button("Yes") || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
			{
				popupResult = PopupResult::Yes;
				ImGui::CloseCurrentPopup();
				Close();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))
				)
			{
				popupResult = PopupResult::Cancel;
				ImGui::CloseCurrentPopup();
				Close();
			}
		}
		else
		{
			LOG_ERROR("Invalid PopupType {} used by {}", (u64)type_, title_);
			ImGui::CloseCurrentPopup();
			Close();
		}

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	firstFrame_ = false;
	return popupResult;
}
