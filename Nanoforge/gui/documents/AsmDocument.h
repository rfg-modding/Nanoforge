#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "application/Registry.h"

class AsmFile5;

class AsmDocument final : public IDocument
{
public:
    AsmDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer);
    ~AsmDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;
    bool CanClose() override { return true; }
    void OnClose(GuiState* state) override {}

private:
    string filename_;
    string parentName_;
    string vppName_;
    bool inContainer_;

    AsmFile5* asmFile_ = nullptr;
    ObjectHandle _asmFileObject = { nullptr };
    string search_;
};