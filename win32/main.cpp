/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma warning ( disable : 4530 )		// Don't warn about unused exceptions.

#include "glew.h"
#include "SDL.h "

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glvector.h"
#include "../game/cgame.h"
#include "../grinliz/glstringutil.h"
#include "audio.h"
#include "../version.h"

// Used for map maker mode - directly call the game object.
#include "../game/game.h"

#include "wglew.h"

// For error logging.
#define WINDOWS_LEAN_AND_MEAN
#include <winhttp.h>

//#define TEST_ROTATION
//#define TEST_FULLSPEED

#define TIME_BETWEEN_FRAMES	30

#define IPOD_SCREEN_WIDTH	320
#define IPOD_SCREEN_HEIGHT	480

#define NEXUS_ONE_SCREEN_WIDTH  480
#define NEXUS_ONE_SCREEN_HEIGHT  800

#if 0
static const int SCREEN_WIDTH  = IPOD_SCREEN_WIDTH;
static const int SCREEN_HEIGHT = IPOD_SCREEN_HEIGHT;
#else
static const int SCREEN_WIDTH  = NEXUS_ONE_SCREEN_WIDTH;
static const int SCREEN_HEIGHT = NEXUS_ONE_SCREEN_HEIGHT;
#endif

const int multisample = 2;
bool fullscreen = false;
int screenWidth = 0;
int screenHeight = 0;

#ifdef TEST_ROTATION
const int rotation = 1;
#else
const int rotation = 0;
#endif

void ScreenCapture( const char* baseFilename );
void PostCurrentGame();


void TransformXY( int x0, int y0, int* x1, int* y1 )
{
	// As a way to do scaling outside of the core, translate all
	// the mouse coordinates so that they are reported in opengl
	// window coordinates.
	if ( rotation == 0 ) {
		*x1 = x0;
		*y1 = screenHeight-1-y0;
	}
	else if ( rotation == 1 ) {
		*x1 = x0;
		*y1 = screenHeight-1-y0;
	}
	else {
		GLASSERT( 0 );
	}
}


Uint32 TimerCallback(Uint32 interval, void *param)
{
	SDL_Event user;
	memset( &user, 0, sizeof(user ) );
	user.type = SDL_USEREVENT;

	SDL_PushEvent( &user );
	return interval;
}


int main( int argc, char **argv )
{    
	MemStartCheck();
	{ char* test = new char[16]; delete [] test; }

	SDL_Surface *surface = 0;

	// SDL initialization steps.
    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE | SDL_INIT_TIMER | SDL_INIT_AUDIO ) < 0 )
	{
	    fprintf( stderr, "SDL initialization failed: %s\n", SDL_GetError( ) );
		exit( 1 );
	}
	SDL_EnableKeyRepeat( 0, 0 );
	SDL_EnableUNICODE( 1 );

	const SDL_version* sversion = SDL_Linked_Version();
	GLOUTPUT(( "SDL: major %d minor %d patch %d\n", sversion->major, sversion->minor, sversion->patch ));

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8);

	if ( multisample ) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, multisample );
	}

	int	videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL */
		videoFlags |= SDL_GL_DOUBLEBUFFER; /* Enable double buffering */

	if ( fullscreen )
		videoFlags |= SDL_FULLSCREEN;
	else
		videoFlags |= SDL_RESIZABLE;

#ifdef TEST_ROTATION
	screenWidth  = SCREEN_WIDTH;
	screenHeight = SCREEN_HEIGHT;
#else
	screenWidth  = SCREEN_HEIGHT;
	screenHeight = SCREEN_WIDTH;
#endif

	// Note that our output surface is rotated from the iPod.
	//surface = SDL_SetVideoMode( IPOD_SCREEN_HEIGHT, IPOD_SCREEN_WIDTH, 32, videoFlags );
	surface = SDL_SetVideoMode( screenWidth, screenHeight, 32, videoFlags );
	GLASSERT( surface );

	int stencil = 0;
	int depth = 0;
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &stencil );
	glGetIntegerv( GL_DEPTH_BITS, &depth );
	GLOUTPUT(( "SDL surface created. w=%d h=%d bpp=%d stencil=%d depthBits=%d\n", 
				surface->w, surface->h, surface->format->BitsPerPixel, stencil, depth ));

    /* Verify there is a surface */
    if ( !surface ) {
	    fprintf( stderr,  "Video mode set failed: %s\n", SDL_GetError( ) );
	    exit( 1 );
	}

	int r = glewInit();
	GLASSERT( r == GL_NO_ERROR );
#ifdef TEST_FULLSPEED	
	wglSwapIntervalEXT( 0 );	// vsync
#else
	wglSwapIntervalEXT( 1 );	// vsync
