ifeq ($(strip $(PVSNESLIB_HOME)),)
$(error "Set PVSNESLIB_HOME to a built PVSnesLib checkout: https://github.com/alekmaul/pvsneslib/wiki/Installation")
endif

# 1 MB LoROM: room for all 24 neighborhoods now, the dungeons and interiors later.
export ROMNAME := RiverCityQuest
export ROMTITLE := RIVER CITY QUEST
export ROMBANKS := 32
export ROMSIZE := 0A

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

.PHONY: all clean cleanLogs

all: buildWithSummary
buildActual: $(OBJS) $(ROMNAME).sfc

clean: cleanBuildRes cleanRom cleanLogs
