#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"

struct TextureDocumentData
{
    string Filename;
    string ParentName;
    string ExtractionPath;
    //Peg10 Peg;
};

void TextureDocument_Init(GuiState* state, Document& doc)
{

}

void TextureDocument_Update(GuiState* state, Document& doc)
{
    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }

    TextureDocumentData* data = (TextureDocumentData*)doc.Data;
    ImGui::Text("Texture document");
    gui::LabelAndValue("Filename:", data->Filename);
    gui::LabelAndValue("Parent name:", data->ParentName);
    gui::LabelAndValue("Extraction path:", data->ExtractionPath);

    ImGui::End();
}

//struct Document;
//class GuiState;
////Function signature for document init and update functions
//using DocumentInitFunction = void(GuiState* state, Document& doc);
//using DocumentUpdateFunc = void(GuiState* state, Document& doc);
//
////Document instance. Destructor auto deletes the Data* you provide
//struct Document
//{
//    Document(string title, DocumentInitFunction* init, DocumentUpdateFunc* update, void* data = nullptr)
//        : Title(title), Init(init), Update(update), Data(data) {}
//    ~Document() { delete Data; }
//
//    string Title;
//    DocumentInitFunction* Init = nullptr;
//    DocumentUpdateFunc* Update = nullptr;
//    void* Data = nullptr;
//    bool Open = true;
//};