#endif

	const unsigned char* vendor   = glGetString( GL_VENDOR );
	const unsigned char* renderer = glGetString( GL_RENDERER );
	const unsigned char* version  = glGetString( GL_VERSION );

	GLOUTPUT(( "OpenGL vendor: '%s'  Renderer: '%s'  Version: '%s'\n", vendor, renderer, version ));

#if 0
	PostCurrentGame();
#else
	{
		// Check for a "didn't crash" file.
		FILE* fp = fopen( "UFO_Running.txt", "r" );
		if ( fp ) {
			fseek( fp, 0, SEEK_END );
			long len = ftell( fp );
			if ( len > 1 ) {
				// Wasn't deleted.
				PostCurrentGame();
			}
			fclose( fp );
		}
	}
	{
		FILE* fp = fopen( "UFO_Running.txt", "w" );
		if ( fp ) {
			fprintf( fp, "Game running." );
			fclose( fp );
		}
	}
#endif

	Audio_Init();

	bool done = false;
	bool zooming = false;
    SDL_Event event;

	float yRotation = 45.0f;
	grinliz::Vector2I mouseDown = { 0, 0 };
	grinliz::Vector2I prevMouseDown = { 0, 0 };
	U32 prevMouseDownTime = 0;

	int zoomX = 0;
	int zoomY = 0;

	void* game = 0;
	bool mapMakerMode = false;

	if ( argc > 1 ) {
		// -- MapMaker -- //
		Engine::mapMakerMode = true;

		TileSetDesc desc;
		desc.set = "FARM";
		desc.size = 16;
		desc.type = "TILE";
		desc.variation = 0;

		if ( argc > 2 ) {
			desc.set = argv[2];
			GLASSERT( strlen( desc.set ) == 4 );
		}

		if ( argc > 3 ) {
			desc.size = atol( argv[3] );
			GLASSERT( desc.size == 16 || desc.size == 32 || desc.size == 64 );
		}

		if ( argc > 4 ) {
			desc.type = argv[4];
			GLASSERT( strlen( desc.type ) == 4 );
		}

		if ( argc > 5 ) {
			desc.variation = atol( argv[5] );
			GLASSERT( desc.variation >= 0 && desc.variation < 100 );
		}

		game = new Game( screenWidth, screenHeight, rotation, ".\\resin\\", desc );
		mapMakerMode = true;
	}
	else {
		game = NewGame( screenWidth, screenHeight, rotation, ".\\" );
	}

#ifndef TEST_FULLSPEED
	SDL_TimerID timerID = SDL_AddTimer( TIME_BETWEEN_FRAMES, TimerCallback, 0 );
#endif

	// ---- Main Loop --- //
