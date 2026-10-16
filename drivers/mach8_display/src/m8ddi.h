/* The DDK's GDI and DIB Engine structures, byte-packed as Windows lays them out.
 *
 * Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
 *
 * Without pack(1) Watcom pads FONTINFO's BYTE fields and dfPixHeight reads two
 * bytes late. The checks below fail the build if any offset the driver reads
 * moves. */

#ifndef M8DDI_H
#define M8DDI_H

#include "winhack.h"
#include <stddef.h>
#pragma pack( push, 1 )
#include <gdidefs.h>
#include <dibeng.h>
#pragma pack( pop )

typedef char m8_check_pixheight[ offsetof( FONTINFO, dfPixHeight ) == 22 ? 1 : -1 ];
typedef char m8_check_maxwidth[ offsetof( FONTINFO, dfMaxWidth ) == 27 ? 1 : -1 ];
typedef char m8_check_charextra[ offsetof( DRAWMODE, CharExtra ) == 22 ? 1 : -1 ];
typedef char m8_check_escapement[ offsetof( TEXTXFORM, ftEscapement ) == 4 ? 1 : -1 ];
typedef char m8_check_brushbits[ offsetof( DIB_Brush8, dp8BrushBits ) == 78 ? 1 : -1 ];
typedef char m8_check_dibengine[ sizeof( DIBENGINE ) == 48 ? 1 : -1 ];

#endif
