#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Define version information
m4_define([libtgvoip_major_version], 2)
m4_define([libtgvoip_minor_version], 4)
m4_define([libtgvoip_patch_version], 4)
m4_define([libtgvoip_version],[libtgvoip_major_version.libtgvoip_minor_version.libtgvoip_patch_version])

AC_PREREQ([2.69])
AC_INIT([libtgvoip], [libtgvoip_version], [https://github.com/grishka/libtgvoip/issues])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT

PKG_INSTALLDIR

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_OBJCXX
AM_PROG_AS
AC_PROG_RANLIB

# Checks for libraries.
PKG_CHECK_MODULES(CRYPTO, [libcrypto])
AC_CHECK_LIB([m], [floorf])
PKG_CHECK_MODULES(OPUS, [opus])
AC_CHECK_LIB([pthread], [pthread_create])

AC_CANONICAL_HOST

AC_SUBST(LIBTGVOIP_MAJOR_VERSION, libtgvoip_major_version)
AC_SUBST(LIBTGVOIP_MINOR_VERSION, libtgvoip_minor_version)
AC_SUBST(LIBTGVOIP_PATCH_VERSION, libtgvoip_patch_version)
AC_SUBST(LIBTGVOIP_VERSION, libtgvoip_version)

AS_CASE([$host_cpu],
	[i?86], [cpu_x86=yes],
	[x86_64], [cpu_x86=yes],
	[arm*], [cpu_arm=yes],
	[AS_ECHO("!! WARNING: libtgvoip wasn't tested with your CPU architecture ($host_cpu)")]
)
AS_CASE([$host_cpu],
	[armv7*], [cpu_armv7=yes]
)
AS_ECHO("Detected CPU: $host_cpu")
AM_CONDITIONAL(TARGET_CPU_X86, test "x$cpu_x86" = xyes)
AM_CONDITIONAL(TARGET_CPU_ARM, test "x$cpu_arm" = xyes)
AM_CONDITIONAL(TARGET_CPU_ARMV7, test "x$cpu_armv7" = xyes)
AS_ECHO("Detected OS: $host_os")

AS_CASE([$host_os],
	[darwin*], [os_osx=yes]
)
AM_CONDITIONAL(TARGET_OS_OSX, test "x$os_osx" = xyes)

AC_ARG_ENABLE([audio-callback], [AS_HELP_STRING([--enable-audio-callback], [enable callback-based audio I/O])], [], [enable_audio_callback=no])
AM_CONDITIONAL(ENABLE_AUDIO_CALLBACK, test "x$enable_audio_callback" = xyes)

AS_IF([test "x$os_osx" != xyes && test "x$enable_audio_callback" != xyes], [ # Linux
	AC_CHECK_LIB([dl], [dlopen])

	AC_ARG_WITH([pulse], [AS_HELP_STRING([--without-pulse], [disable PulseAudio support])], [], [with_pulse=yes])
	AC_ARG_WITH([alsa], [AS_HELP_STRING([--without-alsa], [disable ALSA support])], [], [with_alsa=yes])

	AS_IF([test "x$with_pulse" = xno && test "x$with_alsa" = xno], [
		AC_MSG_FAILURE([You can only disable either ALSA or PulseAudio, not both]);
	])

	AS_IF([test "x$with_pulse" != xno], [
		PKG_CHECK_MODULES(PULSE, [libpulse])
	], [
		AC_DEFINE([WITHOUT_PULSE], [1], [Define to disable PulseAudio support])
	])

	AS_IF([test "x$with_alsa" != xno], [
		PKG_CHECK_MODULES(ALSA, [alsa])
	], [
		AC_DEFINE([WITHOUT_ALSA], [1], [Define to disable ALSA support])
	])
]);

AM_CONDITIONAL(WITH_PULSE, test "x$with_pulse" = xyes)
AM_CONDITIONAL(WITH_ALSA, test "x$with_alsa" = xyes)

AC_ARG_ENABLE([dsp], [AS_HELP_STRING([--disable-dsp], [disable signal processing (echo cancellation, noise suppression, and automatic gain control)])], [], [enable_dsp=yes])
AM_CONDITIONAL(ENABLE_DSP, test "x$enable_dsp" = xyes)

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h float.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime floor gettimeofday inet_ntoa memmove memset select socket sqrt strcasecmp strchr strerror strncasecmp strstr strtol strtoul uname])

AC_CONFIG_FILES([Makefile tgvoip.pc])
AC_OUTPUT
