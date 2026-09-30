#
# Circle's libraries built with CMake, out of the source tree: one source tree,
# a build directory for each configuration (the Makefiles build in place, one
# configuration a tree). The flags and the object lists follow Rules.mk and
# the libraries' Makefiles; so far the libraries and add-ons below.
#
#   cmake -S <app> -B <build> -DCMAKE_TOOLCHAIN_FILE=<circle>/cmake/toolchain.cmake \
#         -DCIRCLE_PREFIX=<toolchain prefix> -DCIRCLE_RASPPI=<1..5> \
#         "-DCIRCLE_DEFINES=OPTION;NAME=VALUE;..."
#
# with an app's CMakeLists.txt that does add_subdirectory (<circle> circle) and
# include (<circle>/cmake/circle-kernel.cmake): circle_kernel (target). The
# architecture (32 or 64 bit) comes from the toolchain, as with configure.
#
# Options (configure's): CIRCLE_RASPPI, CIRCLE_DEFINES (configure's -d, e.g.
# ARM_ALLOW_MULTI_CORE), CIRCLE_STDLIB_SUPPORT (0 or 1), CIRCLE_FLOAT_ABI.
#
cmake_minimum_required(VERSION 3.20)
project(circle C CXX ASM)

set(CIRCLE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")
set(CIRCLE_RASPPI 1 CACHE STRING "Raspberry Pi model (1, 2, 3, 4, 5)")
set(CIRCLE_DEFINES "" CACHE STRING "system options (configure's -d), a list")
set(CIRCLE_STDLIB_SUPPORT 1 CACHE STRING "0 or 1 (Rules.mk's STDLIB_SUPPORT)")
set(CIRCLE_FLOAT_ABI hard CACHE STRING "Rules.mk's FLOAT_ABI (32 bit)")
set(CIRCLE_VERSION 510100)			# Rules.mk's CIRCLEVER

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(CIRCLE_AARCH 64 CACHE INTERNAL "")
else()
	set(CIRCLE_AARCH 32 CACHE INTERNAL "")
endif()
set(RASPPI ${CIRCLE_RASPPI})
set(AARCH ${CIRCLE_AARCH})

# the architecture (Rules.mk: ARCHCPU, TARGET, LOADADDR)
if(AARCH EQUAL 32)
	if(RASPPI EQUAL 1)
		set(archcpu -mcpu=arm1176jzf-s -marm -mfpu=vfp -mfloat-abi=${CIRCLE_FLOAT_ABI})
		set(kernel kernel)
	elseif(RASPPI EQUAL 2)
		set(archcpu -mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=${CIRCLE_FLOAT_ABI})
		set(kernel kernel7)
	elseif(RASPPI EQUAL 3)
		set(archcpu -mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=${CIRCLE_FLOAT_ABI})
		set(kernel kernel8-32)
	elseif(RASPPI EQUAL 4)
		set(archcpu -mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=${CIRCLE_FLOAT_ABI})
		set(kernel kernel7l)
	else()
		message(FATAL_ERROR "CIRCLE_RASPPI must be 1, 2, 3 or 4 (32 bit)")
	endif()
	set(CIRCLE_LOADADDR 0x8000 CACHE INTERNAL "")
	set(unaligned -mno-unaligned-access)
else()
	if(RASPPI EQUAL 3)
		set(archcpu -mcpu=cortex-a53 -mlittle-endian)
		set(kernel kernel8)
	elseif(RASPPI EQUAL 4)
		set(archcpu -mcpu=cortex-a72 -mlittle-endian)
		set(kernel kernel8-rpi4)
	elseif(RASPPI EQUAL 5)
		set(archcpu -mcpu=cortex-a76 -mlittle-endian)
		set(kernel kernel_2712)
	else()
		message(FATAL_ERROR "CIRCLE_RASPPI must be 3, 4 or 5 (64 bit)")
	endif()
	set(CIRCLE_LOADADDR 0x80000 CACHE INTERNAL "")
	set(unaligned -mstrict-align)
endif()
set(CIRCLE_KERNEL ${kernel} CACHE INTERNAL "")		# the image's name, without .img
set(CIRCLE_UNALIGNED ${unaligned} CACHE INTERNAL "")	# what lib/Makefile adds to CFLAGS

# the libraries of the link's group (Rules.mk: EXTRALIBS with STDLIB_SUPPORT 1)
execute_process(COMMAND ${CMAKE_C_COMPILER} ${archcpu} -print-file-name=libgcc.a
		OUTPUT_VARIABLE libgcc OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CIRCLE_EXTRALIBS ${libgcc})
if(CIRCLE_STDLIB_SUPPORT EQUAL 1)
	execute_process(COMMAND ${CMAKE_C_COMPILER} ${archcpu} -print-file-name=libm.a
			OUTPUT_VARIABLE libm OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT libm STREQUAL "libm.a")
		list(APPEND CIRCLE_EXTRALIBS ${libm})
	endif()
endif()
set(CIRCLE_EXTRALIBS ${CIRCLE_EXTRALIBS} CACHE INTERNAL "")
execute_process(COMMAND ${CMAKE_LINKER} --help OUTPUT_VARIABLE ldhelp)
if(ldhelp MATCHES "no-warn-rwx-segments")
	set(CIRCLE_LDFLAGS --section-start=.init=${CIRCLE_LOADADDR} --no-warn-rwx-segments CACHE INTERNAL "")
else()
	set(CIRCLE_LDFLAGS --section-start=.init=${CIRCLE_LOADADDR} CACHE INTERNAL "")
endif()

# the flags of every Circle source and of the app's (Rules.mk: ARCH, DEFINE,
# INCLUDE, OPTIMIZE; CFLAGS and CPPFLAGS for C and C++, AFLAGS for assembly)
add_library(circle_flags INTERFACE)
set(define)
foreach(d ${CIRCLE_DEFINES})
	list(APPEND define -D${d})
endforeach()
list(APPEND define -D__circle__=${CIRCLE_VERSION} -DRASPPI=${RASPPI} -DSTDLIB_SUPPORT=${CIRCLE_STDLIB_SUPPORT}
     -D__VCCOREVER__=0x04000000 -U__unix__ -U__linux__)
target_compile_options(circle_flags INTERFACE
	-DAARCH=${AARCH} ${archcpu}
	$<$<COMPILE_LANGUAGE:C,CXX>:-Wall -fsigned-char -g>
	${define}
	-O2
	$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<IN_LIST:${CIRCLE_STDLIB_SUPPORT},0;1>>:-ffreestanding>
	$<$<COMPILE_LANGUAGE:C>:-std=gnu99>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -nostdinc++ -std=c++17>)
set(CIRCLE_INCLUDE_DIRS ${CIRCLE_DIR}/include ${CIRCLE_DIR}/addon ${CIRCLE_DIR}/app/lib
    ${CIRCLE_DIR}/addon/vc4 ${CIRCLE_DIR}/addon/vc4/interface/khronos/include CACHE INTERNAL "")
target_include_directories(circle_flags INTERFACE ${CIRCLE_INCLUDE_DIRS})
# (an app's own include directories go after these, as Rules.mk's EXTRAINCLUDE
# does: target_include_directories (app PRIVATE ${CIRCLE_INCLUDE_DIRS} ...),
# else CMake puts the app's first, and a header of the same name elsewhere
# would win)

# a library of a directory's objects (as its Makefile lists them: the source
# of each is its .cpp, .c or .S), __FILE__ as a build in that directory gives it
function(circle_library name dir)
	set(sources)
	foreach(obj ${ARGN})
		string(REGEX REPLACE "\\.o$" "" base ${obj})
		set(found)
		foreach(ext cpp c S)
			if(NOT found AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${base}.${ext})
				set(found ${dir}/${base}.${ext})
			endif()
		endforeach()
		if(NOT found)
			message(FATAL_ERROR "${dir}/${obj}: no source")
		endif()
		list(APPEND sources ${found})
	endforeach()
	add_library(${name} STATIC ${sources})
	target_link_libraries(${name} PUBLIC circle_flags)
	target_compile_options(${name} PRIVATE -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/${dir}/=)
	set_target_properties(${name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${dir})
endfunction()

# lib (lib/Makefile)
set(objs actled.o alloc.o assert.o display.o windowdisplay.o bcmframebuffer.o bcmmailbox.o
	bcmpropertytags.o bcmwatchdog.o chargenerator.o classallocator.o
	cputhrottle.o debug.o delayloop.o device.o devicenameservice.o
	dmachannel.o
	koptions.o
	logger.o machineinfo.o multicore.o nulldevice.o ptrarray.o ptrlist.o
	qemu.o terminal.o screen.o serial.o
	spinlock.o
	string.o sysinit.o time.o timer.o tracer.o util.o
	util_fast.o virtualgpiopin.o chainboot.o macaddress.o netdevice.o
	new.o heapallocator.o pageallocator.o setjmp.o numberpool.o
	writebuffer.o 2dgraphics.o ptrlistfiq.o
	font6x7.o font8x8.o font8x10.o font8x12.o font8x14.o font8x16.o font12x22.o)
if(AARCH EQUAL 32)
	list(APPEND objs cache-v7.o exceptionhandler.o exceptionstub.o memory.o pagetable.o
	     startup.o synchronize.o)
	if(CIRCLE_STDLIB_SUPPORT EQUAL 0)
		list(APPEND objs libhelper.o libstub.o)
	endif()
else()
	list(APPEND objs exceptionhandler64.o exceptionstub64.o memory64.o startup64.o
	     synchronize64.o translationtable64.o)
endif()
if(RASPPI LESS_EQUAL 3)
	list(APPEND objs bcmrandom.o interrupt.o mphi.o)
else()
	list(APPEND objs bcmpciehostbridge.o bcmrandom200.o interruptgic.o dma4channel.o devicetreeblob.o)
endif()
if(RASPPI EQUAL 4)
	list(APPEND objs bcm54213.o)
endif()
if(NOT RASPPI EQUAL 5)
	list(APPEND objs gpioclock.o gpiomanager.o gpiopin.o gpiopinfiq.o i2cmaster.o i2cmasterirq.o i2cslave.o
	     pwmoutput.o smimaster.o spimaster.o spimasteraux.o spimasterdma.o usertimer.o
	     latencytester.o)
else()
	list(APPEND objs southbridge.o dmachannel-rp1.o gpiomanager2712.o gpiopin2712.o gpioclock-rp1.o
	     pwmoutput-rp1.o i2cmaster-rp1.o spimaster-rp1.o spimasterdma-rp1.o macb.o)
endif()
list(APPEND objs purecall.o cxa_guard.o)		# (STDLIB_SUPPORT not 3 or 4)
circle_library(circle lib ${objs})
target_compile_options(circle PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${CIRCLE_UNALIGNED}>)

# lib/usb
set(objs lan7800.o smsc951x.o usbbluetooth.o usbcdcethernet.o usbfloppydevice.o
	usbconfigparser.o usbdevice.o usbdevicefactory.o usbendpoint.o usbfunction.o
	usbgamepad.o usbgamepadps3.o usbgamepadps4.o usbgamepadstandard.o usbgamepadswitchpro.o
	usbgamepad8bitdo.o usbgamepad8bitdopro.o usbgamepad8bitdoxinput.o
	usbgamepadxbox360.o usbgamepadxbox360pcwireless.o usbgamepadxboxone.o usbhiddevice.o usbhostcontroller.o
	usbkeyboard.o usbkeyboard8bitdo.o usbmassdevice.o usbmidi.o usbmidihost.o usbmouse.o usbprinter.o usbrequest.o
	usbstandardhub.o usbstring.o usbserial.o usbserialhost.o usbserialch341.o usbserialcp210x.o
	usbserialpl2303.o usbserialft231x.o usbserialcdc.o usbtouchscreen.o dwhciregister.o)
if(RASPPI LESS_EQUAL 3)
	list(APPEND objs dwhcidevice.o dwhciframeschednper.o dwhciframeschednsplit.o dwhciframeschedper.o
	     dwhcirootport.o dwhcixactqueue.o dwhcicompletionqueue.o
	     dwhcixferstagedata.o dwhciframeschediso.o)
else()
	list(APPEND objs xhcicommandmanager.o xhcidevice.o xhciendpoint.o xhcieventmanager.o xhcimmiospace.o
	     xhciring.o xhciroothub.o xhcirootport.o xhcisharedmemallocator.o xhcislotmanager.o
	     xhciusbdevice.o usbaudiocontrol.o usbaudiostreaming.o usbaudiofunctopology.o)
endif()
if(RASPPI EQUAL 5)
	list(APPEND objs usbsubsystem.o)
endif()
circle_library(circle_usb lib/usb ${objs})
target_compile_options(circle_usb PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${CIRCLE_UNALIGNED}>)

# lib/usb/gadget
if(NOT RASPPI EQUAL 5)
	circle_library(circle_usbgadget lib/usb/gadget dwusbgadget.o dwusbgadgetendpoint.o dwusbgadgetendpoint0.o
		       usbmidigadget.o usbmidigadgetendpoint.o
		       usbcdcgadget.o usbcdcgadgetendpoint.o
		       usbmsdgadget.o usbmsdgadgetendpoint.o)
endif()

# lib/input
set(objs keyboardbehaviour.o keymap.o mousebehaviour.o mouse.o
	touchscreen.o xpt2046touchscreen.o
	console.o keyboardbuffer.o linediscipline.o)
if(NOT RASPPI EQUAL 5)
	list(APPEND objs rpitouchscreen.o)
endif()
circle_library(circle_input lib/input ${objs})

# lib/fs, lib/sched
circle_library(circle_fs lib/fs partition.o partitionmanager.o)
circle_library(circle_sched lib/sched task.o scheduler.o taskswitch.o synchronizationevent.o mutex.o
	       semaphore.o pipe.o)

# lib/sound
set(objs soundbasedevice.o pwmsounddevice.o hdmisoundbasedevice.o
	pcm512xsoundcontroller.o wm8960soundcontroller.o)
if(NOT RASPPI EQUAL 5)
	list(APPEND objs dmasoundbuffers.o i2ssoundbasedevice.o pwmsoundbasedevice.o)
else()
	list(APPEND objs i2ssoundbasedevice-rp1.o pwmsoundbasedevice-rp1.o)
endif()
if(RASPPI GREATER_EQUAL 4)
	list(APPEND objs usbsoundbasedevice.o usbsoundcontroller.o)
endif()
circle_library(circle_sound lib/sound ${objs})

# add-ons
set(objs emmc.o)
if(NOT RASPPI EQUAL 5)
	list(APPEND objs mmchost.o sdhost.o)
endif()
circle_library(circle_sdcard addon/SDCard ${objs})
circle_library(circle_fatfs addon/fatfs ff.o diskio.o ffsystem.o ffunicode.o)
circle_library(circle_linuxemu addon/linux linuxdevice.o linuxemu.o
	       bug.o completion.o delay.o device.o dma-mapping.o interrupt.o kthread.o
	       mutex.o platform_device.o printk.o pthread.o raspberrypi-firmware.o rwlock.o
	       semaphore.o spinlock.o sprintf.o timer.o)
circle_library(circle_vcos addon/vc4/interface/vcos vcos_abort.o vcos_backtrace.o vcos_generic_blockpool.o
	       vcos_generic_event_flags.o vcos_generic_named_sem.o vcos_generic_reentrant_mtx.o
	       vcos_generic_safe_string.o vcos_init.o vcos_logcat.o vcos_mem_from_malloc.o
	       vcos_msgqueue.o vcos_pthreads.o)
circle_library(circle_vchiq addon/vc4/vchiq vchiqdevice.o
	       vchiq_arm.o vchiq_2835_arm.o vchiq_core.o vchiq_kern_lib.o vchiq_connected.o
	       vchiq_shim.o vchiq_util.o)
target_compile_options(circle_vchiq PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:${CIRCLE_UNALIGNED}>)
circle_library(circle_vchiqsound addon/vc4/sound vchiqsoundbasedevice.o vchiqsounddevice.o
	       vchiqsoundcontroller.o)
