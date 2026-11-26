ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error "Set PVSNESLIB_HOME to a built PVSnesLib checkout: https://github.com/alekmaul/pvsneslib/wiki/Installation")
endif

# 2 MB LoROM: the outdoor maps, interiors and caves now, the dungeons later.
export ROMNAME := RiverCityQuest
export ROMTITLE := RIVER CITY QUEST
export ROMBANKS := 64
export ROMSIZE := 0B

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

.PHONY: all clean cleanLogs

all: buildWithSummary
buildActual: $(OBJS) $(ROMNAME).sfc

clean: cleanBuildRes cleanRom cleanLogs
