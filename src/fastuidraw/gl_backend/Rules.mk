# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/ngl_generator
include $(dir)/Rules.mk

dir := $(d)/ngl
include $(dir)/Rules.mk

FASTUIDRAW_GL_SOURCES += $(call filelist, gl_get.cpp \
	gluniform_implement.cpp \
	gl_program.cpp gl_context_properties.cpp \
	texture_image_gl.cpp \
	painter_engine_gl.cpp)

NGL_COMMON_SRCS += $(call filelist, gl_binding.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
