set(PFFFT_SRC pffft/src/pffft.c)
list(TRANSFORM PFFFT_SRC PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

set(RNNOISE_SRC rnnoise/src/rnn_vad_weights.cc
  rnnoise/src/celt_lpc.c
  rnnoise/src/denoise.c
  rnnoise/src/kiss_fft.c
  rnnoise/src/pitch.c
  rnnoise/src/rnn.c
  rnnoise/src/nnet.c
  rnnoise/src/nnet_default.c
  rnnoise/src/parse_lpcnet_weights.c
  rnnoise/src/rnnoise_tables.c
  rnnoise/src/rnnoise_data.c)
if(have_avx2)
    list(APPEND RNNOISE_SRC
      rnnoise/src/x86/nnet_avx2.c
      rnnoise/src/x86/nnet_sse4_1.c
      rnnoise/src/x86/x86_dnn_map.c
      rnnoise/src/x86/x86cpu.c)
endif()
list(TRANSFORM RNNOISE_SRC PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

set(THIRD_PARTY_SRC ${RNNOISE_SRC} ${PFFFT_SRC})
set(THIRD_PARTY_SRC ${THIRD_PARTY_SRC} PARENT_SCOPE)
