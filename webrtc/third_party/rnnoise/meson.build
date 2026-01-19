rnnoise_sources = [
  'src/rnn_vad_weights.cc',
  'src/celt_lpc.c',
  'src/denoise.c',
  'src/kiss_fft.c',
  'src/pitch.c',
  'src/rnn.c',
  'src/nnet.c',
  'src/nnet_default.c',
  'src/parse_lpcnet_weights.c',
  'src/rnnoise_tables.c',
  'src/rnnoise_data.c',
]
rnnoise_inc = include_directories('src')
avx_flags = []
if have_x86
  avx_flags = ['-mavx2', '-mfma']
  rnnoise_sources += [
    'src/x86/x86cpu.c',
    'src/x86/x86_dnn_map.c',
    'src/x86/nnet_avx2.c',
    'src/x86/nnet_sse4_1.c',
  ]
endif

librnnoise = static_library('librnnoise',
    rnnoise_sources,
    dependencies: common_deps,
    include_directories: rnnoise_inc,
    c_args: common_cflags + avx_flags,
    cpp_args: common_cxxflags + avx_flags
)

rnnoise_dep = declare_dependency(
    link_with: librnnoise,
    include_directories: rnnoise_inc,
)

