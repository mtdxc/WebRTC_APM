#include "rnnoise.h"
#include "RnNoiseProcessor.h"
#include "api/audio/audio_frame.h"
#include "audio/utility/audio_frame_operations.h"
#include "rtc_base/logging.h"

void RemixAndResample(const int16_t* src_data,
                      size_t samples_per_channel,
                      size_t num_channels,
                      int sample_rate_hz,
                      webrtc::PushResampler<int16_t>* resampler,
                      webrtc::AudioFrame* dst_frame) {
  const int16_t* audio_ptr = src_data;
  size_t audio_ptr_num_channels = num_channels;
  int16_t downmixed_audio[webrtc::AudioFrame::kMaxDataSizeSamples];
  // Downmix before resampling.
  if (num_channels > dst_frame->num_channels_) {
    RTC_DCHECK(num_channels == 2 || num_channels == 4)
        << "num_channels: " << num_channels;
    RTC_DCHECK(dst_frame->num_channels_ == 1 || dst_frame->num_channels_ == 2)
        << "dst_frame->num_channels_: " << dst_frame->num_channels_;

    webrtc::AudioFrameOperations::DownmixChannels(
        src_data, num_channels, samples_per_channel, dst_frame->num_channels_,
        downmixed_audio);
    audio_ptr = downmixed_audio;
    audio_ptr_num_channels = dst_frame->num_channels_;
  }

  if (resampler->InitializeIfNeeded(sample_rate_hz, dst_frame->sample_rate_hz_,
                                    audio_ptr_num_channels) == -1) {
    RTC_LOG(LS_ERROR) << "InitializeIfNeeded failed: sample_rate_hz = "
                << sample_rate_hz << ", dst_frame->sample_rate_hz_ = "
                << dst_frame->sample_rate_hz_
                << ", audio_ptr_num_channels = " << audio_ptr_num_channels;
  }

  // TODO(yujo): for muted input frames, don't resample. Either 1) allow
  // resampler to return output length without doing the resample, so we know
  // how much to zero here; or 2) make resampler accept a hint that the input is
  // zeroed.
  const size_t src_length = samples_per_channel * audio_ptr_num_channels;
  int out_length =
      resampler->Resample(audio_ptr, src_length, dst_frame->mutable_data(),
                          webrtc::AudioFrame::kMaxDataSizeSamples);
  if (out_length == -1) {
    RTC_LOG(LS_ERROR) << "Resample failed: audio_ptr = " << audio_ptr
                << ", src_length = " << src_length
                << ", dst_frame->mutable_data() = "
                << dst_frame->mutable_data();
  }
  dst_frame->samples_per_channel_ = out_length / audio_ptr_num_channels;

  // Upmix after resampling.
  if (num_channels == 1 && dst_frame->num_channels_ == 2) {
    // The audio in dst_frame really is mono at this point; MonoToStereo will
    // set this back to stereo.
    dst_frame->num_channels_ = 1;
    webrtc::AudioFrameOperations::UpmixChannels(2, dst_frame);
  }
}

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
    dst->timestamp_ = frame->timestamp_;
    dst->elapsed_time_ms_ = frame->elapsed_time_ms_;
    dst->ntp_time_ms_ = frame->ntp_time_ms_;
    dst->packet_infos_ = frame->packet_infos_;
    RemixAndResample(frame->data(), frame->samples_per_channel(), frame->num_channels(), frame->sample_rate_hz(), &resample_, dst);
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
