# =============================================================================
# Makefile for building AVR peripherals (use with Borland BCC 5.5)
#
# Copyright (C) 2010 Wojciech Stryjewski <thvortex@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#


# All the DLL files that need to be included in "mculib"
all:  dummy168.dll timer0_168.dll timer2_168.dll timerN_168.dll \
      wdog.dll comp.dll eeprom.dll adc.dll

# Resource files need explicit dependencies (not handled by .autodepend)
dummy168.dll:     dummy168.res
timer0_168.dll:   timer0_168.res
timer2_168.dll:   timer2_168.res
timerN_168.dll:   timerN_168.res
wdog.dll:         wdog.res
comp.dll:         comp.res
adc.dll:          adc.res

dummy168.res:     version.h
timer0_168.res:   version.h timer0_168.h
timer2_168.res:   version.h timer2_168.h
timerN_168.res:   version.h timerN_168.h
wdog.res:         version.h wdog.h
comp.res:         version.h comp.h
eeprom.res:       version.h eeprom.h
adc.res:          version.h adc.h

# DLL files that link multiple .obj files require explicit rules
eeprom.dll:       eeprom.obj hexfile.obj eeprom.res
   ${LD} ${LDFLAGS} ${STARTUP} eeprom.obj hexfile.obj,$@, ,${LIBS}, ,$&.res
   
         
# Suffixes directive needed to make implicit rules work properly
# Autodepend tracks include files in .obj; doesn't work for includes in .rc
.suffixes: .cpp .rc .obj
.autodepend

# Absolute pathnames to the tools; they don't have to be in the path
CC        = ${MAKEDIR}\bcc32
LD        = ${MAKEDIR}\ilink32
RC        = ${MAKEDIR}\brcc32

# Directories, libraries, and startup .obj for linker and compilers
# Based on the command lines used in VMLAB's usercomp.exe tool
INCLUDE   = ${MAKEDIR}\..\include
LIBDIR    = ${MAKEDIR}\..\lib
STARTUP   = ${LIBDIR}\c0d32.obj
LIBS      = ${LIBDIR}\import32.lib ${LIBDIR}\cw32.lib

# Flags passed to C++ compiler, linker, and resource compiler
# Initially based on VMLAB's usercomp.exe tool. OPTFLAGS was added to contain
# all the speed optimizations found in Borland's help file.
OPTFLAGS  = -O -O2 -Oc -Oi -OS -Ov
CPPFLAGS  = -I"${INCLUDE}" -H -w-par -WM- -vi -WD ${OPTFLAGS}
LDFLAGS   = -L"${LIBDIR}; ${LIBDIR}\psdk" -Tpd -aa -x
RFLAGS    = -I"${INCLUDE}"


# Delete all temporary (and final DLL) files created while compiling
clean:
   del *.il? *.obj *.res *.tds
distclean: clean
   del *.dll
   
# Implicit rule for linking .obj and .res into .dll
.obj.dll:
   ${LD} ${LDFLAGS} ${STARTUP} $**,$@, ,${LIBS}, ,$&.res
