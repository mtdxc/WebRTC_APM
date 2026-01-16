#include "rnnoise.h"
#include "RnNoiseProcessor.h"
#include "api/audio/audio_frame.h"
#include "audio/remix_resample.h"
#include "audio/utility/audio_frame_operations.h"
#include "rtc_base/logging.h"

RnNoiseProcessor::RnNoiseProcessor(const char* path) {
  loadModel(path);
}

bool RnNoiseProcessor::loadModel(const char* path) {
  cur_pcs_ = nullptr;
  if (path && path[0]) {
    model_ = rnnoise_model_from_filename(path);
    if (model_) {
      model_path_ = path;
    }
    else {
      auto p = strrchr(path, '/');
      if (!p) p = strrchr(path, '\\');
      if (p && pcs_map_.count(p+1)) {
        cur_pcs_ = pcs_map_[p+1];
        cur_pcs_->SetSink(sink_);
        model_path_ = p + 1;
        RTC_LOG_F(LS_INFO) << "use process " << model_path_;
        if (state_) {
          rnnoise_destroy(state_);
          state_ = nullptr;
        }
        if (model_) {
          rnnoise_model_free(model_);
          model_ = nullptr;
        }
        return true;
      }
    }
    RTC_LOG(LS_INFO) << "rnnoise_model_from_filename " << path << " return " << model_;
  } else {
    model_path_.clear();
  }
  std::unique_lock<decltype(lock_)> l(lock_);
  auto old = state_;
  state_ = rnnoise_create(model_);
  RTC_LOG_F(LS_INFO) << state_;
  if (old) {
    rnnoise_destroy(old);
  }
  return state_ != nullptr;
}

RnNoiseProcessor::~RnNoiseProcessor() {
  RTC_LOG_F(LS_INFO) << state_;
  std::unique_lock<decltype(lock_)> l(lock_);
  if (state_) {
    rnnoise_destroy(state_);
  }
  if (model_) {
    rnnoise_model_free(model_);
    model_ = nullptr;
  }
  cur_pcs_ = nullptr;
  auto map = pcs_map_;
  pcs_map_.clear();
  for (auto it : map) {
    delete it.second;
  }
}

void RnNoiseProcessor::addProcess(const char* name, webrtc::AudioFrameProcessor* pcs) {
  std::unique_lock<decltype(lock_)> l(lock_);
  RTC_LOG_F(LS_INFO) << name << pcs;
  pcs_map_[name] = pcs;
}

bool RnNoiseProcessor::set_scale(float v) {
  if (v < 0 || v > 10) {
    RTC_LOG_F(LS_WARNING) << v << " out of range";
    return false;
  }
  std::unique_lock<decltype(lock_)> l(lock_);
  RTC_LOG_F(LS_INFO) << scale_ << "->" << v;
  scale_ = v;
  return true;
}

#define FRAME_SIZE 480
void RnNoiseProcessor::Process(std::unique_ptr<webrtc::AudioFrame> frame) {
  if (!sink_) return;
  if (frame->muted()) {
    sink_(std::move(frame));
    return ;
  }
  if (scale_ != 1.0f) {
    webrtc::AudioFrameOperations::ScaleWithSat(scale_, frame.get());
  }
  if (cur_pcs_) {
    cur_pcs_->Process(std::move(frame));
    return;
  }
  if (!state_) {
    sink_(std::move(frame));
    return;
  }
  int16_t* pcm = frame->mutable_data();
  if (frame->samples_per_channel() % FRAME_SIZE) {
    auto dst = new webrtc::AudioFrame();
    dst->num_channels_ = frame->num_channels_;
    dst->sample_rate_hz_ = 48000;
    webrtc::voe::RemixAndResample(*frame, &resample_, dst);
    pcm = dst->mutable_data();
    frame.reset(dst);
  }
  assert(!(frame->samples_per_channel() % FRAME_SIZE));

  std::unique_lock<decltype(lock_)> l(lock_);
  float in[FRAME_SIZE]; float* out = in;
  for (size_t i = 0; i < frame->num_channels(); i++)
  {
    int16_t* p = pcm + i * FRAME_SIZE;
    for (int j = 0; j<FRAME_SIZE; j++) {
      in[j] = p[j];
    }
    rnnoise_process_frame(state_, out, in);
    for (int j = 0; j<FRAME_SIZE; j++) {
      p[j] = out[j];
    }
  }
  sink_(std::move(frame));
}

void RnNoiseProcessor::SetSink(OnAudioFrameCallback sink_callback) {
  sink_ = sink_callback;
  if (cur_pcs_) {
    cur_pcs_->SetSink(sink_);
  }
}