#ifdef TEST_FULLSPEED	
	while ( !done ) {
		if ( SDL_PollEvent( &event ) )
#else
	while ( !done && SDL_WaitEvent( &event ) )
#endif
	{
		switch( event.type )
		{
			case SDL_VIDEORESIZE:
				screenWidth = event.resize.w;
				screenHeight = event.resize.h;
				surface = SDL_SetVideoMode( screenWidth, screenHeight, 32, videoFlags );
				GameDeviceLoss( game );
				GameResize( game, event.resize.w, event.resize.h, rotation );
				break;

			case SDL_KEYDOWN:
			{
				SDLMod sdlMod = SDL_GetModState();

				switch ( event.key.keysym.sym )
				{
#ifdef DEBUG
					case SDLK_ESCAPE:
						done = true;
						break;
#endif

					case SDLK_F4:
						if ( sdlMod & ( KMOD_RALT | KMOD_LALT ) )
							done = true;
						break;

						/*
					case SDLK_KP_PLUS:
					case SDLK_KP_MINUS:
						{
							float zoom;
							GameCameraGet( game, GAME_CAMERA_ZOOM, &zoom );
							zoom += event.key.keysym.sym == SDLK_KP_PLUS ? 0.1f : -0.1f;
							GameCameraSet( game, GAME_CAMERA_ZOOM, zoom );
						}
						break;
						*/

					case SDLK_RIGHT:
						if ( !mapMakerMode ) {
							if ( sdlMod & (KMOD_RCTRL|KMOD_LCTRL) )
								GameHotKey( game, GAME_HK_ROTATE_CW );
							else
								GameHotKey( game, GAME_HK_NEXT_UNIT );
						}
						break;

					case SDLK_LEFT:
						if ( !mapMakerMode ) {
							if ( sdlMod & (KMOD_RCTRL|KMOD_LCTRL) )
								GameHotKey( game, GAME_HK_ROTATE_CCW );
							else
								GameHotKey( game, GAME_HK_PREV_UNIT );
						}
						break;

					case SDLK_u:
						if ( mapMakerMode ) {
							((Game*)game)->engine->camera.SetTilt( -90.0f );
							((Game*)game)->engine->camera.SetPosWC( 8.f, 90.f, 8.f );
							((Game*)game)->engine->camera.SetYRotation( 0.0f );
						}
						else {
							GameHotKey( game, GAME_HK_TOGGLE_ROTATION_UI | GAME_HK_TOGGLE_NEXT_UI );
						}
						break;

					case SDLK_s:
						ScreenCapture( "cap" );
						break;

					case SDLK_d:
						GameHotKey( game, GAME_HK_TOGGLE_DEBUG_TEXT );
						break;

					case SDLK_DELETE:
						if ( mapMakerMode )
							((Game*)game)->DeleteAtSelection(); 
						break;

					case SDLK_KP9:			
						if ( mapMakerMode )
							((Game*)game)->RotateSelection( -1 );			
						break;

					case SDLK_r:
					case SDLK_KP7:			
						if ( mapMakerMode )
							((Game*)game)->RotateSelection( 1 );			
						break;

					case SDLK_KP8:			
						if ( mapMakerMode )
							((Game*)game)->DeltaCurrentMapItem(16);			
						break;

					case SDLK_KP5:			
						if ( mapMakerMode )
							((Game*)game)->DeltaCurrentMapItem(-16);		
						break;

					case SDLK_KP6:			
						if ( mapMakerMode )
							((Game*)game)->DeltaCurrentMapItem(1); 			
						break;

					case SDLK_KP4:			
						if ( mapMakerMode )
							((Game*)game)->DeltaCurrentMapItem(-1);			
						break;

					case SDLK_p:
						//if ( mapMakerMode )
						{
							int pathing = (((Game*)game)->ShowingPathing() + 1) % 3;
							((Game*)game)->ShowPathing( pathing );
						}
						break;

					case SDLK_t:
						if ( mapMakerMode )
							((Game*)game)->engine->GetMap()->SetDayTime( !((Game*)game)->engine->GetMap()->DayTime() );
						break;

					case SDLK_m:
						if ( mapMakerMode )
							((Game*)game)->engine->EnableMetadata( !((Game*)game)->engine->IsMetadataEnabled() );
						break;

					default:
						break;
				}
/*					GLOUTPUT(( "fov=%.1f rot=%.1f h=%.1f\n", 
							game->engine.fov, 
							game->engine.camera.Tilt(), 
							game->engine.camera.PosWC().y ));
*/
			}
			break;

			case SDL_MOUSEBUTTONDOWN:
			{
				int x, y;
				TransformXY( event.button.x, event.button.y, &x, &y );

				mouseDown.Set( event.button.x, event.button.y );

				if ( event.button.button == 1 ) {
					GameTap( game, GAME_TAP_DOWN, x, y );
				}
				else if ( event.button.button == 3 ) {
					GameTap( game, GAME_TAP_CANCEL, x, y );
					zooming = true;
					//GameCameraRotate( game, GAME_ROTATE_START, 0.0f );
					SDL_GetRelativeMouseState( &zoomX, &zoomY );
				}
			}
			break;

			case SDL_MOUSEBUTTONUP:
			{
				int x, y;
				TransformXY( event.button.x, event.button.y, &x, &y );

				if ( event.button.button == 3 ) {
					zooming = false;
				}
				if ( event.button.button == 1 ) {
					GameTap( game, GAME_TAP_UP, x, y );
				}
			}
			break;

			case SDL_MOUSEMOTION:
			{
				SDL_GetRelativeMouseState( &zoomX, &zoomY );
				int state = SDL_GetMouseState(NULL, NULL);
				int x, y;
				TransformXY( event.button.x, event.button.y, &x, &y );

				if ( state & SDL_BUTTON(1) ) {
					GameTap( game, GAME_TAP_MOVE, x, y );
				}
				else if ( zooming && (state & SDL_BUTTON(3)) ) {
					float deltaZoom = 0.01f * (float)zoomY;
					GameZoom( game, GAME_ZOOM_DISTANCE, deltaZoom );
					GameCameraRotate( game, (float)(zoomX)*0.5f );
				}
				else if ( ( ( state & SDL_BUTTON(1) ) == 0 ) && Engine::mapMakerMode ) {
					((Game*)game)->MouseMove( x, y );
				}
			}
			break;

			case SDL_QUIT:
			{
				done = true;
			}
			break;

			case SDL_USEREVENT:
			{
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );

				GameDoTick( game, SDL_GetTicks() );
				SDL_GL_SwapBuffers();

				int size=0, offset=0;
				while ( GamePopSound( game, &offset, &size ) ) {
					Audio_PlayWav( "./res/uforesource.db", offset, size );
				}
			};

			default:
				break;
		}
#ifdef TEST_FULLSPEED	
		}

		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );

		GameDoTick( game, SDL_GetTicks() );
		SDL_GL_SwapBuffers();
	}
