
# mac使用subproject的sdl3无法使用opengl3，这边用回系统的sdl3，要先运行brew install sdl3
sdl3_dep = dependency('sdl3')
imgui = subproject('imgui', default_options : ['default_library=static'])

# Copy Font Awesome font to build dir and install with executable if present
fs = import('fs')
font_file = 'fa-solid-900.ttf'
if fs.exists(join_paths(meson.current_source_dir(), font_file))
  # Copy to current build directory so runtime can find it
  configure_file(input: font_file, output: font_file, copy: true)
  # Install alongside the executable, since the app looks in CWD
  install_data(font_file, install_dir: get_option('bindir'))
endif

executable('imgui_demo', 'main.cpp',
  dependencies: [sdl3_dep, imgui.get_variable('imgui_dep'), audio_processing_dep],
  install: true)

