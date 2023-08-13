#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		:=	build
SOURCES		:=	source/cpu/z80 source/cpu/z80/drz80 source/cpu/z80/cz80 source/cpu/tms9918a source/cpu/sn76496 source/cpu/m6502 source 
INCLUDES	:=	include 
DATA		:=	data
GRAPHICS	:=	gfx
SPRITES		:=  spr_data
BACKGRD		:=  gfx_data

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
#ARCH	:=	-mthumb -mthumb-interwork
ARCH	:=

CFLAGS	:= -Wall -Wno-strict-aliasing -O3 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math $(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9
CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH) -march=armv5te -mtune=arm946e-s
ASFLAGS	:=	$(ARCH) -march=armv5te -mtune=arm946e-s

#LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -mno-fpu -Wl,-Map,$(notdir $*.map)
LDFLAGS	=	-specs=ds_arm9.specs $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -ldswifi9 -lfat -lnds9 -lmm9
 
#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(LIBNDS)
 
#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
 
export ARM9ELF	:=	$(CURDIR)/$(TARGET).elf
export DEPSDIR := $(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
 					$(foreach dir,$(SPRITES),$(CURDIR)/$(dir)) \
          $(foreach dir,$(BACKGRD),$(CURDIR)/$(dir))
 
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
PNGFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))

OSPECIALS :=  colecovision.o options.o debug_ovl.o adam_sm.o cvision_kbd.o alpha_kbd.o msx_sm.o msx_full.o mtx_full.o sc3000_kbd.o svi_full.o msx_japan.o adam_full.o einstein_kbd.o pv2000_sm.o cvision.o topscreen.o wargames.o gateway.o mousetrap.o fixupmixup.o spyhunter.o boulder.o quest.o hal2010.o pdev_bg0.o pdev_tbg0.o  

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(OSPECIALS) $(addsuffix .o,$(BINFILES)) \
					$(PNGFILES:.png=.o) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o) 
 
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
 
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
 
.PHONY: $(BUILD) clean
 
#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) *.elf *.nds* *.bin 
 
 
#---------------------------------------------------------------------------------
else
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(ARM9ELF)	:	$(OFILES)
	@echo linking $(notdir $@)
	@$(LD)  $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
	@echo $(notdir $<)
	@$(bin2o)

%.ROM.o	:	%.ROM
	@echo $(notdir $<)
	@$(bin2o)

%.bmp.o	:	%.bmp
	@echo $(notdir $<)
	@$(bin2o)

%.wav.o	:	%.wav
	@echo $(notdir $<)
	@$(bin2o)

# BG------------------------
colecovision.s colecovision.h : colecovision.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

adam_sm.s adam_sm.h : adam_sm.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

cvision_kbd.s cvision_kbd.h : cvision_kbd.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

alpha_kbd.s alpha_kbd.h : alpha_kbd.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

msx_sm.s msx_sm.h : msx_sm.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

cvision.s cvision.h : cvision.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

msx_full.s msx_full.h : msx_full.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

msx_japan.s msx_japan.h : msx_japan.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

mtx_full.s mtx_full.h : mtx_full.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

sc3000_kbd.s sc3000_kbd.h : sc3000_kbd.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

svi_full.s svi_full.h : svi_full.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

adam_full.s adam_full.h : adam_full.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

einstein_kbd.s einstein_kbd.h : einstein_kbd.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

pv2000_sm.s pv2000_sm.h : pv2000_sm.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

debug_ovl.s debug_ovl.h : debug_ovl.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

options.s options.h : options.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl
    
topscreen.s topscreen.h : topscreen.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

wargames.s wargames.h : wargames.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

gateway.s gateway.h : gateway.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

mousetrap.s mousetrap.h : mousetrap.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

fixupmixup.s fixupmixup.h : fixupmixup.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

spyhunter.s spyhunter.h : spyhunter.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

boulder.s boulder.h : boulder.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

quest.s quest.h : quest.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

hal2010.s hal2010.h : hal2010.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

pdev_bg0.s pdev_bg0.h : pdev_bg0.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

pdev_tbg0.s pdev_tbg0.h : pdev_tbg0.png
	grit $^ -o $@  -gt -mrt -mR8 -mLs -gzl -mzl

  
-include $(DEPENDS)
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