#else
	}
	SDL_RemoveTimer( timerID );
#endif

	GameSave( game );
	DeleteGame( game );
	Audio_Close();

	SDL_Quit();

	// Empty the file - quit went just fine.
	{
		FILE* fp = fopen( "UFO_Running.txt", "w" );
		if ( fp )
			fclose( fp );
	}

	MemLeakCheck();
	return 0;
}



void ScreenCapture( const char* baseFilename )
{
	int viewPort[4];
	glGetIntegerv(GL_VIEWPORT, viewPort);
	int width  = viewPort[2]-viewPort[0];
	int height = viewPort[3]-viewPort[1];

	SDL_Surface* surface = SDL_CreateRGBSurface( SDL_SWSURFACE, width, height, 
												 32, 0xff, 0xff<<8, 0xff<<16, 0xff<<24 );
	if ( !surface )
		return;

	glReadPixels( 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels );

	// This is a fancy swap, for the screen pixels:
	int i;
	U32* buffer = new U32[width];

	for( i=0; i<height/2; ++i )
	{
		memcpy( buffer, 
				( (U32*)surface->pixels + i*width ), 
				width*4 );
		memcpy( ( (U32*)surface->pixels + i*width ), 
				( (U32*)surface->pixels + (height-1-i)*width ),
				width*4 );
		memcpy( ( (U32*)surface->pixels + (height-1-i)*width ),
				buffer,
				width*4 );
	}
	delete [] buffer;

	// And now, set all the alphas to opaque:
	for( i=0; i<width*height; ++i )
		*( (U32*)surface->pixels + i ) |= 0xff000000;

	int index = 0;
	char buf[ 256 ];
	for( index = 0; index<100; ++index )
	{
		grinliz::SNPrintf( buf, 256, "%s%02d.bmp", baseFilename, index );
#pragma warning ( push )
#pragma warning ( disable : 4996 )	// fopen is unsafe. For video games that seems extreme.
		FILE* fp = fopen( buf, "rb" );
#pragma warning ( pop )
		if ( fp )
			fclose( fp );
		else
			break;
	}
	if ( index < 100 )
		SDL_SaveBMP( surface, buf );
	SDL_FreeSurface( surface );
}


void PostCurrentGame()
{
	GLOUTPUT(( "Posting current game.\n" ));

    BOOL  bResults = FALSE;
    HINTERNET hSession = NULL,
              hConnect = NULL,
              hRequest = NULL;

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(  L"UFO Attack", 
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, 
                             WINHTTP_NO_PROXY_BYPASS, 0);

    // Specify an HTTP server.
    if (hSession)
        hConnect = WinHttpConnect( hSession, L"www.grinninglizard.com",
                                   INTERNET_DEFAULT_HTTP_PORT, 0);

    // Create an HTTP Request handle.
    if (hConnect)
        hRequest = WinHttpOpenRequest( hConnect, L"POST", 
                                       L"/collect/server.php", 
                                       NULL, WINHTTP_NO_REFERER, 
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                       0);

	char buf[32];
	grinliz::SNPrintf( buf, 32, "version=%d", VERSION );

	std::string data( buf );
	data += "&device=win32&stacktrace=";
	FILE* fp = fopen( "currentgame.xml", "r" );

	if ( fp ) {
		fseek( fp, 0, SEEK_END );
		long len = ftell( fp );
		fseek( fp, 0, SEEK_SET );

		char* mem = new char[len+1];
		fread( mem, 1, len, fp );
		fclose( fp );

		mem[len] = 0;
	
		data += mem;
		delete [] mem;
	}
	else {
		data += "none";
	}

    // Send a Request.
	// BLACKEST VOODOO. Getting this to work: http://social.msdn.microsoft.com/Forums/en/vcgeneral/thread/917e9b99-4b8e-4173-99ad-001fec6a59e2
	//
	LPCWSTR additionalHeaders = L"Content-Type: application/x-www-form-urlencoded\r\n";
	DWORD hLen   = -1;

    bResults = WinHttpSendRequest( hRequest, 
                                   additionalHeaders,
                                   hLen, 
								   (LPVOID)data.c_str(),
								   data.size(), 
                                   data.size(), 
								   0 );

    // Report errors.
    if (!bResults) {
        GLOUTPUT(("Error %d has occurred.\n",GetLastError()));
	}

	// If we close the handles too soon, it seems like the requests fails. Even though this is being run synchronously...
	Sleep( 1000 );

    // Close open handles.
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

}
