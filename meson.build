project(
    'mediamtx-rpicamera',
    'c', 'cpp',
    version : '1.0.0',
    meson_version : '>=1.0.0',
    default_options: [
        # 'c_std=c18',
        'cpp_std=c++17',
        'buildtype=release',
        'strip=true',
        'prefix=/'
    ]
)

c_args = [
    '-Ofast',
    '-Werror',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-unused-result'
]

cpp_args = [
    '-Ofast',
    '-Werror',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-unused-result'
]

add_project_arguments(c_args, language : 'c')
add_project_arguments(cpp_args, language : 'cpp')

x264 = subproject(
    'x264',
    default_options : [
        'default_library=static',
        'cli=false',
        'strip=true',
        'interlaced=false',
        'bit-depth=8',
        'chroma-format=all'
    ])
x264_dep = x264.get_variable('libx264_dep')

libcamera = subproject(
    'libcamera',
    default_options : [
        'buildtype=release',
        'strip=true',
        'wrap_mode=forcefallback',
        'lc-compliance=disabled',
        'ipas=rpi/vc4',
        'pipelines=rpi/vc4',
        'cam=disabled',
        'documentation=disabled',
        'gstreamer=disabled',
        'pycamera=disabled',
        'qcam=disabled',
        'tracing=disabled',
        'udev=disabled'
    ])
libcamera_dep = libcamera.get_variable('libcamera_public')

cmake = import('cmake')
freetype_vars = cmake.subproject_options()
freetype_vars.add_cmake_defines({
    'CMAKE_BUILD_TYPE': 'Release',
    'BUILD_SHARED_LIBS': false,
    'FT_DISABLE_ZLIB': true,
    'FT_DISABLE_BZIP2': true,
    'FT_DISABLE_PNG': true,
    'FT_DISABLE_HARFBUZZ': true,
    'FT_DISABLE_BROTLI': true
})
freetype = cmake.subproject('freetype', options : freetype_vars)
freetype_dep = freetype.dependency('freetype')

dependencies = [
    x264_dep,
    libcamera_dep,
    freetype_dep
]

text_font = custom_target(
    'text_font.h',
    output : 'text_font.h',
    command : ['./text_font.sh']
)

sources = [
    'base64.c',
    'camera.cpp',
    'encoder_hard_h264.c',
    'encoder_soft_h264.c',
    'encoder.c',
    'main.c',
    'parameters.c',
    'pipe.c',
    'sensor_mode.c',
    'text.c',
    'window.c',
    text_font
]

mtxrpicam = executable(
    'mtxrpicam',
    sources,
    dependencies : dependencies,
    install : true
)

meson.add_install_script('post_install.sh')
