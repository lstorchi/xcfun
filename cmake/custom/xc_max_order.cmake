# XC_MAX_ORDER is the maximum order of derivatives of the exchange-correlation kernel
if(DEFINED XC_MAX_ORDER)
    if(XC_MAX_ORDER LESS 3)
        # make sure it is larger than 2
        message(STATUS "Resetting XC_MAX_ORDER to its minimum admissible value (3)")
        set(XC_MAX_ORDER 3)
    endif()
else()
    # make sure it is not undefined
    set(XC_MAX_ORDER 3)
endif()

# TODO this should move closer to the target(s) where this is used
# and not be defined globally as is now
add_definitions("-DXC_MAX_ORDER=${XC_MAX_ORDER}")
