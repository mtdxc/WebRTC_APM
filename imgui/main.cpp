// Dear ImGui: standalone example application for SDL3 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "IconsFontAwesome6.h"
#include "modules/audio_processing/RnNoiseProcessor.h"
#include <fstream>
#ifdef _WIN32
#include <io.h>
#pragma comment(lib, "opengl32.lib")
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

//#include "audio_processing.h"
#include <mutex>
#include <memory>
#include <iostream>
#include <string>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif
std::string genFileName(const char* prefix, const char* ext = "txt") {
    time_t timeValue = 0;
    time(&timeValue);
    struct tm* p = localtime(&timeValue);
    char fname[128] = { 0 };
    snprintf(fname, sizeof(fname), "%s-%d-%d_%d-%d-%d.%s", prefix,
        p->tm_mon + 1,
        p->tm_mday,
        p->tm_hour,
        p->tm_min,
        p->tm_sec,
        ext);
    return fname;
}

class SDLSurfaceTexture {
private:
    GLuint textureID;
    int width, height;

public:
    SDLSurfaceTexture() : textureID(0), width(0), height(0) {
        // 创建纹理
        glGenTextures(1, &textureID);
    }

    ~SDLSurfaceTexture() {
        if (textureID) {
            glDeleteTextures(1, &textureID);
        }
    }
    void clear() {
        static SDL_Surface* sur = nullptr;
        if (!sur) {
            sur = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGB24);
            memset(sur->pixels, 0, 3);
        }
        LoadFromSurface(sur);
        //SDL_DestroySurface(sur);
    }
    bool LoadFromSurface(SDL_Surface* surface) {
        if (!surface) return false;

        // 获取表面信息
        width = surface->w;
        height = surface->h;
        GLenum texture_format = GL_RGBA;
        // 确定纹理格式
        switch (surface->format) {
        case SDL_PIXELFORMAT_RGBA32:
            texture_format = GL_RGBA;
            break;
        //case SDL_PIXELFORMAT_ARGB32:
        //case SDL_PIXELFORMAT_ABGR32:
        case SDL_PIXELFORMAT_BGRA32:
            texture_format = GL_BGRA;
            break;
        case SDL_PIXELFORMAT_RGB24:
            texture_format = GL_RGB;
            break;
        case SDL_PIXELFORMAT_BGR24:
            texture_format = GL_BGR;
            break;
        default:
        {
            // 不支持其他格式，需要转换
            SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            if (!converted) return false;

            bool result = LoadFromSurface(converted);
            SDL_DestroySurface(converted);
            return result;
        }
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        // 设置纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // 上传纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            texture_format, GL_UNSIGNED_BYTE, surface->pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    void Draw(float size_x = -1, float size_y = -1) {
        if (!textureID) return;

        if (size_x < 0) size_x = (float)width;
        if (size_y < 0) size_y = (float)height;
        ImGui::Image((ImTextureID)textureID, ImVec2(size_x, size_y));
    }

    GLuint GetTextureID() const { return textureID; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
};

struct CameraClose {
    void operator()(SDL_Camera* t) const {
        SDL_CloseCamera(t);
    }
};

struct AudioStreamClose {
    void operator()(SDL_AudioStream* t) const {
        SDL_DestroyAudioStream(t);
    }
};

template <class T>
class PcmBuffer {
    std::mutex lock_;
    T* buff_;
    int read_pos_ = 0;
    int write_pos_ = 0;
    int max_size_ = 8192;
    static const int ELEM_SIZE = sizeof(T);
public:
    PcmBuffer(int max_size) : max_size_(max_size) {
        buff_ = new T[max_size];
    }
    ~PcmBuffer() {
        delete[] buff_;
    }
    T* data() const {
        return buff_ + read_pos_;
    }
    int size() const {
        return write_pos_ - read_pos_;
    }
    void clear() {
        write_pos_ = read_pos_ = 0;
    }
    int Read(T* buff, int size) {
        std::unique_lock<decltype(lock_)> l(lock_);
        int n = write_pos_ - read_pos_;
        if (size > n) {
            memcpy(buff, buff_ + read_pos_, n * ELEM_SIZE);
            memset(buff + n, 0, ELEM_SIZE * (size - n));
            read_pos_ = write_pos_ = 0;
            return n;
        }
        else {
            memcpy(buff, buff_ + read_pos_, size * ELEM_SIZE);
            read_pos_ += size;
            return size;
        }
    }
    int Write(const float* buff, int size) {
        std::unique_lock<decltype(lock_)> l(lock_);
        if (max_size_ - write_pos_ < size) {
            if (read_pos_) {
                write_pos_ -= read_pos_;
                memmove(buff_, buff_ + read_pos_, write_pos_ * ELEM_SIZE);
                read_pos_ = 0;
            }
            if (max_size_ - write_pos_ < size) {
                // return -1;
                int pos = write_pos_ - size;
                memmove(buff_, buff_ + size, pos * ELEM_SIZE);
                memcpy(buff_ + pos, buff, size * ELEM_SIZE);
                return size;
            }
        }
        memcpy(&buff_[write_pos_], buff, size * ELEM_SIZE);
        write_pos_ += size;
        return size;
    }
};

webrtc::AudioProcessing::Config config_;
void loadApConfig(webrtc::AudioProcessing* apm) {
    if (apm) {
        config_ = apm->GetConfig();
    }
}

void ShowApConfig(webrtc::AudioProcessing* apm) {
    ImGui::Begin("audio process", nullptr);
    if (auto* pipeline = &config_.pipeline) {
        ImGui::LabelText("##pipeine", "pipeine");
        ImGui::Indent();
        //static const char* strDownmixMethod[] = { "AverageChannels", "UseFirstChannel" };
        //ImGui::Combo("capture_downmix_method", (int*)&pipeline->capture_downmix_method, strDownmixMethod, 2);
        ImGui::SetNextItemWidth(80); ImGui::InputInt("maximum_internal_processing_rate", &pipeline->maximum_internal_processing_rate, 0, 0);
        ImGui::Checkbox("multi_channel_render", &pipeline->multi_channel_render);
        ImGui::Checkbox("multi_channel_capture", &pipeline->multi_channel_capture);
        ImGui::Unindent();
    }

    auto pre = &config_.pre_amplifier;
    ImGui::Checkbox("PreAmplifier", &pre->enabled);
    if (pre && pre->enabled) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(80);
        ImGui::InputFloat("fixed_gain_factor", &pre->fixed_gain_factor);
        ImGui::Unindent();
    }
    /*
    auto cla = &config_.capture_level_adjustment;
    ImGui::Checkbox("capture_level_adjustment", &cla->enabled);
    if (cla && cla->enabled) {
        ImGui::Indent();
        ImGui::PushItemWidth(80);
        ImGui::InputFloat("pre_gain_factor", &cla->pre_gain_factor);
        ImGui::InputFloat("post_gain_factor", &cla->post_gain_factor);
        ImGui::Checkbox("analog_mic_gain_emulation", &cla->analog_mic_gain_emulation.enabled);
        if (cla->analog_mic_gain_emulation.enabled) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("initial_level", &cla->analog_mic_gain_emulation.initial_level, 0, 0);
            ImGui::Unindent();
        }
        ImGui::PopItemWidth();
        ImGui::Unindent();
    }
    */
    auto hpf = &config_.high_pass_filter;
    ImGui::Checkbox("high pass filter", &hpf->enabled);
    if (hpf && hpf->enabled) {
        ImGui::Indent();
        ImGui::Checkbox("apply in full band", &hpf->apply_in_full_band);
        ImGui::Unindent();
    }

    auto ec = &config_.echo_canceller;
    ImGui::Checkbox("echo_canceller", &ec->enabled);
    if (ec && ec->enabled) {
        ImGui::Indent();
        ImGui::Checkbox("aecm", &ec->mobile_mode);
        ImGui::Checkbox("export_linear_aec_output", &ec->export_linear_aec_output);
        ImGui::Checkbox("enforce_high_pass_filtering", &ec->enforce_high_pass_filtering);
        ImGui::Unindent();
    }

    auto ns = &config_.noise_suppression;
    ImGui::Checkbox("noise_suppression", &ns->enabled);
    if (ns && ns->enabled) {
        ImGui::Indent();
        static const char* level[] = {"Low", "Moderate", "High", "VeryHigh"};
        ImGui::Combo("nslevel", (int*) & ns->level, level, 4);
        ImGui::Checkbox("analyze_linear_aec_output_when_available", &ns->analyze_linear_aec_output_when_available);
        ImGui::Checkbox("howling_suppression", &ns->howling_suppression);
        ImGui::Unindent();
    }

    ImGui::Checkbox("transient_suppression", &config_.transient_suppression.enabled);
    ImGui::Checkbox("vad_enable", &config_.voice_detection.enabled);

    
    auto gc1 = &config_.gain_controller1;
    ImGui::Checkbox("gain_controller1", &gc1->enabled);
    if (gc1 && gc1->enabled) {
        ImGui::Indent();
        static const char* strMode[] = { "AdaptiveAnalog", "AdaptiveDigital", "FixedDigital" };
        ImGui::Combo("gc1mode", (int*)&gc1->mode, strMode, 3);
        ImGui::PushItemWidth(60);
        ImGui::InputInt("target_level_dbfs", &gc1->target_level_dbfs, 0, 0);
        ImGui::InputInt("compression_gain_db", &gc1->compression_gain_db, 0, 0);
        ImGui::PopItemWidth();
        ImGui::Checkbox("enable limiter", &gc1->enable_limiter);

        auto gagc = &gc1->analog_gain_controller;
        ImGui::Checkbox("analog_gain_controller", &gagc->enabled);
        if (gagc && gagc->enabled) {
            ImGui::Indent();
            ImGui::Checkbox("digital_adaptive", &gagc->enable_digital_adaptive);
            ImGui::Checkbox("enable_agc2_level_estimator", &gagc->enable_agc2_level_estimator);
            ImGui::PushItemWidth(60);
            ImGui::InputInt("startup_min_volume", &gagc->startup_min_volume, 0, 0);
            ImGui::InputInt("clipped_level_min", &gagc->clipped_level_min, 0, 0);
            /*
            ImGui::InputInt("clipped_level_step", &gagc->clipped_level_step, 0, 0);
            ImGui::InputInt("clipped_wait_frames", &gagc->clipped_wait_frames, 0, 0);
            ImGui::InputFloat("clipped_ratio_threshold", &gagc->clipped_ratio_threshold);
            ImGui::PopItemWidth();

            auto gapcp = &gagc->clipping_predictor;
            ImGui::Checkbox("clipping_predictor", &gapcp->enabled);
            if (gapcp && gapcp->enabled) {
                ImGui::Indent();
                static const char* cpMode[] = {"ClippingEventPrediction", "AdaptiveStepClippingPeakPrediction", "FixedStepClippingPeakPrediction"};
                ImGui::Combo("cpmode", (int*)&gapcp->mode, cpMode, 3);
                ImGui::PushItemWidth(60);
                ImGui::InputInt("window_length", &gapcp->window_length, 0, 0);
                ImGui::InputInt("reference_window_length", &gapcp->reference_window_length, 0, 0);
                ImGui::InputInt("reference_window_delay", &gapcp->reference_window_delay, 0, 0);
                ImGui::InputFloat("clipping_threshold", &gapcp->clipping_threshold);
                ImGui::InputFloat("crest_factor_margin", &gapcp->crest_factor_margin);
                ImGui::Checkbox("use_predicted_step", &gapcp->use_predicted_step);
                ImGui::PopItemWidth();
                ImGui::Unindent();
            }*/
            ImGui::Unindent();
        }
        ImGui::Unindent();
    }

    auto gc2 = &config_.gain_controller2;
    ImGui::Checkbox("gain_controller2", &gc2->enabled);
    if (gc2 && gc2->enabled) {
        ImGui::Indent();
        //ImGui::Checkbox("input_volume_controller", &gc2->input_volume_controller.enabled);
        auto gc2ad = &gc2->adaptive_digital;
        ImGui::Checkbox("adaptive_digital", &gc2ad->enabled);
        if (gc2ad && gc2ad->enabled) {
            ImGui::Indent();
            ImGui::PushItemWidth(80);
            ImGui::Checkbox("use_saturation_protector", &gc2ad->use_saturation_protector);
            ImGui::InputFloat("vad_probability_attack", &gc2ad->vad_probability_attack);
            ImGui::InputInt("level_estimator_adjacent_speech_frames_threshold", &gc2ad->level_estimator_adjacent_speech_frames_threshold);
            ImGui::InputFloat("extra_saturation_margin_db", &gc2ad->extra_saturation_margin_db);
            ImGui::InputFloat("initial_saturation_margin_db", &gc2ad->initial_saturation_margin_db);
            ImGui::InputInt("gain_applier_adjacent_speech_frames_threshold", &gc2ad->gain_applier_adjacent_speech_frames_threshold);
            ImGui::InputFloat("max_gain_change_db_per_second", &gc2ad->max_gain_change_db_per_second);
            ImGui::InputFloat("max_output_noise_level_dbfs", &gc2ad->max_output_noise_level_dbfs);
            ImGui::PopItemWidth();
            ImGui::Unindent();
        }
        ImGui::SetNextItemWidth(80);
        ImGui::InputFloat("fixed digital gain db", &gc2->fixed_digital.gain_db);
        ImGui::Unindent();
    }
    ImGui::Checkbox("residual_echo_detector", &config_.residual_echo_detector.enabled);
    ImGui::Checkbox("level_estimation", &config_.level_estimation.enabled);
    if (ImGui::Button("Apply")) {
      if (apm) {
          std::cout << config_.ToString();
          apm->ApplyConfig(config_); 
          loadApConfig(apm);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
      loadApConfig(apm);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      std::string fname = genFileName("config");
      std::cout << "ap config will be save in " << fname;
      std::ofstream fs(fname);
      fs << config_.ToString();
    }
    ImGui::End();
}


#if 0
void ShowRnNoiseCfg() {
  auto pcs = callback_->getRnPcs();
  if (!pcs)
    return;
  ImGui::Begin("rnnoise");
  static int item_current = 0;
  if (auto path = pcs->modelPath()) {
    for (int i = 0; i < (int)rn_models_.size(); ++i) {
      if (strstr(path, rn_models_[i].c_str()) != nullptr) {
        item_current = i;
        break;
  }
    }
  }
  if (ImGui::BeginCombo("##Combo", rn_models_[item_current].c_str())) {
    for (int i = 0; i < (int)rn_models_.size(); ++i) {
      bool is_selected = i == item_current;
      if (ImGui::Selectable(rn_models_[i].c_str(), is_selected)) {
        item_current = i;
        if (i)
          pcs->loadModel((rn_dir_ + rn_models_[i]).c_str());
        else
          pcs->loadModel("");
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (pcs->ok()) {
    ImGui::SameLine();
    ImGui::Text("loaded");
  }
  ImGui::End();
}

void loadRnModel() {
  rn_models_.clear();
  char fontPath[256];
  rn_dir_ = AppPath(fontPath, "rnmodel/");
  rtc::Filesystem::MakeFolder(rn_dir_);
  rn_models_.push_back("disable");
  rtc::DirectoryIterator di;
  if (di.Iterate(rn_dir_)) {
    while (di.Next()) {
      if (di.IsDirectory())
        continue;
      else if (di.Name().length() > 4 &&
               di.Name().substr(di.Name().length() - 4) == ".bin") {
        rn_models_.push_back(di.Name());
      }
    }
  }
}

void DrawOptionalBool(const char* label, absl::optional<bool>& value)
{
  int tvalue = value.has_value() + value.value_or(0);
  static const char* tBoolMap[] = {"unset", "false", "true"};
  ImGui::SetNextItemWidth(80);
  if (ImGui::Combo(label, &tvalue, tBoolMap, 3)) {
    if(tvalue==0){
      value.reset();
    }
    else {
      value = tvalue - 1;
    }
  }
}

void showAudioOption() {
  ImGui::Begin("audioOption");
  //DrawOptionalBool("disable_buildin_process", option_.disable_buildin_process);
  DrawOptionalBool("echo_cancellation", option_.echo_cancellation);
#if defined(WEBRTC_IOS)
  // Forces software echo cancellation on iOS. This is a temporary workaround
  // (until Apple fixes the bug) for a device with non-functioning AEC. May
  // improve performance on that particular device, but will cause
  // unpredictable behavior in all other cases. See
  // http://bugs.webrtc.org/8682.
  DrawOptionalBool("ios_force_software_aec_HACK", &option_.ios_force_software_aec_HACK);
#endif
  DrawOptionalBool("auto_gain_control", option_.auto_gain_control);
  DrawOptionalBool("noise_suppression", option_.noise_suppression);
  DrawOptionalBool("highpass_filter", option_.highpass_filter);
  DrawOptionalBool("stereo_swapping", option_.stereo_swapping);
  DrawOptionalBool("ajb_fast_accelerate", option_.audio_jitter_buffer_fast_accelerate);
#if 0
  ImGui::PushItemWidth(60);
  ImGui::InputInt("ajb_max_packets", option_.audio_jitter_buffer_max_packets, 0, 0);
  ImGui::InputInt("ajb_min_delay_ms", option_.audio_jitter_buffer_min_delay_ms, 0, 0);
  ImGui::PopItemWidth();
  DrawOptionalBool("audio_network_adaptor", option_.audio_network_adaptor);
  ImGui::Text("%s", option_.audio_network_adaptor_config.c_str());
#endif
  DrawOptionalBool("init_recording_on_send", option_.init_recording_on_send);
  if (ImGui::Button("reset")) {
    option_ = {};
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    std::string fname = genFileName("option");
    RTC_LOG(LS_INFO) << "audio option will be save in " << fname;
    std::ofstream fs(fname);
    fs << option_.ToString();
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply")) {
    callback_->SetAudioOption(option_);
  }
  ImGui::End();
}
#endif

class SDLDevice {
    std::unique_ptr<SDL_Camera, CameraClose> camera_;
    std::unique_ptr<SDL_AudioStream, AudioStreamClose> mic_stream_, spk_stream_;
    webrtc::StreamConfig input_config_, output_config_;
    rtc::scoped_refptr<webrtc::AudioProcessing> apm_;
    PcmBuffer<float> pcm;
    std::vector<char> play_buff_, rec_buff_;
public:
    char* getBuffer(bool play, int size) {
        auto& buf = play ? play_buff_ : rec_buff_;
        if (buf.size() < size)
            buf.resize(size);
        return buf.data();
    }
    bool initApm(int sample_rate, int channels) {
        if (!apm_) {
            input_config_ = output_config_ = webrtc::StreamConfig(sample_rate, channels);
            apm_ = webrtc::AudioProcessingBuilder().Create();
            loadApConfig(apm_);
        }
        return apm_ != nullptr;
    }
    SDLDevice() : pcm(8192) {
        SDL_Init(SDL_INIT_CAMERA | SDL_INIT_AUDIO);
    }
    virtual ~SDLDevice() {
        StopAll();
    }
    void StopAll() {
        StopPlayout();
        StopRecord();
        StopPreview();
        apm_ = nullptr;
    }

    float* pcm_data() const {
        return pcm.data();
    }
    int pcm_size() const {
        return pcm.size();
    }
    static void printSpec(const SDL_AudioSpec& spec, const char* msg) {
        printf("%s %s %dx%d\n", msg, SDL_GetAudioFormatName(spec.format), spec.freq, spec.channels);
    }

    bool StartRecord(SDL_AudioDeviceID id, const SDL_AudioSpec& aspec) {
        mic_stream_.reset(SDL_OpenAudioDeviceStream(id, &aspec, [](void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
            // printf("Microphone stream callback: %d, %d\n", additional_amount, total_amount);
            auto self = (SDLDevice*)userdata;
            if (additional_amount > 0) {
                char* buffer = self->getBuffer(false, additional_amount);
                int got = SDL_GetAudioStreamData(stream, buffer, additional_amount);
                if (got > 0) {
                    float* f = (float*)buffer;
                    got /= sizeof(*f);
                    if (self->apm_) {
                        int step = self->input_config_.num_samples();
                        int n = got / step;
                        for (int i = 0; i < n; i++) {
                            self->apm_->ProcessStream(&f, self->input_config_, self->output_config_, &f);
                            f += step;
                        }
                    }
                    self->pcm.Write((float*)buffer, got);
                }
            }
         }, this));
        if (!mic_stream_) return false;
        SDL_AudioSpec ispec, ospec;
        SDL_GetAudioStreamFormat(mic_stream_.get(), &ispec, &ospec);
        printSpec(ispec, "record in  spec");
        printSpec(ospec, "record out spec");
        initApm(ospec.freq, ospec.channels);
        SDL_ResumeAudioStreamDevice(mic_stream_.get());
        return true;
    };
    bool isRecord() const {
        return mic_stream_ != nullptr;
    }
    void StopRecord() {
        mic_stream_ = nullptr;
        pcm.clear();
    }
    bool StartPlayout(SDL_AudioDeviceID id, const SDL_AudioSpec& aspec) {
        printSpec(aspec, "playout spec");
        spk_stream_.reset(SDL_OpenAudioDeviceStream(id, &aspec, [](void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
            auto self = (SDLDevice*)userdata;
            if (additional_amount > 0) {
                // printf("Speaker stream callback: %d, %d\n", additional_amount, total_amount);
                /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
                char* buffer = self->getBuffer(true, additional_amount);
                float* f = (float*)buffer;
                int samples = additional_amount / sizeof(*f);
                int n = self->pcm.Read(f, samples);
                if (n && self->apm_) {
                    int step = self->input_config_.num_samples();
                    n /= step;
                    for (int i = 0; i < n; i++) {
                        self->apm_->ProcessReverseStream(&f, self->input_config_, self->output_config_, &f);
                        f += step;
                    }
                }
                SDL_PutAudioStreamData(stream, buffer, additional_amount);
            }
        }, this));
        if (!spk_stream_) return false;
        SDL_AudioSpec ispec, ospec;
        SDL_GetAudioStreamFormat(spk_stream_.get(), &ispec, &ospec);
        printSpec(ispec, "playout in  spec");
        printSpec(ospec, "playout out spec");
        initApm(ispec.freq, ispec.channels);
        int frameSize = 1024;
        SDL_GetAudioDeviceFormat(id, &ispec, &frameSize);
        if (frameSize > 4) {
            printf("frameSize=%d\n", frameSize);
        }
        SDL_ResumeAudioStreamDevice(spk_stream_.get());
        return true;
    };
    bool isPlayout() const {
        return spk_stream_ != nullptr;
    }
    void StopPlayout() {
        spk_stream_ = nullptr;
    }
    bool StartPreview(SDL_CameraID id, const SDL_CameraSpec& spec) {
        printf("open camera %d with fmt %s %dx%d@%d\n", id, SDL_GetPixelFormatName(spec.format), spec.width, spec.height, spec.framerate_numerator / spec.framerate_denominator);
        camera_.reset(SDL_OpenCamera(id, &spec));
        return camera_ != nullptr;
    };
    bool isPreview() const {
        return camera_ != nullptr;
    }
    void StopPreview() {
        camera_ = nullptr;
    }
    webrtc::AudioProcessing* apm() {
        return apm_.get();
    }
    // SDL_Camera* camera() { return camera_.get(); }
    std::shared_ptr<SDL_Surface> captureFrame(uint64_t* tsp) {
        std::shared_ptr<SDL_Surface> ret;
        if (auto camera = camera_.get()) {
            ret.reset(SDL_AcquireCameraFrame(camera, tsp),
                [camera](SDL_Surface* frame) {SDL_ReleaseCameraFrame(camera, frame); });
        }
        return std::move(ret);
    }
};

// Main code
int main(int, char**)
{
    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_CAMERA | SDL_INIT_AUDIO))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("WebRTC APM example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    //style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
#ifdef _WIN32
    #define DEFALUT_FONT_PATH "c:\\Windows\\Fonts\\msyh.ttc"
#elif __APPLE__
    #define DEFALUT_FONT_PATH "/System/Library/Fonts/STHeiti Light.ttc"
#else
    #define DEFALUT_FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#endif
    if (0 == access(DEFALUT_FONT_PATH, 0)) {
        io.Fonts->AddFontFromFileTTF(DEFALUT_FONT_PATH, 20.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    }
    if (0 == access(FONT_ICON_FILE_NAME_FAS, 0)) {
    float baseFontSize = 13.0f; // 13.0f is the size of the default font. Change to the font size you use.
        float iconFontSize = baseFontSize * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
        // merge in icons from Font Awesome
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
        // use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid
    }

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    SDLSurfaceTexture preview_texture;

    int fmt_idx = 0, camera_fmt_count = 0;
    SDL_CameraSpec** camera_fmts = nullptr;
    int camera_idx = 0, camera_count = 0;
    SDL_CameraID* camera_ids = SDL_GetCameras(&camera_count);
    if (camera_count) {
        camera_fmts = SDL_GetCameraSupportedFormats(camera_ids[camera_idx], &camera_fmt_count);
    }
    // SDL_free(camera_ids);

    int mic_idx = 0, mic_count = 0;
    SDL_AudioDeviceID* mic_ids = SDL_GetAudioRecordingDevices(&mic_count);

    int spk_idx = 0, spk_count = 0;
    SDL_AudioDeviceID* spk_ids = SDL_GetAudioPlaybackDevices(&spk_count);

    SDLDevice device;
    SDL_AudioSpec aspec;
    aspec.format = SDL_AUDIO_F32;
    aspec.channels = 1;
    aspec.freq = 44100;

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

            if (mic_ids) {
                bool val = device.isRecord();
                if(ImGui::Combo("##Microphone", &mic_idx, [](void* data, int idx) {
                    auto ids = (SDL_AudioDeviceID*)data;
                    return SDL_GetAudioDeviceName(ids[idx]);
                }, mic_ids, mic_count) && val) {
                    device.StartRecord(mic_ids[mic_idx], aspec);
                }
                ImGui::SameLine();
                if (ImGui::Button(val ? "Stop Record" : "Start Record")) {
                    if (val) {
                        device.StopRecord();
                    }
                    else{
                        device.StartRecord(mic_ids[mic_idx], aspec);
                    }
                }
            }
            if (spk_ids) {
                bool val = device.isPlayout();
                if(ImGui::Combo("##Speaker", &spk_idx, [](void* data, int idx) {
                    auto ids = (SDL_AudioDeviceID*)data;
                    return SDL_GetAudioDeviceName(ids[idx]);
                }, spk_ids, spk_count) && val) {
                    device.StartPlayout(spk_ids[spk_idx], aspec);
                }
                ImGui::SameLine();
                if (ImGui::Button(val?"Stop Play":"Start Play")) {
                    if (val) {
                        device.StopPlayout();
                    }
                    else {
                        device.StartPlayout(spk_ids[spk_idx], aspec);
                    }
                }
            }
            if (spk_ids || mic_ids) {
                ImGui::PlotLines("##wav", device.pcm_data(), device.pcm_size(), 0, nullptr, -1.0f, 1.0f, ImVec2(0, 100));
                ImGui::SameLine();
                bool loop = device.isPlayout() && device.isRecord();
                if (ImGui::Button(loop ? "Stop Loopback":"Start Lookback")) {
                    if (loop) {
                        device.StopPlayout();
                        device.StopRecord();
                    }
                    else{
                        device.StartPlayout(spk_ids[spk_idx], aspec);
                        device.StartRecord(mic_ids[mic_idx], aspec);
                    }
                }
                if (auto fs = device.apm()) {
                    ShowApConfig(fs);
                }
            }

            if (camera_ids) {
                if (ImGui::Combo("Camera", &camera_idx, [](void* data, int idx) {
                    auto ids = (SDL_CameraID*)data;
                    return SDL_GetCameraName(ids[idx]);
                }, camera_ids, camera_count)) {
                    auto camera_id = camera_ids[camera_idx];
                    if (camera_fmts) { SDL_free(camera_fmts); }
                    camera_fmts = SDL_GetCameraSupportedFormats(camera_id, &camera_fmt_count);
                }

                if (camera_fmts) {
                    bool val = device.isPreview();
                    if(ImGui::Combo("##formats", &fmt_idx, [](void* data, int idx) {
                        auto ids = (SDL_CameraSpec**)data;
                        auto spec = ids[idx];
                        static char buff[64];
                        sprintf(buff, "%dx%d@%s", spec->width, spec->height, SDL_GetPixelFormatName(spec->format));
                        return (const char*)buff;
                    }, camera_fmts, camera_fmt_count) && val){
                        device.StopPreview();
                        preview_texture.clear();
                        device.StartPreview(camera_ids[camera_idx], *camera_fmts[fmt_idx]);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(val?"Stop Preview":"Start Preview")) {
                        if (val) {
                            device.StopPreview();
                            preview_texture.clear();
                        } else {
                            device.StartPreview(camera_ids[camera_idx], *camera_fmts[fmt_idx]);
                        }
                    }
                }

                Uint64 tsp;
                if (auto frame = device.captureFrame(&tsp)) {
                    preview_texture.LoadFromSurface(frame.get());
                }
                preview_texture.Draw(640, 480);
            }
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    device.StopAll();
    // Free camera_/audio device lists
    SDL_free(camera_fmts);
    SDL_free(camera_ids);
    SDL_free(mic_ids);
    SDL_free(spk_ids);

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
