ifeq ($(MAKECMDGOALS),check)

ifeq (, $(shell which $(LEX)))
CHECK_LEX := "Cannot find $(LEX) for lex/flex tool in path. Install package with tool, for example with Ubuntu using flex, apt install flex"
else
CHECK_LEX := "Found $(LEX) for lex/flex in path"
endif

CHECK_FREETYPE_CFLAGS_CODE := $(shell pkg-config freetype2 --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FREETYPE_CFLAGS_CODE), 0)
CHECK_FREETYPE_CFLAGS := "Found cflags for freetype2: $(shell pkg-config freetype2 --cflags)"
else
CHECK_FREETYPE_CFLAGS := "Cannot build FastUIDraw: Unable to find freetype2 cflags from pkg-config: Install package with fontconfig development files, for example on Ubuntu, apt install libfreetype6-dev"
FASTUIDRAW_CAN_BUILD := 0
endif

CHECK_FREETYPE_LIBS_CODE := $(shell pkg-config freetype2 --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FREETYPE_LIBS_CODE), 0)
CHECK_FREETYPE_LIBS := "Found libs for freetype2: $(shell pkg-config freetype2 --libs)"
else
CHECK_FREETYPE_LIBS := "Cannot build FastUIDraw: Unable to find freetype2 libs from pkg-config: Install package with fontconfig development files, for example on Ubuntu, apt install libfreetype6-dev"
endif

#####################################
## check for each of the GL headers
define checkglheader
$(eval FILE_$(1)_$(2):="$(GL_INCLUDEPATH)/$$(2)"
EXISTS_$(1)_$(2):= $$(shell test -e $$(FILE_$(1)_$(2)) && $(ECHO) -n yes)
ifeq ($$(EXISTS_$(1)_$(2)),yes)
FOUND_$(1)_HEADERS += $$(FILE_$(1)_$(2))
HAVE_FOUND_$(1)_HEADERS := 1
else
MISSING_$(1)_HEADERS += $$(FILE_$(1)_$(2))
HAVE_MISSING_$(1)_HEADERS:=1
endif
)
endef

ifeq ($(BUILD_GL), 1)
FOUND_GL_HEADERS:=
HAVE_FOUND_GL_HEADERS:=0
MISSING_GL_HEADERS:=
HAVE_MISSING_GL_HEADERS:=0
$(foreach headerfile,$(GL_RAW_HEADER_FILES),$(call checkglheader,GL,$(headerfile)))

ifneq ($(HAVE_FOUND_GL_HEADERS), 0)
CHECK_GL_HEADERS := "GL headers found: $(FOUND_GL_HEADERS)"
endif
ifneq ($(HAVE_MISSING_GL_HEADERS), 0)
CHECK_GL_HEADERS := "GL headers missing: $(MISSING_GL_HEADERS): Either get the files directly from khronos or install a package that includes them"
endif

endif

ifeq ($(BUILD_GLES), 1)
FOUND_GLES_HEADERS:=
HAVE_FOUND_GLES_HEADERS:=0
MISSING_GLES_HEADERS:=
HAVE_MISSING_GLES_HEADERS:=0
$(foreach headerfile,$(GLES_RAW_HEADER_FILES),$(call checkglheader,GLES,$(headerfile)))

ifneq ($(HAVE_FOUND_GLES_HEADERS), 0)
CHECK_GLES_HEADERS := "GLES headers found: $(FOUND_GLES_HEADERS)"
endif
ifneq ($(HAVE_MISSING_GLES_HEADERS), 0)
CHECK_GLES_HEADERS := "GLES headers missing: $(MISSING_GLES_HEADERS): Either get the files directly from khronos or install a package that includes them"
endif

endif

###################################################
## Check for demo requirements: SDL2_image and fontconfig
CHECK_SDL_CFLAGS_CODE := $(shell pkg-config SDL2_image --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_SDL_CFLAGS_CODE), 0)
CHECK_SDL_CFLAGS := "Found cflags for SDL2_image: $(shell pkg-config SDL2_image --cflags)"
else
CHECK_SDL_CFLAGS := "Cannot build demos: Unable to find SDL2_image cflags from pkg-config SDL2_image: Install package with SDL2_image development files, for example on Ubuntu, apt install libsdl2-image-dev"
endif

CHECK_SDL_LIBS_CODE := $(shell pkg-config SDL2_image --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_SDL_LIBS_CODE), 0)
CHECK_SDL_LIBS := "Found libs for SDL2_image: $(shell pkg-config SDL2_image --libs)"
else
CHECK_SDL_LIBS := "Cannot build demos: Unable to find SDL2_image libs from pkg-config SDL2_image: Install package with SDL2_image development files, for example on Ubuntu, apt install libsdl2-image-dev"
endif

ifeq ($(DEMOS_HAVE_FONT_CONFIG),1)
CHECK_FONTCONFIG_CFLAGS_CODE := $(shell pkg-config fontconfig --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FONTCONFIG_CFLAGS_CODE), 0)
CHECK_FONTCONFIG_CFLAGS := "Found cflags for fontconfig: $(shell pkg-config fontconfig --cflags)"
else
CHECK_FONTCONFIG_CFLAGS := "Cannot build demos: Unable to find fontconfig cflags from pkg-config: Install package with fontconfig development files, for example on Ubuntu, apt install libfontconfig1-dev"
endif

CHECK_FONTCONFIG_LIBS_CODE := $(shell pkg-config fontconfig --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FONTCONFIG_LIBS_CODE), 0)
CHECK_FONTCONFIG_LIBS := "Found libs for fontconfig: $(shell pkg-config fontconfig --libs)"
else
CHECK_FONTCONFIG_LIBS := "Cannot build demos: Unable to find fontconfig libs from pkg-config: Install package with fontconfig development files, for example on Ubuntu, apt install libfontconfig1-dev"
endif

endif

check:
	@$(ECHO) "$(CHECK_LEX)"
	@$(ECHO) "$(CHECK_SDL_CFLAGS)"
	@$(ECHO) "$(CHECK_SDL_LIBS)"
ifeq ($(DEMOS_HAVE_FONT_CONFIG),1)
	@$(ECHO) "$(CHECK_FONTCONFIG_CFLAGS)"
	@$(ECHO) "$(CHECK_FONTCONFIG_LIBS)"
endif
	@$(ECHO) "$(CHECK_FREETYPE_CFLAGS)"
	@$(ECHO) "$(CHECK_FREETYPE_LIBS)"
ifeq ($(BUILD_GL), 1)
	@$(ECHO) "$(CHECK_GL_HEADERS)"
endif
ifeq ($(BUILD_GLES), 1)
	@$(ECHO) "$(CHECK_GLES_HEADERS)"
endif

.PHONY: check

endif
