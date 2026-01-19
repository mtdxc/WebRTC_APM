
sdl3 = subproject('sdl3')
imgui = subproject('imgui', default_options : ['default_library=static'])
executable('imgui_demo', 'main.cpp',
  dependencies: [sdl3.get_variable('sdl3_dep'), imgui.get_variable('imgui_dep'), audio_processing_dep, absl_dep],
  install: true)