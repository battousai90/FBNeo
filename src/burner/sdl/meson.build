frontend_sources = files(
    'bzip.cpp',
    'config.cpp',
    'drv.cpp',
    'inpdipsw.cpp',
    'input_sdl2.cpp',
    'ips_manager.cpp',
    'localise_gamelist.cpp',
    'main.cpp',
    'media.cpp',
    '../cdlist.cpp',
    'replay.cpp',
    'romdata.cpp',
    'run.cpp',
    'scrn.cpp',
    'sdl2_gui.cpp',
    'sdl2_gui_common.cpp',
    'sdl2_gui_ingame.cpp',
    'sdl2_inprint.cpp',
    'stated.cpp',
    'stringset.cpp',
    'support_paths.cpp',
    '../../intf/audio/sdl/aud_sdl.cpp',
    '../../intf/cd/cd_img.cpp',
    '../../intf/input/sdl/inp_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2opengl.cpp',
    '../../intf/video/vid_softfx.cpp',
    '../../intf/video/scalers/2xpm.cpp',
    '../../intf/video/scalers/2xsai.cpp',
    '../../intf/video/scalers/crt.cpp',
    '../../intf/video/scalers/ddt3x.cpp',
    '../../intf/video/scalers/epx.cpp',
    '../../intf/video/scalers/hq2xs.cpp',
    '../../intf/video/scalers/hq2xs_16.cpp',
    '../../intf/video/scalers/hq3xs.cpp',
    '../../intf/video/scalers/hq_shared32.cpp',
    '../../intf/video/scalers/xbr.cpp',
    '../../intf/video/scalers/xbr32.cpp',
)

sdl2_defs = ['-DBUILD_SDL2', '-DUSE_FILE32API', '-DHAVE_UNISTD_H']
if host_machine.system() == 'windows'
    sdl2_defs += ['-DSDL_WINDOWS']
endif

frontend_dep = declare_dependency(
    include_directories: include_directories(
        '.',
        '../../intf/input/sdl',
        '../../intf/audio/sdl',
        '../../intf/video/sdl',
    ),
    dependencies: [
        dependency('SDL2_image'),
        dependency('opengl'),
    ],
    compile_args: sdl2_defs,
)

frontend_link_args = []
