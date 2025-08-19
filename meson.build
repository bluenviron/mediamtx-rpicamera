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
        'prefix=/',
        'wrap_mode=forcefallback'
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

libcamera_dep = dependency(
    'libcamera',
    fallback : ['libcamera', 'libcamera_public'],
    default_options : [
        'buildtype=release',
        'strip=true',
        'wrap_mode=forcefallback',
        'lc-compliance=disabled',
        'ipas=rpi/vc4,rpi/pisp',
        'pipelines=rpi/vc4,rpi/pisp',
        'cam=disabled',
        'documentation=disabled',
        'gstreamer=disabled',
        'pycamera=disabled',
        'qcam=disabled',
        'tracing=disabled',
        'udev=disabled'
    ])

freetype_dep = dependency('freetype2', required : false)
if not freetype_dep.found()
    cmake = import('cmake')
    freetype_vars = cmake.subproject_options()
    freetype_vars.set_override_option('wrap_mode', 'none')
    freetype_vars.add_cmake_defines({
        'CMAKE_BUILD_TYPE': 'Release',
        'BUILD_SHARED_LIBS': false,
        'FT_DISABLE_ZLIB': true,
        'FT_DISABLE_BZIP2': true,
        'FT_DISABLE_PNG': true,
        'FT_DISABLE_HARFBUZZ': true,
        'FT_DISABLE_BROTLI': true
    })
    freetype_proj = cmake.subproject('freetype', options : freetype_vars)
    freetype_dep = freetype_proj.dependency('freetype')
endif

libjpeg_dep = dependency('libjpeg', version: '<3.0.0', static : true)

openh264_dep = dependency(
    'openh264',
    fallback : ['openh264', 'openh264_dep'],
    static : true,
    default_options : [
        'tests=disabled'
    ])

dependencies = [
    libcamera_dep,
    freetype_dep,
    libjpeg_dep,
    openh264_dep
]

text_font = custom_target(
    'text_font.h',
    output : 'text_font.h',
    command : ['./text_font.sh']
)

sources = [
    'base64.c',
    'camera.cpp',
    'encoder_hardware_h264.c',
    'encoder_jpeg.c',
    'encoder_software_h264.cpp',
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
