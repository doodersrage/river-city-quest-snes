ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error "Set PVSNESLIB_HOME to a built PVSnesLib checkout: https://github.com/alekmaul/pvsneslib/wiki/Installation")
endif

# 2 MB LoROM: the outdoor maps, interiors and caves now, the dungeons later.
export ROMNAME := RiverCityQuest
export ROMTITLE := RIVER CITY QUEST
export ROMBANKS := 64
export ROMSIZE := 0B
# FastROM: code and data in banks $80 and up run at 3.58 MHz instead of 2.68
export FASTROM := 1
# Battery-backed SRAM for the three save slots (stage 11): ROM + RAM + battery, 8 KB
export CARTRIDGETYPE := 02
export SRAMSIZE := 03

# Music and sound effects (tools/mkaudio.py): the effects module must come first.
AUDIOFILES := data/audio/effects.it data/audio/city.it data/audio/south.it data/audio/dungeon.it data/audio/boss.it \
              data/audio/forge.it data/audio/win.it data/audio/title.it data/audio/night.it
export SOUNDBANK := data/audio/soundbank

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

# the soundbank goes in its own banks at the top of the ROM (banks 52 and up are free)
SMCONVFLAGS := -s -o $(SOUNDBANK) -V -b 52

.PHONY: all clean cleanLogs

all: buildWithSummary
buildActual: $(SOUNDBANK).obj $(OBJS) $(ROMNAME).sfc

clean: cleanBuildRes cleanRom cleanLogs
