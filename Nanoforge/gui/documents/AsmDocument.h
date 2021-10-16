#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "RfgTools++/formats/asm/AsmFile5.h"
#include <vector>

class AsmDocument final : public IDocument
{
public:
    AsmDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer);
    ~AsmDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

private:
    string filename_;
    string parentName_;
    string vppName_;
    bool inContainer_;

    AsmFile5* asmFile_ = nullptr;
};