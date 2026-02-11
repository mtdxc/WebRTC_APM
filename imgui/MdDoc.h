#pragma once
#include <map>
#include <string>
namespace ImGui {
    struct MarkdownImageData;
}

class MdDoc {
    std::string content_;
    std::string dir_, name_;
    std::map<std::string, ImGui::MarkdownImageData*> img_map_;
public:
    bool opened_ = false;
    static void LoadFonts(const char* path, float fontSize);

    bool Open(const char* path);
    bool getImage(const std::string& path, ImGui::MarkdownImageData& data);
    void Draw();
    void Close();
    ~MdDoc() {
        Close();
    }
};

std::string parentDir(std::string file);
FILE* FileOpen(const char* src, const char* mod, const char* dir = nullptr);
int ReadFileContent(FILE* fp, std::string& data);