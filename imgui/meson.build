
sdl3 = subproject('sdl3')
executable('imgui_demo', ['main.cpp',
    'imgui.cpp',
    'imgui_demo.cpp',
    'imgui_draw.cpp',
    'imgui_tables.cpp',
    'imgui_widgets.cpp',
    'backends/imgui_impl_opengl3.cpp',
    'backends/imgui_impl_sdl3.cpp',
  ],
  dependencies: [sdl3.get_variable('sdl3_dep'), audio_processing_dep, absl_dep],
  install: true)