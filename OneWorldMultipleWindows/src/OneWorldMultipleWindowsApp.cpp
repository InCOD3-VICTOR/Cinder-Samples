/*
 Copyright (c) 2014, Paul Houx - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Camera.h"
#include "cinder/Rand.h"

#include "Pistons.h"

using namespace ci;
using namespace ci::app;
using namespace std;

// Our application
class OneWorldMultipleWindowsApp : public AppNative {
public:
	void setup();	
	void update();
	void draw();

	void keyDown( KeyEvent event );
private:
	CameraPersp         mCamera;
	gl::GlslProg        mShader;        
	Pistons				mPistons;
	double              mTime;   
};

void OneWorldMultipleWindowsApp::setup()
{
	mPistons.setup();
}

void OneWorldMultipleWindowsApp::update()
{
	// Note: this function is only called once per frame

	// Keep track of time
	mTime = getElapsedSeconds();

	// Animate our camera
	double t = mTime / 10.0;

	float phi = (float) t;
	float theta = 3.14159265f * (0.25f + 0.2f * math<float>::sin(phi * 0.9f));
	float x = 150.0f * math<float>::cos(phi) * math<float>::cos(theta);
	float y = 150.0f * math<float>::sin(theta);
	float z = 150.0f * math<float>::sin(phi) * math<float>::cos(theta);

	mCamera.setEyePoint( Vec3f(x, y, z) );
	mCamera.setCenterOfInterestPoint( Vec3f(1, 50, 0) );

	// Animate our pistons
	mPistons.update(mCamera);
}

void OneWorldMultipleWindowsApp::draw()
{
	// Note: this function is called once per frame for EACH WINDOW

	// We are going to use the whole display to render our scene.
	Vec2f displaySize( getDisplay()->getSize() );
	Vec2f displayCenter = displaySize * 0.5f;

	// Each window will be literally a window into our scene. This is made easy
	// by the lens shift functions of the camera. We also need to set the correct
	// vertical field of view and of course the aspect ratio of each window.
	Vec2f windowPos( getWindow()->getPos() );
	Vec2f windowSize( getWindow()->getSize() );
	Vec2f windowCenter = windowPos + windowSize * 0.5f;

	float lensShiftX = 2.0f * (windowCenter.x - displayCenter.x) / windowSize.x;
	float lensShiftY = 2.0f * (displayCenter.y - windowCenter.y) / windowSize.y;
	mCamera.setAspectRatio( getWindowAspectRatio() );
	mCamera.setFov( 60.0f * windowSize.y / displaySize.y );
	mCamera.setLensShift( lensShiftX, lensShiftY );

	// Draw our scene. Note: if possible, try to only draw what is visible in
	// your window, by properly culling objects against the camera's view frustum.
	// This is beyond the scope of the sample. See the FrustumCulling sample
	// for more information.

	gl::clear(); 

	mPistons.draw(mCamera, (float)mTime);
}

void OneWorldMultipleWindowsApp::keyDown( KeyEvent event )
{
	switch( event.getCode() )
	{
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	default:
		// Create a new window
		app::WindowRef newWindow = createWindow( Window::Format().size( 400, 300 ) );
		newWindow->setTitle("OneWorldMultipleWindowsApp");
		break;
	}
}

CINDER_APP_NATIVE( OneWorldMultipleWindowsApp, RendererGl( RendererGl::AA_MSAA_4 ) )
