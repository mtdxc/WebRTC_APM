audio_sources = [
  'remix_resample.cc',
  'utility/audio_frame_operations.cc',
]

libaudio = static_library('audio',
    audio_sources,
    dependencies: common_deps,
    include_directories: webrtc_inc,
    c_args: common_cflags,
    cpp_args: common_cxxflags
)

audio_dep = declare_dependency(
  link_with: [libaudio],
)
