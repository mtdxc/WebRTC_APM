#pragma once
#include <map>
#include <string>
#include <time.h>
namespace ImGui {
    struct MarkdownImageData;
}

struct ImgItem {
  using Ptr = std::shared_ptr<ImgItem>;
  ~ImgItem();
  
  bool Open(FILE* fp);
  void touch() { time(&tsp); }
  unsigned int id = 0;
  int width = 0;
  int height = 0;

  time_t tsp;
};

class MdDoc {
    std::string content_;
    std::string dir_, name_;
    std::map<std::string, ImgItem::Ptr> img_map_;
    time_t last_tsp_ = 0;
public:
    bool opened_ = false;
    static void LoadFonts(const char* path, float fontSize);

    bool Open(const char* path);
    bool getImage(const std::string& path, ImGui::MarkdownImageData& data);
    int compactImg(time_t tsp);
    void Draw();
    void Close();
    ~MdDoc() {
        Close();
    }
};

std::string parentDir(std::string file);
FILE* FileOpen(const char* src, const char* mod, const char* dir = nullptr);
int ReadFileContent(FILE* fp, std::string& data);