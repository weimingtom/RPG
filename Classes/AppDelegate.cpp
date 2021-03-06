#include "AppDelegate.h"
#include "SceneMain.h"
#include "StateLoading.h"
#include "SimpleAudioEngine.h"
#include "AndroidInterface.h"
using namespace CocosDenshion;

USING_NS_CC;

AppDelegate::AppDelegate() {

}

AppDelegate::~AppDelegate() 
{
}

extern int g_frame_rate;

bool AppDelegate::applicationDidFinishLaunching() {
    // initialize director
    CCDirector* pDirector = CCDirector::sharedDirector();
    CCEGLView* pEGLView = CCEGLView::sharedOpenGLView();
	
    pDirector->setOpenGLView(pEGLView);
	pEGLView->setDesignResolutionSize(854,480,kResolutionShowAll);
	
    // turn on display FPS
    pDirector->setDisplayStats(true);

    // set FPS. the default value is 1.0/60 if you don't call this
    pDirector->setAnimationInterval(1.0 / 60);

    // create a scene. it's an autorelease object
    CCScene *pScene = StateLoading::scene();

    // run
    pDirector->runWithScene(pScene);

	pDirector->getKeypadDispatcher()->addDelegate(KeyDelegate::getInstance());

    return true;
}

// This function will be called when the app is inactive. When comes a phone call,it's be invoked too
void AppDelegate::applicationDidEnterBackground() {
    CCDirector::sharedDirector()->stopAnimation();

    // if you use SimpleAudioEngine, it must be pause
     SimpleAudioEngine::sharedEngine()->pauseBackgroundMusic();
}

// this function will be called when the app is active again
void AppDelegate::applicationWillEnterForeground() {
    CCDirector::sharedDirector()->startAnimation();

    // if you use SimpleAudioEngine, it must resume here
     SimpleAudioEngine::sharedEngine()->resumeBackgroundMusic();
}



void KeyDelegate::keyBackClicked()
{
	exit_game();
}

void KeyDelegate::keyMenuClicked()
{

}

KeyDelegate* KeyDelegate::getInstance()
{
	static KeyDelegate instance;
	return &instance;
}

KeyDelegate::KeyDelegate()
{

}

KeyDelegate::~KeyDelegate()
{

}
