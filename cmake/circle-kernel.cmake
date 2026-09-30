#
# circle-kernel.cmake - include () it in an app's CMakeLists.txt, after
# add_subdirectory () of Circle (CMakeLists.txt). circle_kernel (target):
# the target, an executable of the app's objects and Circle's libraries, is
# linked as Rules.mk links a kernel (ld with circle.ld at the load address,
# the libraries in one group with libgcc and libm), then its listing
# (<kernel>.lst) and its image (<kernel>.img: kernel.img, kernel8.img, ...
# by the board, next to the target's <kernel>.elf and <kernel>.map).
#
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_LINKER> -o <TARGET> -Map <TARGET>.map ${CIRCLE_LDFLAGS} -T ${CIRCLE_DIR}/circle.ld <OBJECTS> --start-group <LINK_LIBRARIES> ${CIRCLE_EXTRALIBS} --end-group")
string(REPLACE ";" " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")

function(circle_kernel target)
	set_target_properties(${target} PROPERTIES
		LINKER_LANGUAGE CXX
		OUTPUT_NAME ${CIRCLE_KERNEL}
		SUFFIX .elf
		LINK_DEPENDS ${CIRCLE_DIR}/circle.ld)
	set(dir $<TARGET_FILE_DIR:${target}>)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND sh -c "${CMAKE_OBJDUMP} -d ${CIRCLE_KERNEL}.elf | ${CIRCLE_CPPFILT} > ${CIRCLE_KERNEL}.lst"
		COMMAND ${CMAKE_OBJCOPY} ${CIRCLE_KERNEL}.elf -O binary ${CIRCLE_KERNEL}.img
		COMMAND sh -c "echo \"  WC    ${CIRCLE_KERNEL}.img => $(wc -c < ${CIRCLE_KERNEL}.img)\""
		WORKING_DIRECTORY ${dir}
		VERBATIM)
endfunction()
