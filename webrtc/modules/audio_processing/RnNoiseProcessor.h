#ifndef API_RNNOISE_PROCESSOR_H_
#define API_RNNOISE_PROCESSOR_H_
#include <map>
#include <mutex>
#include "api/audio/audio_frame_processor.h"
#include "common_audio/resampler/include/push_resampler.h"
struct DenoiseState;
struct RNNModel;
class RnNoiseProcessor : public webrtc::AudioFrameProcessor {
  OnAudioFrameCallback sink_;
  DenoiseState* state_ = nullptr;
  std::string model_path_;
  RNNModel* model_ = nullptr;
  std::mutex lock_;
  webrtc::PushResampler<int16_t> resample_;
  float scale_ = 1.0f;
  // 外挂处理器
  webrtc::AudioFrameProcessor* cur_pcs_ = nullptr;
  std::map<std::string, webrtc::AudioFrameProcessor*> pcs_map_;
 public:
  void addProcess(const char* name, webrtc::AudioFrameProcessor* pcs);
  void forEachPcs(std::function<void(const std::string& name, webrtc::AudioFrameProcessor*)> cb) {
    for (auto it : pcs_map_) {
      cb(it.first, it.second);
    }
  }
  bool set_scale(float v);
  float scale() const {return scale_;}
  RnNoiseProcessor(const char* path);
  virtual ~RnNoiseProcessor();
  bool loadModel(const char* path);
  const char* modelPath() const {return model_path_.c_str();}
  bool ok() const { return state_ || cur_pcs_; }
  virtual void Process(std::unique_ptr<webrtc::AudioFrame> frame);

  // Atomically replaces the current sink with the new one. Before the
  // first call to this function, or if the provided `sink_callback` is nullptr,
  // processed frames are simply discarded.
  virtual void SetSink(OnAudioFrameCallback sink_callback);
};

#endif