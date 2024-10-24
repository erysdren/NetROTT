/*
MIT License

Copyright (c) 2024 erysdren (it/she/they)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <SDL3/SDL.h>

#include "vgafont.h"

#define BLINK_HZ ((1000 / 70) * 16)

// ega 16-color palette
static const Uint8 palette[16][3] = {
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0xab}, {0x00, 0xab, 0x00}, {0x00, 0xab, 0xab},
	{0xab, 0x00, 0x00}, {0xab, 0x00, 0xab}, {0xab, 0x57, 0x00}, {0xab, 0xab, 0xab},
	{0x57, 0x57, 0x57}, {0x57, 0x57, 0xff}, {0x57, 0xff, 0x57}, {0x57, 0xff, 0xff},
	{0xff, 0x57, 0x57}, {0xff, 0x57, 0xff}, {0xff, 0xff, 0x57}, {0xff, 0xff, 0xff}
};

static void render_cell(Uint8 *image, int pitch, Uint16 cell, bool noblink)
{
	// get components
	Uint8 code = (Uint8)(cell & 0xFF);
	Uint8 attr = (Uint8)(cell >> 8);

	// break out attributes
	Uint8 blink = (attr >> 7) & 0x01;
	Uint8 bgcolor = (attr >> 4) & 0x07;
	Uint8 fgcolor = attr & 0x0F;

	for (int y = 0; y < 16; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			// write bgcolor
			image[y * pitch + x] = bgcolor;

			if (blink && noblink)
				continue;

			// write fgcolor
			Uint8 *bitmap = &VGA_FONT_CP437[code * 16];

			if (bitmap[y] & 1 << SDL_abs(x - 7))
				image[y * pitch + x] = fgcolor;
		}
	}
}

int vgatext_main(SDL_Window *window, Uint16 *screen)
{
	Uint8 image0[400][640];
	Uint8 image1[400][640];
	Uint32 format;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Surface *surface8;
	SDL_Surface **windowsurface;
	SDL_Surface *windowsurface0;
	SDL_Surface *windowsurface1;
	SDL_Color colors[16];
	SDL_Rect rect;
	SDL_Palette *pal;

	if (!window || !screen)
		return -1;

	// setup renderer
	renderer = SDL_GetRenderer(window);
	if (!renderer)
		return -1;

	SDL_SetRenderLogicalPresentation(renderer, 640, 400, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_SetWindowMinimumSize(window, 640, 400);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	// setup render surface
	surface8 = SDL_CreateSurface(640, 400, SDL_PIXELFORMAT_INDEX8);
	if (!surface8)
		return -1;

	for (int i = 0; i < 16; i++)
	{
		colors[i].r = palette[i][0];
		colors[i].g = palette[i][1];
		colors[i].b = palette[i][2];
	}

	pal = SDL_CreateSurfacePalette(surface8);

	SDL_SetPaletteColors(pal, colors, 0, 16);
	SDL_FillSurfaceRect(surface8, NULL, 0);

	// setup display surfaces
	format = SDL_GetWindowPixelFormat(window);
	windowsurface0 = SDL_CreateSurface(640, 400, format);
	windowsurface1 = SDL_CreateSurface(640, 400, format);
	if (!windowsurface0 || !windowsurface1)
		return -1;

	// setup render texture
	texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, 640, 400);
	if (!texture)
		return -1;
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

	// setup blit rect
	rect.x = 0;
	rect.y = 0;
	rect.w = 640;
	rect.h = 400;

	// render vgatext
	for (int y = 0; y < 25; y++)
	{
		for (int x = 0; x < 80; x++)
		{
			uint16_t cell = screen[y * 80 + x];
			uint8_t *imgpos0 = &image0[y * 16][x * 8];
			uint8_t *imgpos1 = &image1[y * 16][x * 8];

			render_cell(imgpos0, 640, cell, false);
			render_cell(imgpos1, 640, cell, true);
		}
	}

	// create rgb image0
	for (int y = 0; y < 400; y++)
		SDL_memcpy(&((Uint8 *)surface8->pixels)[y * surface8->pitch], &image0[y][0], 640);
	SDL_BlitSurface(surface8, &rect, windowsurface0, &rect);

	// create rgb image1
	for (int y = 0; y < 400; y++)
		SDL_memcpy(&((Uint8 *)surface8->pixels)[y * surface8->pitch], &image1[y][0], 640);
	SDL_BlitSurface(surface8, &rect, windowsurface1, &rect);

	// save windowsurface ptr
	windowsurface = &windowsurface0;

	// start counting time
	Uint64 next = SDL_GetTicks() + BLINK_HZ;

	// main loop
	while (true)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				goto done;
		}

		Uint64 now = SDL_GetTicks();
		if (next <= now)
		{
			if (windowsurface == &windowsurface0)
				windowsurface = &windowsurface1;
			else
				windowsurface = &windowsurface0;

			next += BLINK_HZ;
		}

		SDL_UpdateTexture(texture, NULL, (*windowsurface)->pixels, (*windowsurface)->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderTexture(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	}

done:

	SDL_DestroySurface(surface8);
	SDL_DestroySurface(windowsurface0);
	SDL_DestroySurface(windowsurface1);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);

	return 0;
}
