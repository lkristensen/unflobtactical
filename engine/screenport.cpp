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

#include "screenport.h"
#include "gpustatemanager.h"
#include "enginelimits.h"

#include "../grinliz/glrectangle.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glmatrix.h"

#include "../game/gamelimits.h"

using namespace grinliz;


Screenport::Screenport( int w, int h, int r )
{
	Resize( w, h, r );
	uiMode = false;
	clipInUI2D = Rectangle2F( 0, 0, UIWidth(), UIHeight() );
	clipInUI3D = Rectangle2F( 0, 0, UIWidth(), UIHeight() );
}


void Screenport::Resize( int w, int h, int r )
{
	physicalWidth  = (float)w;
	physicalHeight = (float)h;
	rotation =	r;
	GLASSERT( rotation >= 0 && rotation < 4 );

	GPUShader::SetViewport( w, h );
	GPUShader::SetScissor( 0, 0, 0, 0 );

	// Sad but true: the game assets are set up for 480x320 resolution.
	// How to scale?
	//   1. Scale the smallest axis to be within 320x480. But then buttons get
	//      bigger and all layout has to be programmatic.
	//   2. Clip to a window. Seems a waste to lose that space.
	//   3. Fix UI Height at 320. Stretch backgrounds. That looks weird...but
	//		the background images can be patched up.
	// Try #3.

	if ( (rotation&1) == 0 ) {
		screenHeight = GAME_VIRTUAL_SCREEN_HEIGHT();
		screenWidth = screenHeight * physicalWidth / physicalHeight;
	}
	else {
		screenWidth  = GAME_VIRTUAL_SCREEN_HEIGHT();
		screenHeight = screenWidth * physicalHeight / physicalWidth;
	}

	GLOUTPUT(( "Screenport::Resize physical=(%.1f,%.1f) view=(%.1f,%.1f) rotation=%d\n", physicalWidth, physicalHeight, screenWidth, screenHeight, r ));
}


void Screenport::SetUI( const Rectangle2I* clip )	
{
	if ( clip && clip->Area() > 1 ) {
		clipInUI2D = Rectangle2F( (float)clip->min.x, (float)clip->min.y, (float)clip->max.x, (float)clip->max.y );
	}
	else {
		clipInUI2D = Rectangle2F( 0, 0, UIWidth(), UIHeight() );
	}
	GLASSERT( clipInUI2D.IsValid() );
	GLASSERT( clipInUI2D.min.x >= 0 && clipInUI2D.max.x <= UIWidth() );
	GLASSERT( clipInUI2D.min.y >= 0 && clipInUI2D.max.y <= UIHeight() );

	Rectangle2F scissor;
	UIToWindow( clipInUI2D, &scissor );

	Rectangle2I clean;
	CleanScissor( scissor, &clean );
	GPUShader::SetScissor(  clean.min.x, clean.min.y, clean.Width(), clean.Height() );

	//view2D.SetIdentity();
	
	projection2D.SetIdentity();
	projection2D.SetOrtho( 0, screenWidth, screenHeight, 0, -1, 1 );
	GPUShader::SetOrthoTransform( (int)screenWidth, (int)screenHeight, Rotation()*90 );
	uiMode = true;
}


void Screenport::SetView( const Matrix4& _view )
{
	GLASSERT( uiMode == false );
	view3D = _view;
	GPUShader::SetCameraTransform( view3D );
}


void Screenport::SetPerspective( const grinliz::Rectangle2I* clip )
{
	uiMode = false;

	if ( clip && clip->Area() > 1 ) {
		clipInUI3D = Rectangle2F( (float)clip->min.x, (float)clip->min.y, (float)clip->max.x, (float)clip->max.y );
	}
	else {
		clipInUI3D = Rectangle2F( 0, 0, UIWidth(), UIHeight() );
	}
	GLASSERT( clipInUI3D.IsValid() );
	GLASSERT( clipInUI3D.min.x >= 0 && clipInUI3D.max.x <= UIWidth() );
	GLASSERT( clipInUI3D.min.y >= 0 && clipInUI3D.max.y <= UIHeight() );
	
	Rectangle2F scissor;
	UIToWindow( clipInUI3D,  &scissor );

	Rectangle2I clean;
	CleanScissor( scissor, &clean );
	GPUShader::SetScissor(  clean.min.x, clean.min.y, clean.Width(), clean.Height() );

	GLASSERT( uiMode == false );
	GLASSERT( EL_NEAR > 0.0f );
	GLASSERT( EL_FAR > EL_NEAR );

	frustum.zNear = EL_NEAR;
	frustum.zFar  = EL_FAR;

	// Convert from the FOV to the half angle.
	float theta = ToRadian( EL_FOV * 0.5f );
	float tanTheta = tanf( theta );
	float halfLongSide = tanTheta * frustum.zNear;

	// left, right, top, & bottom are on the near clipping
	// plane. (Not an obvious point to my mind.)

	// Also, the 3D camera applies the rotation.
	
	if ( Rotation() & 1 ) {
		float ratio = (float)clipInUI3D.Height() / (float)clipInUI3D.Width();
		
		// frustum is in original screen coordinates.
		frustum.top		=  halfLongSide;
		frustum.bottom	= -halfLongSide;
		
		frustum.left	= -ratio * halfLongSide;
		frustum.right	=  ratio * halfLongSide;
	}
	else {
		// Since FOV is specified as the 1/2 width, the ratio
		// is the height/width (different than gluPerspective)
		float ratio = (float)clipInUI3D.Height() / (float)clipInUI3D.Width();

		frustum.top		= ratio * halfLongSide;
		frustum.bottom	= -frustum.top;

		frustum.left	= -halfLongSide;
		frustum.right	=  halfLongSide;
	}
	
	Matrix4 rot;
	rot.SetZRotation( (float)(-90 * Rotation()) );
	
	// In normalized coordinates.
	projection3D.SetFrustum( frustum.left, frustum.right, frustum.bottom, frustum.top, frustum.zNear, frustum.zFar );
	projection3D = projection3D * rot;
	GPUShader::SetPerspectiveTransform( frustum.left, frustum.right,
										frustum.bottom, frustum.top,	
										frustum.zNear, frustum.zFar,
										(90*Rotation()) );
}


