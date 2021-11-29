#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/Localization.h"

class LocalizationDocument : public IDocument
{
public:
    LocalizationDocument(GuiState* state);
    ~LocalizationDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

private:
    Locale selectedLocale_;

    //Search bar data
    string searchTerm_ = ""; //Search string
    bool searchByHash_ = false; //If true match entries hash with the hash of the search string
    u32 searchHash_; //Hash of the search string. Only used if searchByHash_ == true
};
