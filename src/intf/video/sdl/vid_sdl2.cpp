// blitter effects via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#ifdef INCLUDE_SWITCHRES
#include <switchres_wrapper.h>
#endif

#include <SDL.h>
#include <SDL_image.h>

extern int vsync;
extern char videofiltering[3];
extern int nVidSoftFX;						// SoftFX filter index (VidSoftFXGetEffect), -1 = off

#include "../win32/rgb_pattern.h"			// RGB mask patterns (B, G, R, A bytes), same tables as the D3D blitter

static const struct { const char* szName; int nWidth; int nHeight; const unsigned char* pData; } RGBPatterns[RGB_PATTERN_COUNT] = {
	{ "18x10 large round", 18, 10, pattern_18x10_large_round },
	{ "12x10 large ellipsoid", 12, 10, pattern_12x10_large_ellipsoid },
	{ "10x6 large dot", 10, 6, pattern_10x6_large_dot },
	{ "9x10 ellipsoid", 9, 10, pattern_9x10_ellipsoid },
	{ "8x8 mame rgbtiny", 8, 8, pattern_8x8_mame_rgbtiny },
	{ "6x8 rgb pattern", 6, 8, pattern_6x8_rgb_pattern },
	{ "4x6 rgb pattern", 4, 6, pattern_4x6_rgb_pattern },
	{ "4x4 mame rgbtiny", 4, 4, pattern_4x4_mame_rgbtiny },
	{ "4x4 rgb pattern", 4, 4, pattern_4x4_rgb_pattern },
	{ "3x1 aperture grille", 3, 1, pattern_3x1_aperture_grille },
};

const char* RGBPatternName(int nPattern)
{
	return RGBPatterns[nPattern].szName;
}

static unsigned char* VidMem = NULL;
static bool bOutputEffects = false;				// scanlines or RGB mask applied to the image before upload
static unsigned char* pEffectBuffer = NULL;		// copy of the image the effects are applied to (not used with SoftFX)
static bool bSoftFX = false;					// SoftFX filter active
static int nSoftFXScale = 1;					// Zoom of the SoftFX filter, the texture is nVidImage size * zoom
static int nSoftFXPitch = 0;
static unsigned char* pSoftFXBuffer = NULL;
extern SDL_Window* sdlWindow;
SDL_Renderer* sdlRenderer = NULL;
static SDL_Texture* sdlTexture = NULL;
static int  nRotateGame = 0;
static bool bFlipped = false;
static SDL_Rect dstrect;
static char Windowtitle[512];
static int display_w = 400, display_h = 300;
Uint32 screenFlags;

extern UINT16 maxLinesMenu;	// sdl2_gui_ingame.cpp: number of lines to show in ingame menus
extern bool didReinitialise;

