# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/3rd_party
include $(dir)/Rules.mk

dir := $(d)/private
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
