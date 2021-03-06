/*
 * $Id: screen.c,v 1.3 2003/02/09 07:34:16 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * SDL screen handler.
 *
 * @version $Revision: 1.3 $
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "noiz2sa.h"
#include "screen.h"
#include "clrtbl.h"
#include "vector.h"
#include "degutil.h"
#include "letterrender.h"
#include "attractmanager.h"

int windowMode = 0;
int brightness = DEFAULT_BRIGHTNESS;

static SDL_Rect windowRect;
static SDL_Window* window;
static SDL_Renderer* renderer;

static SDL_Surface *video, *layer, *lpanel, *rpanel;
static LayerBit **smokeBuf;
static LayerBit *pbuf;
LayerBit *l1buf, *l2buf;
LayerBit *buf;
LayerBit *lpbuf, *rpbuf;
static SDL_Rect layerRect, layerClearRect;
static SDL_Rect lpanelRect, rpanelRect;
static int pitch, ppitch;

// Handle BMP images.
#define SPRITE_NUM 7

static SDL_Surface *sprite[SPRITE_NUM];
static char *spriteFile[SPRITE_NUM] = {
  "title_n.bmp", "title_o.bmp", "title_i.bmp", "title_z.bmp", "title_2.bmp",
  "title_s.bmp", "title_a.bmp",
};

const unsigned char* keys;
SDL_Joystick *stick = NULL;

static void loadSprites() {
  SDL_Surface *img;
  int i;
  char name[32];
  color[0].r = 100; color[0].g = 0; color[0].b = 0;
  SDL_SetPaletteColors(video->format->palette, color, 0, 1);
  for ( i=0 ; i<SPRITE_NUM ; i++ ) {
    strcpy(name, "images/");
    strcat(name, spriteFile[i]);
    img = SDL_LoadBMP(name);
    if ( img == NULL ) {
      fprintf(stderr, "Unable to load: %s\n", name);
      SDL_Quit();
      exit(1);
    }
    sprite[i] = SDL_ConvertSurface(img, video->format, 0);
    //SDL_SetColorKey(sprite[i], SDL_RLEACCEL, 0);
    fprintf(stdout, "Sprite loaded. : %s\n", name);
  }
  //color[0].r = color[0].g = color[0].b = 255;
  //SDL_SetPaletteColors(video->format->palette, color, 0, 1);
  fprintf(stdout, "All sprites loaded\n");
}


void drawSprite(int n, int x, int y) {
  SDL_Rect pos;
  pos.x = x; pos.y = y;
  pos.w = sprite[n]->w;
  pos.h = sprite[n]->h;
  SDL_BlitSurface(sprite[n], NULL, video, &pos);
}

// Initialize palletes.
static void initPalette() {
  int i;
  for ( i=0 ; i<256 ; i++ ) {
    color[i].r = color[i].r*brightness/256;
    color[i].g = color[i].g*brightness/256;
    color[i].b = color[i].b*brightness/256;
  }
  SDL_SetPaletteColors(video->format->palette, color, 0, 256);
  SDL_SetPaletteColors(layer->format->palette, color, 0, 256);
  SDL_SetPaletteColors(lpanel->format->palette, color, 0, 256);
  SDL_SetPaletteColors(rpanel->format->palette, color, 0, 256);
}

static int lyrSize;

static void makeSmokeBuf() {
  int x, y, mx, my;
  lyrSize = sizeof(LayerBit)*pitch*LAYER_HEIGHT;
  if ( NULL == (smokeBuf = (LayerBit**)malloc(sizeof(LayerBit*)*pitch*LAYER_HEIGHT)) ) {
    fprintf(stderr, "Couldn't malloc smokeBuf.");
    exit(1);
  }
  if ( NULL == (pbuf  = (LayerBit*)malloc(lyrSize+sizeof(LayerBit))) ||
       NULL == (l1buf = (LayerBit*)malloc(lyrSize+sizeof(LayerBit))) ||
       NULL == (l2buf = (LayerBit*)malloc(lyrSize+sizeof(LayerBit))) ) {
    fprintf(stderr, "Couldn't malloc buffer.");
    exit(1);
  }
  pbuf[pitch*LAYER_HEIGHT] = 0;
  for ( y=0 ; y<LAYER_HEIGHT ; y++ ) {
    for ( x=0 ; x<LAYER_WIDTH ; x++ ) {
      mx = x + sctbl[(x*8)&(DIV-1)]/128;
      my = y + sctbl[(y*8)&(DIV-1)]/128;
      if ( mx < 0 || mx >= LAYER_WIDTH || my < 0 || my >= LAYER_HEIGHT ) {
        smokeBuf[x+y*pitch] = &(pbuf[pitch*LAYER_HEIGHT]);
      } else {
        smokeBuf[x+y*pitch] = &(pbuf[mx+my*pitch]);
      }
    }
  }
}

void initSDL(int const isWindow, int const display) {
  if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0 ) {
    fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
    exit(1);
  }

  const Uint8 videoBpp = BPP;

  if (display < 0 || display >= SDL_GetNumVideoDisplays()) {
    fprintf(stderr, "Unable to specify display: there are just %d displays\n", SDL_GetNumVideoDisplays());
    SDL_Quit();
    exit(1);
  }

  if (isWindow) {
    window = SDL_CreateWindow(
      CAPTION,
      SDL_WINDOWPOS_CENTERED_DISPLAY(display),
      SDL_WINDOWPOS_CENTERED_DISPLAY(display),
      SCREEN_WIDTH,
      SCREEN_HEIGHT,
      0);
  } else {
    window = SDL_CreateWindow(
      CAPTION,
      SDL_WINDOWPOS_CENTERED_DISPLAY(display),
      SDL_WINDOWPOS_CENTERED_DISPLAY(display),
      SCREEN_WIDTH,
      SCREEN_HEIGHT,
      SDL_WINDOW_FULLSCREEN_DESKTOP);
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  }
  if (window == NULL) {
    fprintf(stderr, "Unable to create SDL window: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  if ( NULL == (renderer = SDL_CreateRenderer(window, -1, 0))) {
    fprintf(stderr, "Unable to create SDL renderer: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  if ( NULL == (video = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, videoBpp, 0, 0, 0, 0)) ) {
    fprintf(stderr, "Unable to create SDL screen: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  int sw,sh;
  SDL_GetWindowSize(window, &sw, &sh);
  const double scale = fmin((double) sw / SCREEN_WIDTH, (double) sh / SCREEN_HEIGHT);
  windowRect.w = (int) (SCREEN_WIDTH * scale);
  windowRect.h = (int) (SCREEN_HEIGHT * scale);
  windowRect.x = (sw - windowRect.w) / 2;
  windowRect.y = (sh - windowRect.h) / 2;

	SDL_PixelFormat* const pfrm = video->format;
	if ( NULL == (layer = SDL_CreateRGBSurface(0, LAYER_WIDTH, LAYER_HEIGHT,videoBpp, 0, 0, 0, 0))
	  || NULL == (lpanel = SDL_CreateRGBSurface(0, PANEL_WIDTH, PANEL_HEIGHT, videoBpp, 0, 0, 0, 0))
	  || NULL == (rpanel = SDL_CreateRGBSurface(0, PANEL_WIDTH, PANEL_HEIGHT, videoBpp, 0, 0, 0, 0))) {
      fprintf(stderr, "Couldn't create surface: %s\n", SDL_GetError());
		exit(1);
	}
  layerRect.x = (SCREEN_WIDTH-LAYER_WIDTH)/2;
  layerRect.y = (SCREEN_HEIGHT-LAYER_HEIGHT)/2;
  layerRect.w = LAYER_WIDTH;
  layerRect.h = LAYER_HEIGHT;
  lpanelRect.x = 0;
  lpanelRect.y = (SCREEN_HEIGHT-PANEL_HEIGHT)/2;
  rpanelRect.x = SCREEN_WIDTH-PANEL_WIDTH;
  rpanelRect.y = (SCREEN_HEIGHT-PANEL_HEIGHT)/2;
  lpanelRect.w = rpanelRect.w = PANEL_WIDTH;
  lpanelRect.h = rpanelRect.h = PANEL_HEIGHT;

  pitch = layer->pitch/(videoBpp/8);
  buf = (LayerBit*)layer->pixels;
  ppitch = lpanel->pitch/(videoBpp/8);
  lpbuf = (LayerBit*)lpanel->pixels;
  rpbuf = (LayerBit*)rpanel->pixels;

  initPalette();
  makeSmokeBuf();
  clearLPanel();
  clearRPanel();

  loadSprites();

  stick = SDL_JoystickOpen(0);
  if (stick != NULL) {
    fprintf(stderr, " ** Joystick: %04x:%04x ** \n", SDL_JoystickGetVendor(stick), SDL_JoystickGetProduct(stick));
    fprintf(stderr, "    - Num Buttons: %d ** \n", SDL_JoystickNumButtons(stick));
    fprintf(stderr, "    - Num Axes: %d ** \n", SDL_JoystickNumAxes(stick));
    fprintf(stderr, "    - Num Balls: %d ** \n", SDL_JoystickNumBalls(stick));
    fprintf(stderr, "    - Num Hats: %d ** \n", SDL_JoystickNumHats(stick));
  }

  SDL_ShowCursor(SDL_DISABLE);
  //SDL_WM_GrabInput(SDL_GRAB_ON);
}

void closeSDL() {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_ShowCursor(SDL_ENABLE);
  SDL_Quit();
}

void blendScreen() {
  int i;
  for ( i = lyrSize-1 ; i >= 0 ; i-- ) {
    buf[i] = colorAlp[l1buf[i]][l2buf[i]];
  }
}

void flipScreen() {
  SDL_UpperBlit(layer, NULL, video, &layerRect);
  SDL_UpperBlit(lpanel, NULL, video, &lpanelRect);
  SDL_UpperBlit(rpanel, NULL, video, &rpanelRect);
  if ( status == TITLE ) {
    drawTitle();
  }
  SDL_Texture* const tex = SDL_CreateTextureFromSurface(renderer, video);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, tex, NULL, &windowRect);
  SDL_RenderPresent(renderer);
  SDL_DestroyTexture(tex);
}

void clearScreen() {
  SDL_FillRect(layer, NULL, 0);
}

void clearLPanel() {
  SDL_FillRect(lpanel, NULL, 0);
}

void clearRPanel() {
  SDL_FillRect(rpanel, NULL, 0);
}

void smokeScreen() {
  int i;
  memcpy(pbuf, l2buf, lyrSize);
  for ( i = lyrSize-1 ; i >= 0 ; i-- ) {
    l1buf[i] = colorDfs[l1buf[i]];
    l2buf[i] = colorDfs[*(smokeBuf[i])];
  }
}


void drawLine(int x1, int y1, int x2, int y2, LayerBit color, int width, LayerBit *buf) {
  int lx, ly, ax, ay, x, y, ptr, i, j;
  int xMax, yMax;

  lx = absN(x2 - x1);
  ly = absN(y2 - y1);
  if ( lx < ly ) {
    x1 -= width>>1; x2 -= width>>1;
  } else {
    y1 -= width>>1; y2 -= width>>1;
  }
  xMax = LAYER_WIDTH-width-1; yMax = LAYER_HEIGHT-width-1;

  if ( x1 < 0 ) {
    if ( x2 < 0 ) return;
    y1 = (y1-y2)*x2/(x2-x1)+y2;
    x1 = 0;
  } else if ( x2 < 0 ) {
    y2 = (y2-y1)*x1/(x1-x2)+y1;
    x2 = 0;
  }
  if ( x1 > xMax ) {
    if ( x2 > xMax ) return;
    y1 = (y1-y2)*(x2-xMax)/(x2-x1)+y2;
    x1 = xMax;
  } else if ( x2 > xMax ) {
    y2 = (y2-y1)*(x1-xMax)/(x1-x2)+y1;
    x2 = xMax;
  }
  if ( y1 < 0 ) {
    if ( y2 < 0 ) return;
    x1 = (x1-x2)*y2/(y2-y1)+x2;
    y1 = 0;
  } else if ( y2 < 0 ) {
    x2 = (x2-x1)*y1/(y1-y2)+x1;
    y2 = 0;
  }
  if ( y1 > yMax ) {
    if ( y2 > yMax ) return;
    x1 = (x1-x2)*(y2-yMax)/(y2-y1)+x2;
    y1 = yMax;
  } else if ( y2 > yMax ) {
    x2 = (x2-x1)*(y1-yMax)/(y1-y2)+x1;
    y2 = yMax;
  }

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);

  if ( lx < ly ) {
    if ( ly == 0 ) ly++;
    ax = ((x2 - x1)<<8) / ly;
    ay = ((y2 - y1)>>8) | 1;
    x  = x1<<8;
    y  = y1;
    for ( i=ly ; i>0 ; i--, x+=ax, y+=ay ){
      ptr = y*pitch + (x>>8);
      for ( j=width ; j>0 ; j--, ptr++ ) {
        buf[ptr] = color;
      }
    }
  } else {
    if ( lx == 0 ) lx++;
    ay = ((y2 - y1)<<8) / lx;
    ax = ((x2 - x1)>>8) | 1;
    x  = x1;
    y  = y1<<8;
    for ( i=lx ; i>0 ; i--, x+=ax, y+=ay ) {
      ptr = (y>>8)*pitch + x;
      for ( j=width ; j>0 ; j--, ptr+=pitch ) {
        buf[ptr] = color;
      }
    }
  }
}

void drawThickLine(int x1, int y1, int x2, int y2,
                   LayerBit color1, LayerBit color2, int width) {
  int lx, ly, ax, ay, x, y, ptr, i, j;
  int xMax, yMax;
  int width1, width2;

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);
  if ( lx < ly ) {
    x1 -= width>>1; x2 -= width>>1;
  } else {
    y1 -= width>>1; y2 -= width>>1;
  }
  xMax = LAYER_WIDTH-width; yMax = LAYER_HEIGHT-width;

  if ( x1 < 0 ) {
    if ( x2 < 0 ) return;
    y1 = (y1-y2)*x2/(x2-x1)+y2;
    x1 = 0;
  } else if ( x2 < 0 ) {
    y2 = (y2-y1)*x1/(x1-x2)+y1;
    x2 = 0;
  }
  if ( x1 > xMax ) {
    if ( x2 > xMax ) return;
    y1 = (y1-y2)*(x2-xMax)/(x2-x1)+y2;
    x1 = xMax;
  } else if ( x2 > xMax ) {
    y2 = (y2-y1)*(x1-xMax)/(x1-x2)+y1;
    x2 = xMax;
  }
  if ( y1 < 0 ) {
    if ( y2 < 0 ) return;
    x1 = (x1-x2)*y2/(y2-y1)+x2;
    y1 = 0;
  } else if ( y2 < 0 ) {
    x2 = (x2-x1)*y1/(y1-y2)+x1;
    y2 = 0;
  }
  if ( y1 > yMax ) {
    if ( y2 > yMax ) return;
    x1 = (x1-x2)*(y2-yMax)/(y2-y1)+x2;
    y1 = yMax;
  } else if ( y2 > yMax ) {
    x2 = (x2-x1)*(y1-yMax)/(y1-y2)+x1;
    y2 = yMax;
  }

  lx = abs(x2 - x1);
  ly = abs(y2 - y1);
  width1 = width - 2;

  if ( lx < ly ) {
    if ( ly == 0 ) ly++;
    ax = ((x2 - x1)<<8) / ly;
    ay = ((y2 - y1)>>8) | 1;
    x  = x1<<8;
    y  = y1;
    ptr = y*pitch + (x>>8) + 1;
    memset(&(buf[ptr]), color2, width1);
    x += ax; y += ay;
    for ( i = ly-1 ; i > 1 ; i--, x+=ax, y+=ay ){
      ptr = y*pitch + (x>>8);
      buf[ptr] = color2; ptr++;
      memset(&(buf[ptr]), color1, width1); ptr += width1;
      buf[ptr] = color2;
    }
    ptr = y*pitch + (x>>8) + 1;
    memset(&(buf[ptr]), color2, width1);
  } else {
    if ( lx == 0 ) lx++;
    ay = ((y2 - y1)<<8) / lx;
    ax = ((x2 - x1)>>8) | 1;
    x  = x1;
    y  = y1<<8;
    ptr = ((y>>8)+1)*pitch + x;
    for ( j=width1 ; j>0 ; j--, ptr+=pitch ) {
      buf[ptr] = color2;
    }
    x += ax; y += ay;
    for ( i=lx-1 ; i>1 ; i--, x+=ax, y+=ay ) {
      ptr = (y>>8)*pitch + x;
      buf[ptr] = color2; ptr += pitch;
      for ( j=width1 ; j>0 ; j--, ptr+=pitch ) {
        buf[ptr] = color1;
      }
      buf[ptr] = color2;
    }
    ptr = ((y>>8)+1)*pitch + x;
    for ( j=width1 ; j>0 ; j--, ptr+=pitch ) {
      buf[ptr] = color2;
    }
  }
}

void drawBox(int x, int y, int width, int height,
             LayerBit color1, LayerBit color2, LayerBit *buf) {
  int i, j;
  LayerBit cl;
  int ptr;

  x -= width>>1; y -= height>>1;
  if ( x < 0 ) {
    width += x; x = 0;
  }
  if ( x+width >= LAYER_WIDTH ) {
    width = LAYER_WIDTH-x;
  }
  if ( width <= 1 ) return;
  if ( y < 0 ) {
    height += y; y = 0;
  }
  if ( y+height > LAYER_HEIGHT ) {
    height = LAYER_HEIGHT-y;
  }
  if ( height <= 1 ) return;

  ptr = x + y*LAYER_WIDTH;
  memset(&(buf[ptr]), color2, width);
  y++;
  for ( i=0 ; i<height-2 ; i++, y++ ) {
    ptr = x + y*LAYER_WIDTH;
    buf[ptr] = color2; ptr++;
    memset(&(buf[ptr]), color1, width-2);
    ptr += width-2;
    buf[ptr] = color2;
  }
  ptr = x + y*LAYER_WIDTH;
  memset(&(buf[ptr]), color2, width);
}

void drawBoxPanel(int x, int y, int width, int height,
                  LayerBit color1, LayerBit color2, LayerBit *buf) {
  int i, j;
  LayerBit cl;
  int ptr;

  x -= width>>1; y -= height>>1;
  if ( x < 0 ) {
    width += x; x = 0;
  }
  if ( x+width >= PANEL_WIDTH ) {
    width = PANEL_WIDTH-x;
  }
  if ( width <= 1 ) return;
  if ( y < 0 ) {
    height += y; y = 0;
  }
  if ( y+height > PANEL_HEIGHT ) {
    height = PANEL_HEIGHT-y;
  }
  if ( height <= 1 ) return;

  ptr = x + y*PANEL_WIDTH;
  memset(&(buf[ptr]), color2, width);
  y++;
  for ( i=0 ; i<height-2 ; i++, y++ ) {
    ptr = x + y*PANEL_WIDTH;
    buf[ptr] = color2; ptr++;
    memset(&(buf[ptr]), color1, width-2);
    ptr += width-2;
    buf[ptr] = color2;
  }
  ptr = x + y*PANEL_WIDTH;
  memset(&(buf[ptr]), color2, width);
}

// Draw the numbers.
int drawNum(int n, int x ,int y, int s, int c1, int c2) {
  for ( ; ; ) {
    drawLetter(n%10, x, y, s, 1, c1, c2, lpbuf);
    y += s*1.7f;
    n /= 10;
    if ( n <= 0 ) break;
  }
  return y;
}

int drawNumRight(int n, int x ,int y, int s, int c1, int c2) {
  int d, nd, drawn = 0;
  for ( d = 100000000 ; d > 0 ; d /= 10 ) {
    nd = (int)(n/d);
    if ( nd > 0 || drawn ) {
      n -= d*nd;
      drawLetter(nd%10, x, y, s, 3, c1, c2, rpbuf);
      y += s*1.7f;
      drawn = 1;
    }
  }
  if ( !drawn ) {
    drawLetter(0, x, y, s, 3, c1, c2, rpbuf);
    y += s*1.7f;
  }
  return y;
}

int drawNumCenter(int n, int x ,int y, int s, int c1, int c2) {
  for ( ; ; ) {
    drawLetterBuf(n%10, x, y, s, 2, c1, c2, buf, 0);
    x -= s*1.7f;
    n /= 10;
    if ( n <= 0 ) break;
  }
  return y;
}


#define JOYSTICK_AXIS 16384

int getPadState() {
  int x = 0, y = 0;
  int pad = 0;
  int left, right, up, down;
  if ( stick != NULL ) {
    x = SDL_JoystickGetAxis(stick, 0);
    y = SDL_JoystickGetAxis(stick, 1);
    const int hat = SDL_JoystickGetHat(stick, 0);
    left = (hat & SDL_HAT_LEFT) != 0;
    right = (hat & SDL_HAT_RIGHT) != 0;
    up = (hat & SDL_HAT_UP) != 0;
    down = (hat & SDL_HAT_DOWN) != 0;
  }
  if ( keys[SDL_SCANCODE_RIGHT] == SDL_PRESSED || x > JOYSTICK_AXIS || right ) {
    pad |= PAD_RIGHT;
  }
  if ( keys[SDL_SCANCODE_LEFT] == SDL_PRESSED || x < -JOYSTICK_AXIS || left ) {
    pad |= PAD_LEFT;
  }
  if ( keys[SDL_SCANCODE_DOWN] == SDL_PRESSED || y > JOYSTICK_AXIS || down ) {
    pad |= PAD_DOWN;
  }
  if ( keys[SDL_SCANCODE_UP] == SDL_PRESSED || y < -JOYSTICK_AXIS || up ) {
    pad |= PAD_UP;
  }
  return pad;
}

int buttonReversed = 0;

int getButtonState() {
  int btn = 0;
  int btn1 = 0, btn2 = 0;
  if ( stick != NULL ) {
    btn1 = SDL_JoystickGetButton(stick, 3);
    btn2 = SDL_JoystickGetButton(stick, 0);
  }
  if ( keys[SDL_SCANCODE_Z] == SDL_PRESSED || btn1 ) {
    btn |= !buttonReversed ? PAD_BUTTON1 : PAD_BUTTON2;
  }
  if ( keys[SDL_SCANCODE_X] == SDL_PRESSED || btn2 ) {
    btn |= !buttonReversed ? PAD_BUTTON2 : PAD_BUTTON1;
  }
  return btn;
}
