/*
 Copyright (c) 2010-2012, Paul Houx - All rights reserved.
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

#include "cinder/AxisAlignedBox.h"	
#include "cinder/Filesystem.h"
#include "cinder/Frustum.h"
#include "cinder/ImageIo.h"
#include "cinder/MayaCamUI.h"
#include "cinder/ObjLoader.h"
#include "cinder/Rand.h"
#include "cinder/Text.h"
#include "cinder/app/AppBasic.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Light.h"
#include "cinder/gl/Material.h"

#include "CullableObject.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class FrustumCullingApp 
	: public AppBasic 
{
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void mouseDown( MouseEvent event );
	void mouseDrag( MouseEvent event );

	void keyDown( KeyEvent event );

	void resize();
  protected:
	//! load the heart shaped mesh 
	void			loadObject();
	//! draws a grid to visualize the ground plane
	void			drawGrid(float size=100.0f, float step=10.0f);
	//! renders the help text
	void			renderHelpToTexture();
  protected:
	static const int NUM_OBJECTS = 1500;

	//! keep track of time
	double			mCurrentSeconds;

	//! flags
	bool			mIsCullingEnabled;
	bool			mIsCameraEnabled;
	bool			mDrawEstimatedBoundingBoxes;
	bool			mDrawPreciseBoundingBoxes;
	bool			mDrawWireframes;
	bool			mIsHelpVisible;
	
	//! assets
	ci::TriMesh		mTriMesh;
	gl::VboMesh		mVboMesh;

	//! caches the heart's bounding box in object space coordinates
	AxisAlignedBox3f	mObjectBoundingBox;

	//! render assets
	gl::GlslProg	mShader;
	gl::Material	mMaterial;

	//! objects
	CullableObjectRef	mObjects[NUM_OBJECTS];

	//! camera
	MayaCamUI		mMayaCam;
	CameraPersp		mRenderCam;
	CameraPersp		mCullingCam;

	//! help text
	gl::Texture		mHelp;
};

void FrustumCullingApp::prepareSettings(Settings *settings)
{
	//! setup our window
	settings->setWindowSize(1200, 675);
	settings->setTitle("Frustum Culling Redux");
	//! set the frame rate to something very high, so we can
	//! easily see the effect frustum culling has on performance.
	//! Disable vertical sync to achieve the actual frame rate.
	settings->setFrameRate(300);
}

void FrustumCullingApp::setup()
{
	//! intialize settings
	mIsCullingEnabled = true;
	mIsCameraEnabled = true;
	mDrawEstimatedBoundingBoxes = false;
	mDrawPreciseBoundingBoxes = false;
	mDrawWireframes = false;
	mIsHelpVisible = true;

	//! render help texture
	renderHelpToTexture();

	//! load assets
	loadObject();

	//! create a few hearts
	Rand::randomize();
	for(int i=0;i<NUM_OBJECTS;++i) {
		Vec3f p( Rand::randFloat(-2000.0f, 2000.0f), 0.0f, Rand::randFloat(-2000.0f, 2000.0f) );
		Vec3f r( 0.0f, Rand::randFloat(-360.0f, 360.0f), 0.0f );
		Vec3f s( 50.0f, 50.0f, 50.0f );

		mObjects[i] = CullableObjectRef( new CullableObject(mVboMesh) );
		mObjects[i]->setTransform( p, r, s );
	}

	//! setup material
	mMaterial.setAmbient( Color(0.1f, 0.1f, 0.1f) );
	mMaterial.setDiffuse( Color(1.0f, 0.0f, 0.0f) );
	mMaterial.setSpecular( Color(1.0f, 1.0f, 1.0f) );
	mMaterial.setShininess( 50.0f );

	//! load and compile shader
	try {
		mShader = gl::GlslProg( loadAsset("shaders/phong.vert"), loadAsset("shaders/phong.frag") );
	}
	catch( const std::exception &e ) {
		app::console() << "Could not load and compile shader:" << e.what() << std::endl;
	}

	//! setup cameras
	mRenderCam = CameraPersp(getWindowWidth(), getWindowHeight(), 60.0f, 50.0f, 10000.0f);
	mRenderCam.setEyePoint( Vec3f(200.0f, 200.0f, 200.0f) );
	mRenderCam.setCenterOfInterestPoint( Vec3f(0.0f, 0.0f, 0.0f) );
	mMayaCam.setCurrentCam( mRenderCam );

	mCullingCam = mRenderCam;

	//! track current time so we can calculate elapsed time
	mCurrentSeconds = getElapsedSeconds();
}

void FrustumCullingApp::update()
{
	//! calculate elapsed time
	double elapsed = getElapsedSeconds() - mCurrentSeconds;
	mCurrentSeconds += elapsed;

	//! update culling camera (press SPACE to toggle mIsCameraEnabled)
	if(mIsCameraEnabled) 
		mCullingCam = mRenderCam;

	//! perform frustum culling **********************************************************************************
	Frustumf visibleWorld( mCullingCam );

	for(int i=0;i<NUM_OBJECTS;++i) {
		// update object (so it rotates slowly around its axis)
		mObjects[i]->update(elapsed);

		if(mIsCullingEnabled) {
			// create a fast approximation of the world space bounding box by transforming the
			// eight corners of the object space bounding box and using them to create a new axis aligned bounding box 
			AxisAlignedBox3f worldBoundingBox = mObjectBoundingBox.transformed( mObjects[i]->getTransform() );

			// check if the bounding box intersects the visible world
			mObjects[i]->setCulled( ! visibleWorld.intersects( worldBoundingBox ) );
		}
		else {
			mObjects[i]->setCulled( false );
		}
	}
	//! **********************************************************************************************************
}

void FrustumCullingApp::draw()
{
	// clear the window
	gl::clear( Color(0.25f, 0.25f, 0.25f) );

	// setup camera
	gl::pushMatrices();
	gl::setMatrices( mRenderCam );	

	// enable 3D rendering
	gl::enable(GL_CULL_FACE);
	gl::enableDepthRead();
	gl::enableDepthWrite();

	// draw wireframes if enabled
	if( mDrawWireframes ) {
		gl::color( Color::white() );
		gl::enableWireframe();
		for(int i=0;i<NUM_OBJECTS;++i) 
			mObjects[i]->draw();
		gl::disableWireframe();
	}

	// apply material and setup light
	mMaterial.apply();

	gl::Light light(GL_POINT, GL_LIGHT0);
	light.setAmbient( Color::white() );
	light.setDiffuse( Color::white() );
	light.setSpecular( Color::white() );
	light.enable();

	// bind shader 
	if(mShader) {
		mShader.bind();
		mShader.uniform("numLights", 1);
	}

	// draw hearts
	for(int i=0;i<NUM_OBJECTS;++i) 
		mObjects[i]->draw();

	// unbind shader
	if(mShader)
		mShader.unbind();

	// draw helper objects
	gl::drawCoordinateFrame(100.0f, 5.0f, 2.0f);
	
	AxisAlignedBox3f worldBoundingBox;
	for(int i=0;i<NUM_OBJECTS;++i) {
		if(mDrawEstimatedBoundingBoxes) {
			// create a fast approximation of the world space bounding box by transforming the
			// eight corners and using them to create a new axis aligned bounding box 
			worldBoundingBox = mObjectBoundingBox.transformed( mObjects[i]->getTransform() );
		
			if( !mObjects[i]->isCulled() ) 
				gl::color( Color(0, 1, 1) );
			else 
				gl::color( Color(1, 0.5f, 0) );

			gl::drawStrokedCube( worldBoundingBox );
		}

		if(mDrawPreciseBoundingBoxes && !mObjects[i]->isCulled()) {
			// you can see how much the approximated bounding boxes differ
			// from the precise ones by enabling this code
			worldBoundingBox = mTriMesh.calcBoundingBox( mObjects[i]->getTransform() );
			gl::color( Color(1, 1, 0) );
			gl::drawStrokedCube( worldBoundingBox );	
		}
	}

	if(!mIsCameraEnabled) {
		gl::color( Color(1, 1, 1) );
		gl::drawFrustum( mCullingCam );
	}

	drawGrid(2000.0f, 25.0f);	

	// disable 3D rendering
	gl::disableDepthWrite();
	gl::disableDepthRead();
	gl::disable(GL_CULL_FACE);

	// restore matrices
	gl::popMatrices();

	// render help
	if(mIsHelpVisible && mHelp) {
		gl::enableAlphaBlending();
		gl::color( Color::white() );
		gl::draw( mHelp );
		gl::disableAlphaBlending();
	}
}

void FrustumCullingApp::mouseDown( MouseEvent event )
{
	mMayaCam.mouseDown( event.getPos() );
}

void FrustumCullingApp::mouseDrag( MouseEvent event )
{
	mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
	mRenderCam = mMayaCam.getCamera();
}

void FrustumCullingApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_SPACE:
		mIsCameraEnabled = !mIsCameraEnabled;
		break;
	case KeyEvent::KEY_b:
		if(event.isShiftDown())
			mDrawPreciseBoundingBoxes = ! mDrawPreciseBoundingBoxes;
		else
			mDrawEstimatedBoundingBoxes = ! mDrawEstimatedBoundingBoxes;
		break;
	case KeyEvent::KEY_c:
		mIsCullingEnabled = !mIsCullingEnabled;
		break;
	case KeyEvent::KEY_f: 
		{
			bool verticalSyncEnabled = gl::isVerticalSyncEnabled();
			setFullScreen( !isFullScreen() );
			if(!verticalSyncEnabled) {
				// disable vertical sync again
				gl::disableVerticalSync();
			}
		}
		break;
	case KeyEvent::KEY_h:
		mIsHelpVisible = !mIsHelpVisible;
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( ! gl::isVerticalSyncEnabled() );
		break;
	case KeyEvent::KEY_w:
		mDrawWireframes = !mDrawWireframes;
		break;
	}
	
	// update info
	renderHelpToTexture();
}

void FrustumCullingApp::resize()
{
}

void FrustumCullingApp::loadObject()
{
	fs::path meshfile = getAssetPath("") / "models" / "heart.msh";
	if( fs::exists( meshfile ) )
	{
		// load the binary mesh file, which is much faster than an OBJ file
		mTriMesh.read( loadFile( meshfile ) );
	}
	else
	{
		// let's create the binary mesh file
		ObjLoader loader( loadAsset("models/heart.obj") );
		loader.load( &mTriMesh, true, true, true );

		mTriMesh.write( writeFile( meshfile ) );
	}

	// create a Vbo, which greatly improves drawing speed
	ci::gl::VboMesh::Layout layout;
	layout.setStaticPositions();
	layout.setStaticNormals();
	layout.setStaticTexCoords2d();
	layout.setStaticIndices();

	mVboMesh = ci::gl::VboMesh(mTriMesh, layout);

	// find the object space bounding box
	mObjectBoundingBox = mTriMesh.calcBoundingBox();
}

void FrustumCullingApp::drawGrid(float size, float step)
{
	gl::color( Colorf(0.2f, 0.2f, 0.2f) );
	for(float i=-size;i<=size;i+=step) {
		gl::drawLine( Vec3f(i, 0.0f, -size), Vec3f(i, 0.0f, size) );
		gl::drawLine( Vec3f(-size, 0.0f, i), Vec3f(size, 0.0f, i) );
	}
}

void FrustumCullingApp::renderHelpToTexture()
{
	TextLayout layout;
	layout.setFont( Font("Arial", 18) );
	layout.setColor( ColorA(1.0f, 1.0f, 1.0f, 1.0f) );
	layout.setLeadingOffset(3.0f);

	layout.clear( ColorA(0.25f, 0.25f, 0.25f, 0.5f) );

	if(mIsCullingEnabled) layout.addLine("(C) Toggle culling (currently ON)");
	else  layout.addLine("(C) Toggle culling (currently OFF)");

	if(mDrawEstimatedBoundingBoxes) layout.addLine("(B) Toggle estimated bounding boxes (currently ON)");
	else  layout.addLine("(B) Toggle estimated bounding boxes (currently OFF)");

	if(mDrawPreciseBoundingBoxes) layout.addLine("(B)+(Shift) Toggle precise bounding boxes (currently ON)");
	else  layout.addLine("(B)+(Shift) Toggle precise bounding boxes (currently OFF)");

	if( gl::isVerticalSyncEnabled() ) layout.addLine("(V) Toggle vertical sync (currently ON)");
	else  layout.addLine("(V) Toggle vertical sync (currently OFF)");

	if(mIsCameraEnabled) layout.addLine("(Space) Toggle camera control (currently ON)");
	else  layout.addLine("(Space) Toggle camera control (currently OFF)");

	layout.addLine("(H) Toggle this help panel");

	mHelp = gl::Texture( layout.render(true, false) );
}

CINDER_APP_BASIC( FrustumCullingApp, RendererGl )