void Screenport::ViewToUI( const grinliz::Vector2F& view, grinliz::Vector2F* ui ) const
{
	switch ( rotation ) {
		case 0:	
			ui->x = view.x;
			ui->y = screenHeight-view.y;
			break;

		case 1:
			ui->x = screenHeight - view.y;
			ui->y = screenWidth - view.x;
			break;

		default:
			GLASSERT( 0 );
			break;
	}
}


void Screenport::UIToView( const grinliz::Vector2F& ui, grinliz::Vector2F* view ) const
{
	switch ( rotation ) {
		case 0:
			view->x = ui.x;
			view->y = screenHeight-ui.y;
			break;
		case 1:
			view->x = screenWidth - ui.y;
			view->y = screenHeight - ui.x;
			break;
		default:
			GLASSERT( 0 );
			break;
	}
}


bool Screenport::ViewToWorld( const grinliz::Vector2F& v, const grinliz::Matrix4* _mvpi, grinliz::Ray* ray ) const
{
	Matrix4 mvpi;
	if ( _mvpi ) {
		mvpi = *_mvpi;
	}
	else {
		Matrix4 mvp;
		ViewProjection3D( &mvp );
		mvp.Invert( &mvpi );
	}

	// View normalized:
	Vector4F in = { 2.0f * v.x / screenWidth - 1.0f,
					2.0f * v.y / screenHeight - 1.0f,
					0.f, //v.z*2.0f-1.f,
					1.0f };

	Vector4F out0, out1;
	MultMatrix4( mvpi, in, &out0 );
	in.z = 1.0f;
	MultMatrix4( mvpi, in, &out1 );

	if ( out0.w == 0.0f ) {
		return false;
	}
	ray->origin.x = out0.x / out0.w;
	ray->origin.y = out0.y / out0.w;
	ray->origin.z = out0.z / out0.w;

	ray->direction.x = out1.x / out1.w - ray->origin.x;
	ray->direction.y = out1.y / out1.w - ray->origin.y;
	ray->direction.z = out1.z / out1.w - ray->origin.z;

	return true;
}


void Screenport::WorldToView( const grinliz::Vector3F& world, grinliz::Vector2F* v ) const
{
	Matrix4 mvp;
	ViewProjection3D( &mvp );

	Vector4F p, r;
	p.Set( world, 1 );
	r = mvp * p;

	Vector2F v0, v1;
	Rectangle2F clipInView;
	Vector2F min = { 0, 0 };
	Vector2F max = { UIWidth(), UIHeight() };
	UIToView( min, &v0 );
	UIToView( max, &v1 );

	clipInView.FromPair( v0.x, v0.y, v1.x, v1.y );
	
	v->x = Interpolate( -1.0f, (float)clipInView.min.x,
						1.0f,  (float)clipInView.max.x,
						r.x/r.w );
	v->y = Interpolate( -1.0f, (float)clipInView.min.y,
						1.0f,  (float)clipInView.max.y,
						r.y/r.w );
}


void Screenport::ViewToWindow( const Vector2F& view, Vector2F* window ) const
{
	window->x = view.x * physicalWidth  / screenWidth;
	window->y = view.y * physicalHeight / screenHeight;
}


void Screenport::WindowToView( const Vector2F& window, Vector2F* view ) const
{
	view->x = window.x * screenWidth / physicalWidth;
	view->y = window.y * screenHeight / physicalHeight;
}


void Screenport::UIToWindow( const grinliz::Rectangle2F& ui, grinliz::Rectangle2F* clip ) const
{	
	Vector2F v;
	Vector2F w;
	
	UIToView( ui.min, &v );
	ViewToWindow( v, &w );
	clip->min = clip->max = w;

	UIToView( ui.max, &v );
	ViewToWindow( v, &w );
	clip->DoUnion( w );
}


void Screenport::CleanScissor( const grinliz::Rectangle2F& scissor, grinliz::Rectangle2I* clean )
{
	if ( scissor.min.x == 0 && scissor.min.y == 0 && scissor.max.x == physicalWidth && scissor.max.y == physicalHeight ) {
		clean->min.Set( 0, 0 );
		clean->max.Set( (int)physicalWidth, (int)physicalHeight );
		return;
	}

	clean->min.x = (int)scissor.min.x;
	clean->max.x = (int)scissor.max.x;
	if ( abs( clean->max.x - physicalWidth ) < 4 ) {
		clean->max.x = (int)physicalWidth;
	}
	clean->min.y = (int)scissor.min.y;
	clean->max.y = (int)scissor.max.y;
	if ( abs( clean->max.y - physicalHeight ) < 4 ) {
		clean->max.y = (int)physicalHeight;
	}
}
