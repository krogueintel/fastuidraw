# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

FASTUIDRAW_SOURCES += $(call filelist, static_resource.cpp \
	fastuidraw_memory.cpp util.cpp \
	reference_count_atomic.cpp \
	pixel_distance_math.cpp data_buffer.cpp api_callback.cpp \
	string_array.cpp mutex.cpp blend_mode.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
