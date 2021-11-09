#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include "math.h"
#include "domath.c"
// You'll need to include the DOME header
#include "dome.h"

static GRAPHICS_API_v0* graphics;
static DOME_API_v0* core;
static IO_API_v0* io;
static WREN_API_v0* wren;
static void (*unsafePset)(DOME_Context, int32_t, int32_t, DOME_Color) = NULL;
static DOME_Bitmap* bitmap;

static uint32_t WIDTH = 0;
static uint32_t HEIGHT = 0;

inline double
clamp(double min, double x, double max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
  // return fmax(min, fmin(x, max));
}

typedef struct {
  size_t width;
  size_t height;
  uint8_t* data;
} MAP;

typedef struct {
  MAP map;
} RENDERER;

uint32_t BLACK = 0xFF000000;
uint32_t R =  0xFFFF0000;
uint32_t B = 0xFF0000FF;
uint32_t texture[64] = {};

#define mapWidth 24
#define mapHeight 24

int worldMap[mapWidth][mapHeight]=
{
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,2,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,0,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

double posX = 22, posY = 12;  //x and y start position
double dirX = -1, dirY = 0; //initial direction vector
double planeX = 0, planeY = 0.66; //the 2d raycaster version of camera plane

static const char* source =  ""
"foreign class Raycaster {\n" // Source file for an external module
"construct init() {} \n"
"foreign setPosition(x, y) \n"
"foreign setAngle(angle) \n"
"foreign draw(alpha) \n"
"} \n";

void allocate(WrenVM* vm) {
  DOME_Context ctx = core->getContext(vm);
  WIDTH = graphics->getWidth(ctx);
  HEIGHT = graphics->getHeight(ctx);
  size_t CLASS_SIZE = sizeof(RENDERER); // This should be the size of your object's data
  void* obj = wren->setSlotNewForeign(vm, 0, 0, CLASS_SIZE);
}

void setPosition(WrenVM* vm) {
  double x = wren->getSlotDouble(vm, 1);
  double y = wren->getSlotDouble(vm, 2);
  posX = x;
  posY = y;
}
void setAngle(WrenVM* vm) {
  double angle = wren->getSlotDouble(vm, 1);
  double rads = angle * M_PI / 180.0;
  dirX = cos(rads);
  dirY = sin(rads);
  planeX = -dirY;
  planeY = dirX;
}

void vLine(DOME_Context ctx, int32_t x, int32_t y0, uint32_t y1, DOME_Color color) {
  y0 = fmax(0, y0);
  y1 = fmin(y1, HEIGHT);
  for (int y = y0; y <= y1; y++) {
    unsafePset(ctx, x, y, color);
  }
}

void draw(WrenVM* vm) {
  DOME_Context ctx = core->getContext(vm);
  double alpha = wren->getSlotDouble(vm, 1);
  // Retrieve the DOME Context from the VM. This is needed for many things.
  DOME_Color color;

  V2 rayPosition = { posX, posY };
  V2 direction = { dirX, dirY };
  V2 camera = { planeX, planeY };

  int w = WIDTH;
  int h = HEIGHT;

  int texWidth = 64;
  int texHeight = 64;

  for(int x = 0; x < w; x++) {
    // Perform DDA first
    double cameraX = 2 * (x / (double)w) - 1;
  //   V2 rayDirection = V2_add(direction, V2_mul(camera, cameraX));
    // cast ray
    V2 rayDirection = {direction.x + camera.x * cameraX, direction.y + camera.y * cameraX};
    V2 sideDistance = {0, 0};
    /*
    sideDistance.x = sqrt(1.0 + pow((rayDirection.y / rayDirection.x), 2));
    sideDistance.y = sqrt(1.0 + pow((rayDirection.x / rayDirection.y), 2));
    */
    sideDistance.x = sqrt(1.0 + pow(rayDirection.y, 2) / pow(rayDirection.x, 2));
    sideDistance.y = sqrt(1.0 + pow(rayDirection.x, 2) / pow(rayDirection.y, 2));
    V2 nextSideDistance;
    V2 mapPos = { floor(rayPosition.x), floor(rayPosition.y) };
    V2 stepDirection = {0, 0};
    if (rayDirection.x < 0) {
      stepDirection.x = -1;
      nextSideDistance.x = (rayPosition.x - mapPos.x) * sideDistance.x;
    } else {
      stepDirection.x = 1;
      nextSideDistance.x = (mapPos.x + 1.0 - rayPosition.x) * sideDistance.x;
    }

    if (rayDirection.y < 0) {
      stepDirection.y = -1;
      nextSideDistance.y = (rayPosition.y - mapPos.y) * sideDistance.y;

    } else {
      stepDirection.y = 1;
      nextSideDistance.y = (mapPos.y + 1.0 - rayPosition.y) * sideDistance.y;
    }
    bool hit = false;
    int side = 0;
    int tile = 0;
    while(!hit) {
      if (nextSideDistance.x < nextSideDistance.y) {
        nextSideDistance.x += sideDistance.x;
        mapPos.x += stepDirection.x;
        side = 0;
      } else {
        nextSideDistance.y += sideDistance.y;
        mapPos.y += stepDirection.y;
        side = 1;
      }
      if (mapPos.x < 0 || mapPos.x >= mapWidth || mapPos.y < 0 || mapPos.y >= mapHeight) {
        tile = 1;
        hit = true;
      } else {
        tile = worldMap[(int)(mapPos.x)][(int)(mapPos.y)];
      }
      hit = (tile > 0);
      // Check for door and thin walls here
    }

    double perpWallDistance;
    if (side == 0) {
      perpWallDistance = fabs((mapPos.x - rayPosition.x + (1 - stepDirection.x) / 2.0) / rayDirection.x);
    } else {
      perpWallDistance = fabs((mapPos.y - rayPosition.y + (1 - stepDirection.y) / 2.0) / rayDirection.y);
    }

    // Calculate perspective of wall-slice
    double halfH = (double)h / 2.0;
    double lineHeight = fmax(0, ((double)h / perpWallDistance));
    double drawStart = (-lineHeight / 2.0) + halfH;
    double drawEnd = (lineHeight / 2.0) + halfH;
    drawStart = clamp(0, drawStart, h - 1);
    drawEnd = clamp(0, drawEnd, h - 1);

    double wallX;
    if (side == 0) {
      wallX = rayPosition.y + perpWallDistance * rayDirection.y;
    } else {
      wallX = rayPosition.x + perpWallDistance * rayDirection.x;
    }
    wallX = wallX - floor(wallX);
    int drawWallStart = fmax(0, (int)drawStart);
    int drawWallEnd = fmin((int)ceil(drawEnd), h - 1);
    DOME_Color color;
    int texX = (int)floor(wallX * (double)(texWidth - 1));
    if (side == 0 && rayDirection.x < 0) {
      texX = (texWidth - 1) - texX;
    }
    if (side == 1 && rayDirection.y > 0) {
      texX = (texWidth - 1) - texX;
    }

    texX = clamp(0, texX, texWidth - 1);
    assert(texX >= 0);
    assert(texX < texWidth);

    double texStep = (double)(texHeight) / lineHeight;
    double texPos = (ceil(drawStart) - halfH + (lineHeight / 2.0)) * texStep;
    for (int y = drawWallStart; y < drawWallEnd; y++) {
      int texY = ((int)texPos) % texHeight;
      assert(texY >= 0);
      assert(texY < texHeight);
      color = bitmap->pixels[texWidth * texY + texX]; // textureNum
      if (side == 1) {
        uint8_t alpha = color.component.a;
        color.value = (color.value >> 1) & 8355711;
        color.component.a = alpha;
      }
      assert(y < h);
      unsafePset(ctx, x, y, color);
      texPos += texStep;
    }

#if 0
    switch(tile)
    {
      case 1:  color.value = 0xFFFF0000;  break; //red
      case 2:  color.value = 0xFF00FF00;  break; //green
      case 3:  color.value = 0xFF0000FF;   break; //blue
      case 4:  color.value = 0xFFFFFFFF;  break; //white
      default: color.value = 0xFF00FFFF; break; //yellow
    }

    //give x and y sides different brightness
    if (side == 1) {
      color.component.r /= 2;
      color.component.g /= 2;
      color.component.b /= 2;
    }
    vLine(ctx, x, drawWallStart, drawWallEnd, color);
#endif
  }
  graphics->draw(ctx, bitmap, 0, 0, DOME_DRAWMODE_BLEND);
}

/*
void drawOld(WrenVM* vm) {
  // Fetch the method argument
  double alpha = wren->getSlotDouble(vm, 1);
  int texWidth = 64;
  int texHeight = 64;

  // Retrieve the DOME Context from the VM. This is needed for many things.
  DOME_Context ctx = core->getContext(vm);
  DOME_Color color;

  int w = WIDTH;
  int h = HEIGHT;

  for(int x = 0; x < w; x++) {
    //calculate ray position and direction
    double cameraX = 2 * x / ((double)w) - 1; //x-coordinate in camera space
    double rayDirX = dirX + planeX * cameraX;
    double rayDirY = dirY + planeY * cameraX;
    //which box of the map we're in

    int mapX = posX;
    int mapY = posY;

    //length of ray from current position to next x or y-side
    double sideDistX;
    double sideDistY;

    //length of ray from one x or y-side to next x or y-side
    double deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1 / rayDirX);
    double deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1 / rayDirY);
    double perpWallDist;

    //what direction to step in x or y-direction (either +1 or -1)
    int stepX;
    int stepY;

    int hit = 0; //was there a wall hit?
    int side; //was a NS or a EW wall hit?


    //calculate step and initial sideDist
    if (rayDirX < 0) {
      stepX = -1;
      sideDistX = (posX - mapX) * deltaDistX;
    } else {
      stepX = 1;
      sideDistX = (mapX + 1.0 - posX) * deltaDistX;
    }
    if (rayDirY < 0) {
      stepY = -1;
      sideDistY = (posY - mapY) * deltaDistY;
    } else {
      stepY = 1;
      sideDistY = (mapY + 1.0 - posY) * deltaDistY;
    }

    //perform DDA
    while (hit == 0) {
      //jump to next map square, either in x-direction, or in y-direction
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      //Check if ray has hit a wall
      if (worldMap[mapX][mapY] > 0) {
        hit = 1;
      }
    }

    //Calculate distance projected on camera direction (Euclidean distance would give fisheye effect!)
    if(side == 0) perpWallDist = (sideDistX - deltaDistX);
    else          perpWallDist = (sideDistY - deltaDistY);

    //Calculate height of line to draw on screen
    int lineHeight = (int)(h / perpWallDist);

    //calculate lowest and highest pixel to fill in current stripe
    int drawStart = -lineHeight / 2 + h / 2;
    if(drawStart < 0)drawStart = 0;
    int drawEnd = lineHeight / 2 + h / 2;
    if(drawEnd >= h)drawEnd = h - 1;

    //choose wall color
    switch(worldMap[mapX][mapY])
    {
      case 1:  color.value = 0xFFFF0000;  break; //red
      case 2:  color.value = 0xFF00FF00;  break; //green
      case 3:  color.value = 0xFF0000FF;   break; //blue
      case 4:  color.value = 0xFFFFFFFF;  break; //white
      default: color.value = 0xFF00FFFF; break; //yellow
    }

    //give x and y sides different brightness
    if (side == 1) {color.value = color.value / 2;}
    if (worldMap[mapX][mapY] > 0) {
      //texturing calculations
      int texNum = worldMap[mapX][mapY] - 1; //1 subtracted from it so that texture 0 can be used!

      //calculate value of wallX
      double wallX; //where exactly the wall was hit
      if (side == 0) wallX = posY + perpWallDist * rayDirY;
      else           wallX = posX + perpWallDist * rayDirX;
      wallX -= floor((wallX));

      //x coordinate on the texture
      int texX = clamp(0, round(wallX * (double)texWidth), texWidth);
      if(side == 0 && rayDirX > 0) texX = texWidth - texX - 1;
      if(side == 1 && rayDirY < 0) texX = texWidth - texX - 1;
      // How much to increase the texture coordinate per screen pixel
      double step = 1.0 * texHeight / lineHeight;
      // Starting texture coordinate
      double texPos = (drawStart - h / 2 + lineHeight / 2) * step;
      for(int y = drawStart; y<drawEnd; y++)
      {
        // Cast the texture coordinate to integer, and mask with (texHeight - 1) in case of overflow
        int texY = (int)clamp(0, round(texPos), texHeight - 1);
        texPos += step;
        color = bitmap->pixels[texHeight * texY + texX]; // textureNum
        //make color darker for y-sides: R, G and B byte each divided through two with a "shift" and an "and"
        uint8_t alpha = color.component.a;
        if(side == 1) color.value = (color.value >> 1) & 8355711;
        color.component.a = alpha;
        unsafePset(ctx, x, y, color);
      }
    } else {
      vLine(ctx, x, drawStart, drawEnd, color);
    }
  }
  graphics->draw(ctx, bitmap, 0, 0, DOME_DRAWMODE_BLEND);
}
*/

DOME_EXPORT DOME_Result PLUGIN_onInit(DOME_getAPIFunction DOME_getAPI,
    DOME_Context ctx) {

  // Fetch the latest Core API and save it for later use.
  core = DOME_getAPI(API_DOME, DOME_API_VERSION);
  io = DOME_getAPI(API_IO, IO_API_VERSION);
  graphics = DOME_getAPI(API_GRAPHICS, GRAPHICS_API_VERSION);
  unsafePset = graphics->unsafePset;

  // DOME also provides a subset of the Wren API for accessing slots
  // in foreign methods.
  wren = DOME_getAPI(API_WREN, WREN_API_VERSION);

  core->log(ctx, "Initialising raycaster module\n");

  // Register a module with it's associated source.
  // Avoid giving the module a common name.
  core->registerModule(ctx, "raycaster", source);

  core->registerClass(ctx, "raycaster", "Raycaster", allocate, NULL);
  core->registerFn(ctx, "raycaster", "Raycaster.draw(_)", draw);
  core->registerFn(ctx, "raycaster", "Raycaster.setAngle(_)", setAngle);
  core->registerFn(ctx, "raycaster", "Raycaster.setPosition(_,_)", setPosition);

  uint32_t temp[8][8] = {
    R,R,R,R,R,R,R,R,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B,
    R,B,B,B,B,B,B,B
  };
  for (int j = 0; j < 8; j++) {
    for (int i = 0; i < 8; i++) {
      texture[j*8 + i] = temp[j][i];
    }
  }

  core->log(ctx, "Loading...\n");
  bitmap = io->readImageFile(ctx, "wall.png");
  core->log(ctx, "Complete\n");

  // Returning anything other than SUCCESS here will result in the current fiber
  // aborting. Use this to indicate if your plugin initialised successfully.
  return DOME_RESULT_SUCCESS;
}

DOME_EXPORT DOME_Result PLUGIN_preUpdate(DOME_Context ctx) {
  //  DOME_Color color = graphics->pget(ctx, 0, 0);
  // core->log(ctx, "a: 0x%02hX, r: 0x%02hX, g: 0x%02hX, b: 0x%02hX\n", color.component.a, color.component.r, color.component.g, color.component.b);
  return DOME_RESULT_SUCCESS;
}

DOME_EXPORT DOME_Result PLUGIN_postUpdate(DOME_Context ctx) {
  return DOME_RESULT_SUCCESS;
}
DOME_EXPORT DOME_Result PLUGIN_preDraw(DOME_Context ctx) {
  return DOME_RESULT_SUCCESS;
}
DOME_EXPORT DOME_Result PLUGIN_postDraw(DOME_Context ctx) {
  return DOME_RESULT_SUCCESS;
}

DOME_EXPORT DOME_Result PLUGIN_onShutdown(DOME_Context ctx) {
  io->freeBitmap(bitmap);
  return DOME_RESULT_SUCCESS;
}

