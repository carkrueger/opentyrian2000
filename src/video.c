/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "video.h"

#include "keyboard.h"
#include "logging.h"
#include "video_scale.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display;
SDL_DisplayMode fullscreen_mode = { 0 };
bool vsync = false;
bool frame_repeat = false;
#ifdef _WIN32
// Direct3D 11 presents with the flip model, which variable refresh rate (FreeSync, G-Sync, Adaptive-Sync)
// needs in windowed and borderless full-screen mode; SDL's default on Windows is Direct3D 9.
char render_driver[32] = "direct3d11";
#else
char render_driver[32] = "";
#endif
ScalingMode scaling_mode = SCALE_INTEGER;
static SDL_Rect last_output_rect = { 0, 0, vga_width, vga_height };

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;
SDL_PixelFormat *main_window_tex_format = NULL;
static SDL_Texture *main_window_texture = NULL;

static ScalerFunction scaler_function;

static void init_renderer(void);
static void deinit_renderer(void);
static void init_texture(void);
static void deinit_texture(void);

static int window_get_display_index(void);
static void window_center_in_display(int display_index);
static void calc_dst_render_rect(SDL_Surface *src_surface, SDL_Rect *dst_rect);
static void scale_and_flip(SDL_Surface *);

static void create_window(const SDL_Point *windowed_position)
{
	// Create the window with a temporary initial size, hidden until we set up the
	// scaler and find the true window size
	main_window = SDL_CreateWindow(opentyrian_str,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

	if (main_window == NULL)
	{
		logFatal("Failed to create window: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	reinit_fullscreen(fullscreen_display);
	init_renderer();
	init_texture();
	init_scaler(scaler);

	if (windowed_position != NULL && fullscreen_display == -1)
		SDL_SetWindowPosition(main_window, windowed_position->x, windowed_position->y);

	SDL_ShowWindow(main_window);

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderPresent(main_window_renderer);
}

void init_video(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
	{
		logFatal("Failed to initialize SDL video: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Create the software surfaces that the game renders to. These are all 320x200x8 regardless
	// of the window size or monitor resolution.
	VGAScreen = VGAScreenSeg = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	VGAScreen2 = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	game_screen = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);

	// The game code writes to surface->pixels directly without locking, so make sure that we
	// indeed created software surfaces that support this.
	assert(!SDL_MUSTLOCK(VGAScreen));
	assert(!SDL_MUSTLOCK(VGAScreen2));
	assert(!SDL_MUSTLOCK(game_screen));

	JE_clr256(VGAScreen);

	create_window(NULL);
}

void deinit_video(void)
{
	deinit_texture();
	deinit_renderer();

	SDL_DestroyWindow(main_window);

	SDL_FreeSurface(VGAScreenSeg);
	SDL_FreeSurface(VGAScreen2);
	SDL_FreeSurface(game_screen);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void init_renderer(void)
{
	const Uint32 flags = vsync ? SDL_RENDERER_PRESENTVSYNC : 0;

	int index = -1;
	for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i)
	{
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(i, &info) == 0 && strcmp(info.name, render_driver) == 0)
			index = i;
	}

	if (index != -1)
	{
		main_window_renderer = SDL_CreateRenderer(main_window, index, flags);
		if (main_window_renderer == NULL)
			logWarn("Failed to create '%s' renderer: %s", render_driver, SDL_GetError());
	}
	else if (render_driver[0] != '\0')
	{
		logWarn("Renderer '%s' is not available.", render_driver);
	}

	if (main_window_renderer == NULL)
		main_window_renderer = SDL_CreateRenderer(main_window, -1, flags);

	if (main_window_renderer == NULL)
	{
		logFatal("Failed to create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_RendererInfo info;
	if (SDL_GetRendererInfo(main_window_renderer, &info) == 0)
		logInfo("Renderer: %s%s", info.name, (info.flags & SDL_RENDERER_PRESENTVSYNC) ? ", vsync" : "");
}

static void deinit_renderer(void)
{
	if (main_window_renderer != NULL)
	{
		SDL_DestroyRenderer(main_window_renderer);
		main_window_renderer = NULL;
	}
}

static void init_texture(void)
{
	assert(main_window_renderer != NULL);

	int bpp = 32; // TODOSDL2
	Uint32 format = bpp == 32 ? SDL_PIXELFORMAT_RGB888 : SDL_PIXELFORMAT_RGB565;
	int scaler_w = scalers[scaler].width;
	int scaler_h = scalers[scaler].height;

	main_window_tex_format = SDL_AllocFormat(format);

	main_window_texture = SDL_CreateTexture(main_window_renderer, format, SDL_TEXTUREACCESS_STREAMING, scaler_w, scaler_h);

	if (main_window_texture == NULL)
	{
		logFatal("Failed to create scaler texture (%dx%dx%s): %s", scaler_w, scaler_h, SDL_GetPixelFormatName(format), SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_texture(void)
{
	if (main_window_texture != NULL)
	{
		SDL_DestroyTexture(main_window_texture);
		main_window_texture = NULL;
	}

	if (main_window_tex_format != NULL)
	{
		SDL_FreeFormat(main_window_tex_format);
		main_window_tex_format = NULL;
	}
}

static int window_get_display_index(void)
{
	return SDL_GetWindowDisplayIndex(main_window);
}

static void window_center_in_display(int display_index)
{
	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	SDL_Rect bounds;
	SDL_GetDisplayBounds(display_index, &bounds);

	SDL_SetWindowPosition(main_window, bounds.x + (bounds.w - win_w) / 2, bounds.y + (bounds.h - win_h) / 2);
}

void reinit_fullscreen(int new_display)
{
	fullscreen_display = new_display;

	if (fullscreen_display >= SDL_GetNumVideoDisplays())
	{
		fullscreen_display = 0;
	}

	SDL_SetWindowFullscreen(main_window, SDL_FALSE);
	SDL_SetWindowSize(main_window, scalers[scaler].width, scalers[scaler].height);

	if (fullscreen_display == -1)
	{
		window_center_in_display(window_get_display_index());
	}
	else
	{
		window_center_in_display(fullscreen_display);

		if (fullscreen_mode.w != 0)
		{
			// Exclusive full-screen in the closest mode the display has (for example, to get a refresh rate
			// that matches the game's frame rate).
			SDL_DisplayMode mode;
			if (SDL_GetClosestDisplayMode(fullscreen_display, &fullscreen_mode, &mode) != NULL &&
			    SDL_SetWindowDisplayMode(main_window, &mode) == 0 &&
			    SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN) == 0)
			{
				logInfo("Full-screen mode: %dx%d @ %d Hz", mode.w, mode.h, mode.refresh_rate);
				return;
			}

			logWarn("Failed to set full-screen mode %dx%d @ %d Hz: %s",
			        fullscreen_mode.w, fullscreen_mode.h, fullscreen_mode.refresh_rate, SDL_GetError());
		}

		if (SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
		{
			reinit_fullscreen(-1);
			return;
		}
	}
}

const char *fullscreen_mode_to_name(const SDL_DisplayMode *mode, char *buffer, size_t buffer_size)
{
	if (mode->w == 0)
		return "Desktop";

	snprintf(buffer, buffer_size, "%dx%d@%d", mode->w, mode->h, mode->refresh_rate);
	return buffer;
}

bool set_fullscreen_mode_by_name(const char *name)
{
	SDL_DisplayMode mode = { 0 };
	if (strcmp(name, "Desktop") != 0 &&
	    (sscanf(name, "%dx%d@%d", &mode.w, &mode.h, &mode.refresh_rate) != 3 ||
	     mode.w <= 0 || mode.h <= 0 || mode.refresh_rate < 0))
		return false;

	fullscreen_mode = mode;
	return true;
}

void change_fullscreen_mode(const SDL_DisplayMode *mode)
{
	fullscreen_mode = *mode;

	if (fullscreen_display != -1)
		reinit_fullscreen(fullscreen_display);
}

static bool is_fullscreen_mode_choice(const SDL_DisplayMode *mode, const SDL_DisplayMode *desktop)
{
	// The desktop's resolution at every refresh rate, and any resolution at about 70 Hz or a multiple of
	// it, the refresh rate of the VGA mode Tyrian was made for.
	if (mode->w == desktop->w && mode->h == desktop->h)
		return true;
	for (int hz = 70; hz <= 280; hz += 70)
		if (mode->refresh_rate >= hz - 1 && mode->refresh_rate <= hz + 1)
			return true;
	return false;
}

/** Lists the full-screen modes offered in the setup menu; the first is always the desktop's mode. */
size_t get_fullscreen_mode_choices(SDL_DisplayMode *modes, size_t modes_max)
{
	size_t count = 0;
	if (count < modes_max)
		modes[count++] = (SDL_DisplayMode){ 0 };

	int display = fullscreen_display != -1 ? fullscreen_display : window_get_display_index();
	SDL_DisplayMode desktop;
	if (SDL_GetDesktopDisplayMode(display, &desktop) != 0)
		return count;

	for (int i = 0; i < SDL_GetNumDisplayModes(display) && count < modes_max; ++i)
	{
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(display, i, &mode) != 0 || !is_fullscreen_mode_choice(&mode, &desktop))
			continue;

		// Modes are listed per pixel format too; offer each resolution and refresh rate once.
		bool duplicate = false;
		for (size_t j = 1; j < count; ++j)
			if (modes[j].w == mode.w && modes[j].h == mode.h && modes[j].refresh_rate == mode.refresh_rate)
				duplicate = true;
		if (!duplicate)
			modes[count++] = (SDL_DisplayMode){ .w = mode.w, .h = mode.h, .refresh_rate = mode.refresh_rate };
	}

	return count;
}

/**
 * Whether frames are to be presented again at the given rate (in mHz).  With VSync on a display whose
 * refresh rate is lower than that, presenting would have to wait for the display, which could delay the
 * game's own frames, so frames are not repeated then.
 */
bool can_repeat_frames(Uint32 rate_mhz)
{
	if (!frame_repeat || main_window_texture == NULL)
		return false;

	if (!vsync)
		return true;

	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(main_window), &mode) != 0 || mode.refresh_rate == 0)
		return false;

	return (Uint32)mode.refresh_rate * 1000 >= rate_mhz;
}

// When the last frame was presented, in milliseconds as fixed point UQ22.10 (like the timer in nortsong.c).
static Uint32 last_present_time = 0;

Uint32 get_last_present_time(void)
{
	return last_present_time;
}

/** Presents the last frame again (see frame_repeat). */
void present_again(void)
{
	last_present_time = SDL_GetTicks() << 10;

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderCopy(main_window_renderer, main_window_texture, NULL, &last_output_rect);
	SDL_RenderPresent(main_window_renderer);
}

void change_vsync(bool enable)
{
	vsync = enable;

	if (SDL_RenderSetVSync(main_window_renderer, vsync ? 1 : 0) != 0)
		logWarn("Failed to change vsync: %s", SDL_GetError());
}

void change_render_driver(const char *name)
{
	SDL_strlcpy(render_driver, name, sizeof render_driver);

	SDL_Point position;
	SDL_GetWindowPosition(main_window, &position.x, &position.y);

	// The window is created again too: a window that has been presented to with the flip model (Direct3D
	// 11 and 12) does not show what is presented with the older blit model (Direct3D 9) anymore.
	deinit_texture();
	deinit_renderer();
	SDL_DestroyWindow(main_window);

	create_window(&position);
}

void video_on_win_resize(void)
{
	int w, h;
	int scaler_w, scaler_h;

	// Tell video to reinit if the window was manually resized by the user.
	// Also enforce a minimum size on the window.

	SDL_GetWindowSize(main_window, &w, &h);
	scaler_w = scalers[scaler].width;
	scaler_h = scalers[scaler].height;

	if (w < scaler_w || h < scaler_h)
	{
		w = w < scaler_w ? scaler_w : w;
		h = h < scaler_h ? scaler_h : h;

		SDL_SetWindowSize(main_window, w, h);
	}
}

void toggle_fullscreen(void)
{
	if (fullscreen_display != -1)
		reinit_fullscreen(-1);
	else
		reinit_fullscreen(SDL_GetWindowDisplayIndex(main_window));
}

bool init_scaler(unsigned int new_scaler)
{
	int w = scalers[new_scaler].width,
	    h = scalers[new_scaler].height;
	int bpp = main_window_tex_format->BitsPerPixel; // TODOSDL2

	scaler = new_scaler;

	deinit_texture();
	init_texture();

	if (fullscreen_display == -1)
	{
		// Changing scalers, when not in fullscreen mode, forces the window
		// to resize to exactly match the scaler's output dimensions.
		SDL_SetWindowSize(main_window, w, h);
		window_center_in_display(window_get_display_index());
	}

	switch (bpp)
	{
	case 32:
		scaler_function = scalers[scaler].scaler32;
		break;
	case 16:
		scaler_function = scalers[scaler].scaler16;
		break;
	default:
		scaler_function = NULL;
		break;
	}

	if (scaler_function == NULL)
	{
		assert(false);
		return false;
	}

	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		 if (strcmp(name, scaling_mode_names[i]) == 0)
		 {
			 scaling_mode = i;
			 return true;
		 }
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
	SDL_FillRect(screen, NULL, 0);
}

void JE_showVGA(void)
{
	scale_and_flip(VGAScreen);
}

static void calc_dst_render_rect(SDL_Surface *const src_surface, SDL_Rect *const dst_rect)
{
	// Decides how the logical output texture (after software scaling applied) will fit
	// in the window.

	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	int maxh_width, maxw_height;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
		SDL_QueryTexture(main_window_texture, NULL, NULL, &dst_rect->w, &dst_rect->h);
		break;
	case SCALE_INTEGER:
		dst_rect->w = src_surface->w;
		dst_rect->h = src_surface->h;
		while (dst_rect->w + src_surface->w <= win_w && dst_rect->h + src_surface->h <= win_h)
		{
			dst_rect->w += src_surface->w;
			dst_rect->h += src_surface->h;
		}
		break;
	case SCALE_ASPECT_8_5:
		maxh_width = win_h * (8.f / 5.f);
		maxw_height = win_w * (5.f / 8.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case SCALE_ASPECT_4_3:
		maxh_width = win_h * (4.f / 3.f);
		maxw_height = win_w * (3.f / 4.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case ScalingMode_MAX:
		assert(false);
		break;
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
}

static void scale_and_flip(SDL_Surface *src_surface)
{
	assert(src_surface->format->BitsPerPixel == 8);

	// Do software scaling
	assert(scaler_function != NULL);
	scaler_function(src_surface, main_window_texture);

	SDL_Rect dst_rect;
	calc_dst_render_rect(src_surface, &dst_rect);

	last_present_time = SDL_GetTicks() << 10;

	// Clear the window and blit the output texture to it
	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderCopy(main_window_renderer, main_window_texture, NULL, &dst_rect);
	SDL_RenderPresent(main_window_renderer);

	// Save output rect to be used by mouse functions
	last_output_rect = dst_rect;
}

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * last_output_rect.w / (2 * VGAScreen->w) + last_output_rect.x;
	*inout_y = (2 * *inout_y + 1) * last_output_rect.h / (2 * VGAScreen->h) + last_output_rect.y;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * (*inout_x - last_output_rect.x) + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * (*inout_y - last_output_rect.y) + 1) * VGAScreen->h / (2 * last_output_rect.h);
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * *inout_y + 1) * VGAScreen->h / (2 * last_output_rect.h);
}
