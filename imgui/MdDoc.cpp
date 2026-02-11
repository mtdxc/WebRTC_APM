#include "MdDoc.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include "imgui_markdown.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <codecvt>
#include <SDL3/SDL_opengl.h>

static ImFont* H1 = NULL;
static ImFont* H2 = NULL;
static ImFont* H3 = NULL;

static ImGui::MarkdownConfig mdConfig;
void MdDoc::LoadFonts(const char* path, float fontSize) {
    ImGuiIO& io = ImGui::GetIO();
    auto gr = io.Fonts->GetGlyphRangesChineseFull();
    io.Fonts->Clear();
    // Base font
    io.Fonts->AddFontFromFileTTF(path, fontSize, nullptr, gr);
    // Bold headings H2 and H3
    H2 = io.Fonts->AddFontFromFileTTF(path, fontSize, nullptr, gr);
    H3 = mdConfig.headingFormats[1].font;
    // bold heading H1
    float fontSizeH1 = fontSize * 1.1f;
    H1 = io.Fonts->AddFontFromFileTTF(path, fontSizeH1, nullptr, gr);
}

std::wstring ToUtf16(const std::string& src) {
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
    return converter.from_bytes(src);
}

std::string parentDir(std::string file) {
    bool got_name = false;
    int pos = file.length() - 1;
    while (pos >= 0) {
        char e = file[pos];
        if (e == '\\' || e == '/') {
            if (got_name) {
                return file.substr(0, pos + 1);
            }
        }
        else {
            got_name = true;
        }
        pos--;
    }
    return "";
}

FILE* FileOpen(const char* src, const char* mod, const char* dir) {
#ifdef _WIN32
    std::wstring wsrc = ToUtf16(src);
    std::wstring wmod = ToUtf16(mod);
    FILE* ret = _wfopen(wsrc.c_str(), wmod.c_str());
    if (!ret && dir) {
        wsrc = ToUtf16(dir) + wsrc;
        ret = _wfopen(wsrc.c_str(), wmod.c_str());
    }
#else
    FILE* ret = fopen(src, mod);
    if (!ret && dir) {
        std::string wsrc = dir;
        wsrc += src;
        printf("try open file: %s\n", wsrc.c_str());
        ret = fopen(wsrc.c_str(), mod);
    }
#endif // _WIN32
    return ret;
}

int ReadFileContent(FILE* fp, std::string& data) {
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    data.resize(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    return fread(&data[0], 1, data.size(), fp);
}

ImgItem::~ImgItem() {
    if (id) {
        glDeleteTextures(1, (GLuint*)&id);
    }
}

bool ImgItem::Open(FILE* fp) { 
    unsigned char* image_data = stbi_load_from_file(fp, &width, &height, NULL, 4);
    fclose(fp);
    if (image_data == NULL) {
        return false;
    }

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, image_data);

    stbi_image_free(image_data);
    touch();
    return true;
}


void LinkCallback(ImGui::MarkdownLinkCallbackData data)
{
    std::string url(data.link, data.linkLength);
    if (!data.isImage) {
        //SDL_OpenURL(url.c_str());
        ImGui::GetPlatformIO().Platform_OpenInShellFn(ImGui::GetCurrentContext(), url.c_str());
    }
}

ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data) {
    ImGui::MarkdownImageData imageData;
    std::string url(data.link, data.linkLength);
    MdDoc* doc = (MdDoc*)data.userData;
    if (!doc->getImage(url, imageData)) {
        // In your application you would load an image based on data input. Here we just use the imgui font texture.
        ImTextureID image = ImGui::GetIO().Fonts->TexID;
        // > C++14 can use ImGui::MarkdownImageData imageData{ true, false, image, ImVec2( 40.0f, 20.0f ) };
        ImGui::MarkdownImageData imageData;
        imageData.isValid = true;
        imageData.useLinkCallback = false;
        imageData.user_texture_id = image;
        imageData.size = ImVec2(40.0f, 20.0f);
    }

    // For image resize when available size.x > image width, add
    ImVec2 const contentSize = ImGui::GetContentRegionAvail();
    if (imageData.size.x > contentSize.x)
    {
        float const ratio = imageData.size.y / imageData.size.x;
        imageData.size.x = contentSize.x;
        imageData.size.y = contentSize.x * ratio;
    }

    return imageData;
}

void ExampleMarkdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start)
{
    // Call the default first so any settings can be overwritten by our implementation.
    // Alternatively could be called or not called in a switch statement on a case by case basis.
    // See defaultMarkdownFormatCallback definition for furhter examples of how to use it.
    ImGui::defaultMarkdownFormatCallback(markdownFormatInfo_, start);

    switch (markdownFormatInfo_.type) {
        // example: change the colour of heading level 2
    case ImGui::MarkdownFormatType::HEADING:
        if (markdownFormatInfo_.level == 2) {
            if (start) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            }
            else {
                ImGui::PopStyleColor();
            }
        }
        break;
    default:
        break;
    }
}

void MdDoc::Draw() {
    if (!opened_) {
        return;
    }
    ImGui::Begin(name_.c_str(), &opened_);
    // You can make your own Markdown function with your prefered string container and markdown config.
    // > C++14 can use ImGui::MarkdownConfig mdConfig{ LinkCallback, NULL, ImageCallback, ICON_FA_LINK, { { H1, true }, { H2, true }, { H3, false } }, NULL };
    mdConfig.linkCallback = LinkCallback;
    mdConfig.tooltipCallback = NULL;
    mdConfig.imageCallback = ImageCallback;
    mdConfig.linkIcon = ICON_FA_LINK;
    mdConfig.headingFormats[0] = { H1, true };
    mdConfig.headingFormats[1] = { H2, true };
    mdConfig.headingFormats[2] = { H3, false };
    mdConfig.userData = this;
    mdConfig.formatCallback = ExampleMarkdownFormatCallback;
    ImGui::Markdown(content_.c_str(), content_.length(), mdConfig);
    ImGui::End();
}

bool MdDoc::Open(const char* path) {
    Close();
    if (auto fp = FileOpen(path, "r")) {
        ReadFileContent(fp, content_);
        fclose(fp);
        name_ = path;
        auto pos = name_.find_last_of("/\\");
        if (pos != std::string::npos) {
            dir_ = name_.substr(0, pos + 1);
            name_ = name_.substr(pos + 1);
        }
        opened_ = true;
        return true;
    }
    return false;
}

void MdDoc::Close() {
    img_map_.clear();
    content_ = dir_ = name_ = "";
}

bool MdDoc::getImage(const std::string& path, ImGui::MarkdownImageData& data) {
    ImgItem::Ptr img = nullptr;
    auto it = img_map_.find(path);
    if (it == img_map_.end()) {
       FILE* fp = FileOpen(path.c_str(), "rb", dir_.c_str());
       if (!fp) {
            return false;
       }
       img = std::make_shared<ImgItem>();
       if (!img->Open(fp)) {
           return false;
       }
       if (img_map_.size() > 30) {
          compactImg(img->tsp - 30);
       }
       img_map_[path] = img;
    }
    else {
        img = it->second;
        img->touch();
    }
    if (img) {
        data.isValid = true;
        data.useLinkCallback = false;
        data.size.x = img->width;
        data.size.y = img->height;
        data.user_texture_id = img->id;
        return true;
    }
    return false;
}

int MdDoc::compactImg(time_t tsp) {
    if (last_tsp_ > tsp) {
        return 0;
    }
    last_tsp_ = 0;
    int count = 0;
    for (auto it = img_map_.begin(); it != img_map_.end();) {
        if (it->second->tsp < tsp) {
            it = img_map_.erase(it);
            count++;
        }
        else {
            if (last_tsp_ == 0 || it->second->tsp < last_tsp_) {
                last_tsp_ = it->second->tsp;
            }
            ++it;
        }
    }
    last_tsp_ = tsp;
    return count;
}