void RenderMessage()
{
	// First render anything that is key-held based
	if (bAppDoFast)
	{
		inprint_shadowed(sdlRenderer, "FFWD", 10, 10);
	}
	if (bAppShowFPS)
	{
		if (bAppFullscreen)
		{
			inprint_shadowed(sdlRenderer, fpsstring, 10, 50);
		} 
		else 
		{
			sprintf(Windowtitle, "FBNeo - FPS: %s - %s - %s", fpsstring, BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
			SDL_SetWindowTitle(sdlWindow, Windowtitle);
		}
	}

	if (messageFrames > 1)
	{
		inprint_shadowed(sdlRenderer, lastMessage, 10, 30);
		messageFrames--;
	}
}

// Multiplies scanlines and the RGB mask into the image. Factors are 0..255, 255 leaves the channel unchanged.
// Pattern bytes are B, G, R (same order as the 32-bit image); the mask is doubled so that its lit subpixels stay bright.
static void ApplyOutputEffects(unsigned char* buf, int width, int height, int pitch)
{
	const bool bPattern = nVidRGBMask >= 1 && nVidRGBMask <= RGB_PATTERN_COUNT;
	const unsigned char* pPattern = bPattern ? RGBPatterns[nVidRGBMask - 1].pData : NULL;
	const int nPatternWidth = bPattern ? RGBPatterns[nVidRGBMask - 1].nWidth : 1;
	const int nPatternHeight = bPattern ? RGBPatterns[nVidRGBMask - 1].nHeight : 1;
	const int scan[3] = { nVidScanIntensity & 0xFF, (nVidScanIntensity >> 8) & 0xFF, (nVidScanIntensity >> 16) & 0xFF };

	for (int y = 0; y < height; y++)
	{
		unsigned char* row = buf + y * pitch;
		const bool bDim = bVidScanlines && (y & 1);

		for (int x = 0; x < width; x++)
		{
			int f[3] = { 255, 255, 255 };			// B, G, R
			if (bDim)
			{
				for (int c = 0; c < 3; c++) f[c] = scan[c];
			}
			if (pPattern)
			{
				const unsigned char* m = pPattern + ((y % nPatternHeight) * nPatternWidth + (x % nPatternWidth)) * 4;
				for (int c = 0; c < 3; c++)
				{
					int v = m[c] * 2;
					f[c] = f[c] * (v > 255 ? 255 : v) / 255;
				}
			}

			if (nVidImageBPP == 4)
			{
				unsigned char* px = row + x * 4;
				for (int c = 0; c < 3; c++) px[c] = px[c] * f[c] / 255;
			}
			else
			{
				// RGB565: R in bits 15..11, G in 10..5, B in 4..0
				unsigned short* px = (unsigned short*)(row + x * 2);
				int r = (*px >> 11) & 31, g = (*px >> 5) & 63, b = *px & 31;
				r = r * f[2] / 255;
				g = g * f[1] / 255;
				b = b * f[0] / 255;
				*px = (unsigned short)((r << 11) | (g << 5) | b);
			}
		}
	}
}

static int Exit()
{
#ifdef INCLUDE_SWITCHRES
	sr_deinit();
#endif
	kill_inline_font(); //TODO: This is not supposed to be here
	SDL_DestroyTexture(sdlTexture);
	sdlTexture = NULL;
	SDL_DestroyRenderer(sdlRenderer);
	sdlRenderer = NULL;
	SDL_DestroyWindow(sdlWindow);
	sdlWindow = NULL;
	
	if (VidMem)
	{
		free(VidMem);
	}

	VidSoftFXExit();
	bSoftFX = false;
	nSoftFXScale = 1;
	if (pSoftFXBuffer)
	{
		free(pSoftFXBuffer);
		pSoftFXBuffer = NULL;
	}
	bOutputEffects = false;
	if (pEffectBuffer)
	{
		free(pEffectBuffer);
		pEffectBuffer = NULL;
	}
	return 0;
}

void AdjustImageSize()
{
	screenFlags = SDL_GetWindowFlags(sdlWindow);

	// Scale governed by -windowscale command line option.
	// Screen center fix thanks to Woises
	if (nRotateGame) {
		if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
			int w;
			int h;
			SDL_GetWindowSize(sdlWindow, &w, &h);
			SDL_SetWindowSize(sdlWindow, (display_w * w / h) * nWindowScale, display_w * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, (display_w * w / h), display_w);
			dstrect.x = ((display_w * w / h) - display_w) / 2;
		} else {
			SDL_RestoreWindow(sdlWindow);		// If started fullscreen, switching to window can get maximized
			SDL_SetWindowSize(sdlWindow, display_h * nWindowScale, display_w * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, display_h, display_w);
			dstrect.x = (display_h - display_w) / 2;
		}
		dstrect.y = (display_w - display_h) / 2;
		maxLinesMenu = display_w / 10 - 6;		// Get number of lines to show in ingame menus
	} else {
		if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
			int w;
			int h;
			SDL_GetWindowSize(sdlWindow, &w, &h);
			SDL_SetWindowSize(sdlWindow, (display_h * w / h) * nWindowScale, display_h * nWindowScale);

			if (display_w < (display_h * w / h)) {
				SDL_RenderSetLogicalSize(sdlRenderer, (display_h * w / h), display_h);
				dstrect.x = ((display_h * w / h) - display_w) / 2;
				dstrect.y = 0;
			} else {
				SDL_RenderSetLogicalSize(sdlRenderer, display_w, (display_w * h / w));
				dstrect.x = 0;
				dstrect.y = ((display_w * h / w) - display_h) / 2;
			}
		} else {
			SDL_RestoreWindow(sdlWindow);
			SDL_SetWindowSize(sdlWindow, display_w * nWindowScale, display_h * nWindowScale);
			SDL_RenderSetLogicalSize(sdlRenderer, display_w, display_h);
			dstrect.x = 0;
			dstrect.y = 0;
		}
		maxLinesMenu = display_h / 10 - 6;		// Get number of lines to show in ingame menus
	}
	SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	dstrect.w = display_w;
	dstrect.h = display_h;
}

