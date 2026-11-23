TARGET          := break_room
BUILD           := build
LIBBUTANO       ?= ../gba_development/butano/butano
PYTHON          ?= python3
SOURCES         := src
INCLUDES        := include
GRAPHICS        := graphics
AUDIO           := audio
AUDIOBACKEND    := maxmod
DMGAUDIOBACKEND := null
ROMTITLE        := BREAK ROOM
ROMCODE         := BR8P
export DEVKITPRO ?= /opt/devkitpro
export DEVKITARM ?= $(DEVKITPRO)/devkitARM
ifndef LIBBUTANOABS
export LIBBUTANOABS := $(realpath $(LIBBUTANO))
endif

ifeq ($(LIBBUTANOABS),)
$(error Set LIBBUTANO to the butano directory inside your Butano checkout)
endif

include $(LIBBUTANOABS)/butano.mak
