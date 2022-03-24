#pragma once

#ifdef __MINGW32__
#ifndef PKEY_Device_FriendlyName
#ifdef DEFINE_PROPERTYKEY
#undef DEFINE_PROPERTYKEY
#endif

/* clang-format off */

#define DEFINE_PROPERTYKEY(id, a, b, c, d, e, f, g, h, i, j, k, l) \
	const PROPERTYKEY id = { { a,b,c, { d,e,f,g,h,i,j,k, } }, l };
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, \
	0xa45c254e, 0xdf1c, 0x4efd, 0x80, \
	0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

/* clang-format on */

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM  0x80000000
#endif

#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY  0x08000000
#endif
#endif