static int Init()
{
	int nMemLen = 0;
	int GameAspectX = 4, GameAspectY = 3;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("vid init error\n");
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return 3;
	}

	nRotateGame = 0;

	if (bDrvOkay)
	{
		// Get the game screen size
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
		{
#ifdef FBNEO_DEBUG
			printf("Vertical\n");
#endif
			BurnDrvGetVisibleSize(&nVidImageHeight, &nVidImageWidth);
			BurnDrvGetAspect(&GameAspectY, &GameAspectX);
			nRotateGame = 1;
		}
		else
		{
			BurnDrvGetVisibleSize(&nVidImageWidth, &nVidImageHeight);
			BurnDrvGetAspect(&GameAspectX, &GameAspectY);
		}

#ifdef INCLUDE_SWITCHRES
		// Don't force 4:3 aspect-ratio, until there is a command-line switch
		display_w = nVidImageWidth;
		display_h = nVidImageHeight;
		sr_init();
		if (nRotateGame) sr_set_rotation(1);
#else
		if (nRotateGame) {
			display_w = nVidImageHeight * GameAspectX / GameAspectY;
			display_h = nVidImageHeight;
		} else {
			display_w = nVidImageWidth;
			display_h = nVidImageWidth * GameAspectY / GameAspectX;
		}
#endif

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)
		{
#ifdef FBNEO_DEBUG
			printf("Flipped\n");
#endif
			bFlipped = 1;
		} else {
			bFlipped = 0;
		}
	}

	sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));

	if (bAppFullscreen) screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP;
	else screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	//Test refresh rate availability
#ifdef FBNEO_DEBUG
	printf("Game resolution: %dx%d@%f\n", nVidImageWidth, nVidImageHeight, nBurnFPS/100.0);
#endif

#ifdef INCLUDE_SWITCHRES
	sr_mode srm;
	unsigned char interlace = 0; // FBN doesn't handle interlace yet, force it to disabled
	double rr = nBurnFPS / 100.0;
	sr_init_disp();
	sr_add_mode(display_w, display_h, rr, interlace, &srm);
	sr_switch_to_mode(display_w, display_h, rr, interlace, &srm);
#endif

	if (nRotateGame)
	{
		sdlWindow = SDL_CreateWindow(
			Windowtitle,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_h,
			display_w,
			screenFlags
		);
	}
	else
	{
		sdlWindow = SDL_CreateWindow(
			Windowtitle,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_w,
			display_h,
			screenFlags
		);
	}

	// Check that the window was successfully created
	if (sdlWindow == NULL)
	{
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	Uint32 renderflags = SDL_RENDERER_ACCELERATED;

	if (vsync)
	{
		renderflags = renderflags | SDL_RENDERER_PRESENTVSYNC;
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, renderflags);
	if (sdlRenderer == NULL)
	{
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);
		if (sdlRenderer == NULL)
		{	
			// In the case that the window could not be made...
			printf("Could not create renderer: %s\n", SDL_GetError());
			return 1;
		}
	}

	SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);

	nVidImageDepth = 32;

	if (BurnDrvGetFlags() & BDF_16BIT_ONLY)
	{
		nVidImageDepth = 16;
#ifdef FBNEO_DEBUG
		printf("Forcing 16bit color\n");
#endif
	}

	// Some SoftFX filters only exist for 16-bit (2xPM, 2xSaI, SuperScale...): switch the image to 16-bit for them
	if (nVidSoftFX >= 0 && bDrvOkay && VidSoftFXCheckDepth(nVidSoftFX, nVidImageDepth) == 0 && VidSoftFXCheckDepth(nVidSoftFX, 16) != 0)
	{
		nVidImageDepth = 16;
	}
#ifdef FBNEO_DEBUG
	printf("bbp: %d\n", nVidImageDepth);
#endif
	if (bIntegerScale)
	{
		SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);

	AdjustImageSize();

#ifdef FBNEO_DEBUG
	printf("setting logical size w: %d h: %d\n", display_w, display_h);
#endif
	
	inrenderer(sdlRenderer); //TODO: this is not supposed to be here
	prepare_inline_font();   // TODO: BAD
	incolor(0xFFF000, 0);

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;
#ifdef FBNEO_DEBUG
	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);
#endif
	VidMem = (unsigned char*)malloc(nMemLen);
	if (!VidMem)
	{
		pVidImage = NULL;
		return 1;
	}
	memset(VidMem, 0, nMemLen);
	pVidImage = VidMem;
