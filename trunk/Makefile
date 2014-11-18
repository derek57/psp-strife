TARGET = strife
PSPSDK = $(shell psp-config --pspsdk-path)
PSPBIN = $(PSPSDK)/../bin

PSP    = yes

BUILD_PRX=1
PSP_FW_VERSION=303

LIBS = -lSDL_mixer -lsmpeg -lSDL -lGL -lGLU -lpsprtc -lpspirkeyb -lpsppower -lpspvfpu -lmad
LIBS += -lvorbisidec -lpspgum -lpspgu -lpsphprm -lm -lpspaudio -lstdc++
CFLAGS = -O2 -g -Wall --fast-math -fno-unit-at-a-time -fdiagnostics-show-option -fno-exceptions
CFLAGS += -G0 -I/usr/local/pspdev/psp/include/SDL

OBJS =											\
am_map.o d_items.o d_main.o d_net.o doomdef.o doomstat.o dstrings.o f_finale.o f_wipe.o	\
g_game.o hu_lib.o hu_stuff.o info.o m_menu.o m_random.o m_saves.o p_ceilng.o p_dialog.o	\
p_doors.o p_enemy.o p_floor.o p_inter.o p_lights.o p_map.o p_maputl.o p_mobj.o		\
p_plats.o p_pspr.o p_saveg.o p_setup.o p_sight.o p_spec.o p_switch.o p_telept.o		\
p_tick.o p_user.o r_bsp.o r_data.o r_draw.o r_main.o r_plane.o r_segs.o r_sky.o		\
r_things.o s_sound.o sounds.o st_lib.o st_stuff.o					\
											\
i_endoom.o i_main.o i_system.o m_misc.o d_event.o d_iwad.o d_loop.o d_mode.o m_argv.o	\
i_sound.o i_timer.o i_video.o m_bbox.o m_config.o m_controls.o m_fixed.o midifile.o	\
mus2mid.o sha1.o memio.o tables.o v_video.o w_checksum.o w_main.o w_wad.o w_file.o	\
w_file_stdc.o z_zone.o i_oplmusic.o i_sdlsound.o i_sdlmusic.o opl.o opl_sdl.o		\
opl_queue.o opl_timer.o dbopl.o	txt_sdl.o

OBJS   += disablefpuexceptions.o

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-Strife by nitr8 (R5)
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_UNKPNG = PIC0.PNG
PSP_EBOOT_SND0 = SND0.AT3
#PSP_EBOOT_PIC1 = PIC1.PNG

ifeq ($(PSP),yes)
include $(PSPSDK)/lib/build.mak
else

all: strife

strife: $(OBJS)
	$(CC) $(LIBS) $(OBJS) -o strife

clean:
	rm -f $(OBJS) strife

endif
