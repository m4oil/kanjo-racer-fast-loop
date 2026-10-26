# KANJO RACER: BEAR PRODUCTS - builds a .gba with plain devkitARM (no extra libraries).
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set - install devkitPro / use the devkitpro/devkitarm container")
endif

PREFIX  := $(DEVKITARM)/bin/arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
GBAFIX  := $(DEVKITPRO)/tools/bin/gbafix

TARGET  := kanjo_racer
ARCH    := -mthumb -mthumb-interwork
CFLAGS  := -O2 -Wall -fno-strict-aliasing -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH) \
           -ffunction-sections -fdata-sections -Isource
LDFLAGS := -specs=gba.specs $(ARCH) -Wl,--gc-sections -Wl,-Map,build/$(TARGET).map

SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,build/%.o,$(SRCS))

all: $(TARGET).gba

build/%.o: source/%.c source/gba.h source/game.h source/gfx.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	-$(GBAFIX) $@ -tKANJORACER -cKRBP -mBP

clean:
	rm -rf build $(TARGET).elf $(TARGET).gba

.PHONY: all clean
