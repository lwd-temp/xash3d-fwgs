/*
Copyright (C) 2021 a1batross

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"

#define RIPPLES_CACHEWIDTH_BITS 7
//#define RIPPLES_CACHEWIDTH_BITS 6
#define RIPPLES_CACHEWIDTH (1<<RIPPLES_CACHEWIDTH_BITS)
#define RIPPLES_CACHEWIDTH_MASK ((RIPPLES_CACHEWIDTH)-1)
#define RIPPLES_TEXSIZE ( RIPPLES_CACHEWIDTH * RIPPLES_CACHEWIDTH )
#define RIPPLES_TEXSIZE_MASK (RIPPLES_TEXSIZE-1)

static struct
{
	float time;
	float oldtime;
	short buf1[RIPPLES_TEXSIZE];
	short buf2[RIPPLES_TEXSIZE];
	short *pbuf1, *pbuf2;

	pixel_t cacheblock[RIPPLES_TEXSIZE];
} g_ripples_state =
{
	0.0f, 0.0f,
	{ 0 }, { 0 },
	g_ripples_state.buf1, g_ripples_state.buf2,
	{ 0 }
};

static float R_InitRipplesTime( void )
{
	g_ripples_state.time = g_ripples_state.oldtime = gpGlobals->time - 0.1f;
	return 0.1f;
}

static void R_SwapRipplesBuffers( void )
{
	short *temp;

	temp = g_ripples_state.pbuf1;
	g_ripples_state.pbuf1 = g_ripples_state.pbuf2;
	g_ripples_state.pbuf2 = temp;
}

static void R_SpawnRipple( int x, int y, int val )
{
#if RIPPLES_CACHEWIDTH == 128
#define PIXEL( x, y ) ( ((x) & 0x7f) + (((y) & 0x7f) << RIPPLES_CACHEWIDTH_BITS) )
#elif RIPPLES_CACHEWIDTH == 64
#define PIXEL( x, y ) ( ((x) & 0x3f) + (((y) & 0x3f) << RIPPLES_CACHEWIDTH_BITS) )
#endif
	short sval = (short)val - (short)(val >> 2);

	g_ripples_state.pbuf1[PIXEL( x, y )] = (short)val;
	g_ripples_state.pbuf1[PIXEL( x + 1, y )] = sval;
	g_ripples_state.pbuf1[PIXEL( x - 1, y )] = sval;
	g_ripples_state.pbuf1[PIXEL( x, y + 1 )] = sval;
	g_ripples_state.pbuf1[PIXEL( x, y - 1 )] = sval;

#undef PIXEL
}

static void R_RippleAnimation( void )
{
	int i;
	short *pbuf1 = g_ripples_state.pbuf1;
	short *pbuf2 = g_ripples_state.pbuf2;

	for( i = RIPPLES_CACHEWIDTH;
	     i - (RIPPLES_CACHEWIDTH-1) < RIPPLES_TEXSIZE;
	     i++, pbuf2++ )
	{
		int val = ((((int)pbuf1[(i - (RIPPLES_CACHEWIDTH*2)) & RIPPLES_TEXSIZE_MASK] +
				(int)pbuf1[(i - (RIPPLES_CACHEWIDTH+1)) & RIPPLES_TEXSIZE_MASK] +
				(int)pbuf1[(i - (RIPPLES_CACHEWIDTH-1)) & RIPPLES_TEXSIZE_MASK]) +
				(int)pbuf1[(i) & RIPPLES_TEXSIZE_MASK]) >> 1 ) - (int)*pbuf2;

		*pbuf2 = (short)val - (short)(val >> 6);
	}
}

static int MostSignificantBit( unsigned int v )
{
#if __GNUC__
	return 31 - __builtin_clz( v );
#else
	int i;
	for( i = 0, v >>= 1; v; v >>= 1, i++ );
	return i;
#endif
}

pixel_t *R_Ripples( float step, texture_t *tex, int *cachewidth )
{
	float time;

	time = gpGlobals->time - g_ripples_state.time;
	if( time >= 0.0f )
	{
		if( time < 0.05f )
		{
			time = 0.0f;
		}
	}
	else time = R_InitRipplesTime();

	g_ripples_state.time += time;

	if( time > 0.0f )
	{
		byte v = MostSignificantBit( tex->width );
		image_t *image = R_GetTexture( tex->gl_texturenum );
		int i;

		R_SwapRipplesBuffers();

		if( g_ripples_state.time - g_ripples_state.oldtime > step )
		{
			int x, y, val;

			g_ripples_state.oldtime = g_ripples_state.time;

			x = gEngfuncs.COM_RandomLong( 0, 0x7fff );
			y = gEngfuncs.COM_RandomLong( 0, 0x7fff );
			val = gEngfuncs.COM_RandomLong( 0, 0x3ff );

			R_SpawnRipple( x, y, val );
		}

		R_RippleAnimation();

		for( i = 0; i < RIPPLES_TEXSIZE; i++ )
		{
			int val = g_ripples_state.pbuf2[i];

			g_ripples_state.cacheblock[i] = image->pixels[0][
				((i + (val >> 4)) & (image->width - 1)) +
				((((i >> RIPPLES_CACHEWIDTH_BITS) - (val >> 4)) & (image->height - 1)) << v)
			];
		}
	}

	*cachewidth = RIPPLES_CACHEWIDTH;
	return g_ripples_state.cacheblock;
}
