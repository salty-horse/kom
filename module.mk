MODULE := engines/kom

MODULE_OBJS := \
	kom.o \
	screen.o \
	character.o \
	database.o \
	actor.o \
	input.o \
	sound.o \
	panel.o \
	game.o \
	conv.o \
	font.o \
	debugger.o \
	video_player.o \
	detection.o \
	metaengine.o

# This module can be built as a plugin
ifeq ($(ENABLE_KOM), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules 
include $(srcdir)/rules.mk