#ifdef FBNEO_DEBUG
	printf("Malloc for video Ok %d\n", nMemLen);
#endif

	// SoftFX: each frame is filtered from VidMem into pSoftFXBuffer, the texture has the zoomed size
	nSoftFXScale = 1;
	bSoftFX = false;
	if (nVidSoftFX >= 0 && bDrvOkay && VidSoftFXInitSize(nVidSoftFX, nVidImageWidth, nVidImageHeight, nVidImagePitch, VidMem) != 0)
	{
		printf("SoftFX filter %s is not available for this build and colour depth, using plain output\n", VidSoftFXGetEffect(nVidSoftFX));
	}
	else if (nVidSoftFX >= 0 && bDrvOkay)
	{
		nSoftFXScale = VidSoftFXGetZoom(nVidSoftFX);
		nSoftFXPitch = nVidImageWidth * nSoftFXScale * nVidImageBPP;
		pSoftFXBuffer = (unsigned char*)malloc(nSoftFXPitch * nVidImageHeight * nSoftFXScale);
		if (pSoftFXBuffer)
		{
			bSoftFX = true;
			printf("SoftFX filter: %s (x%d)\n", VidSoftFXGetEffect(nVidSoftFX), nSoftFXScale);
		}
		else
		{
			VidSoftFXExit();
			nSoftFXScale = 1;
		}
	}

	bOutputEffects = bDrvOkay && (bVidScanlines || (nVidRGBMask >= 1 && nVidRGBMask <= RGB_PATTERN_COUNT));
	if (bOutputEffects && !bSoftFX)
	{
		pEffectBuffer = (unsigned char*)malloc(nVidImagePitch * nVidImageHeight);
		if (pEffectBuffer == NULL)
		{
			bOutputEffects = false;
		}
	}
	if (bOutputEffects)
	{
		printf("Output effects:%s%s%s\n", bVidScanlines ? " scanlines" : "", nVidRGBMask ? " RGB mask " : "", nVidRGBMask ? RGBPatternName(nVidRGBMask - 1) : "");
	}

	if (nVidImageDepth == 32)
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth * nSoftFXScale, nVidImageHeight * nSoftFXScale);
	}
	else
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB565,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth * nSoftFXScale, nVidImageHeight * nSoftFXScale);
	}
	if (!sdlTexture)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create sdlTexture from surface: %s", SDL_GetError());
		return 3;
	}

	return 0;

#ifdef FBNEO_DEBUG
	printf("done vid init");
#endif
	return 0;
}

static int vidScale(RECT*, int, int)
{
	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)                                          // bRedraw = 0
{
	if ((didReinitialise) && (pVidImage == NULL)) {
		didReinitialise = false;
		Exit();
		Init();
	}
	if (pVidImage == NULL)
	{
		return 1;
	}

	VidFrameCallback(bRedraw);

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{

	unsigned char* pImage = pVidImage;
	int nImagePitch = nVidImagePitch;

	SDL_RenderClear(sdlRenderer);
	if (bSoftFX)
	{
		VidFilterApplyEffect(pSoftFXBuffer, nSoftFXPitch);
		pImage = pSoftFXBuffer;
		nImagePitch = nSoftFXPitch;
	}
	if (bOutputEffects)
	{
		// The burn image is redrawn every frame, so the effects go on a copy (pVidImage stays untouched).
		// SoftFX output is rebuilt every frame too, the effects are applied to it in place.
		if (!bSoftFX)
		{
			for (int y = 0; y < nVidImageHeight; y++)
			{
				memcpy(pEffectBuffer + y * nVidImagePitch, pVidImage + y * nVidImagePitch, nVidImagePitch);
			}
			pImage = pEffectBuffer;
			nImagePitch = nVidImagePitch;
		}
		ApplyOutputEffects(pImage, nVidImageWidth * nSoftFXScale, nVidImageHeight * nSoftFXScale, nImagePitch);
	}
	SDL_UpdateTexture(sdlTexture, NULL, pImage, nImagePitch);
	if (nRotateGame)
	{
		SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, (bFlipped ? 90 : 270), NULL, SDL_FLIP_NONE);
	}
	else
	{
		if (bFlipped)
		{
			SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, 180, NULL, SDL_FLIP_NONE);
		}
		else
		{
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &dstrect);
		}
	}

	RenderMessage();

	SDL_RenderPresent(sdlRenderer);

	return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2 = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL2 video output") };
