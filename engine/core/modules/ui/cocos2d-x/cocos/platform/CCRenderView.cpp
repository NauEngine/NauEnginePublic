/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "platform/CCRenderView.h"

#include "base/CCTouch.h"
#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"
#include "2d/CCCamera.h"
#include "2d/CCScene.h"
#include "renderer/CCRenderer.h"

NS_CC_BEGIN

namespace {
    
    static Touch* g_touches[EventTouch::MAX_TOUCHES] = { nullptr };
    static unsigned int g_indexBitsUsed = 0;
    // System touch pointer ID (It may not be ascending order number) <-> Ascending order number from 0
    static std::map<intptr_t, int> g_touchIdReorderMap;
    
    static int getUnUsedIndex()
    {
        int i;
        int temp = g_indexBitsUsed;
        
        for (i = 0; i < EventTouch::MAX_TOUCHES; i++) {
            if (! (temp & 0x00000001)) {
                g_indexBitsUsed |= (1 <<  i);
                return i;
            }
            
            temp >>= 1;
        }
        
        // all bits are used
        return -1;
    }
    
    static std::vector<Touch*> getAllTouchesVector()
    {
        std::vector<Touch*> ret;
        int i;
        int temp = g_indexBitsUsed;
        
        for (i = 0; i < EventTouch::MAX_TOUCHES; i++) {
            if ( temp & 0x00000001) {
                ret.push_back(g_touches[i]);
            }
            temp >>= 1;
        }
        return ret;
    }
    
    static void removeUsedIndexBit(int index)
    {
        if (index < 0 || index >= EventTouch::MAX_TOUCHES)
        {
            return;
        }
        
        unsigned int temp = 1 << index;
        temp = ~temp;
        g_indexBitsUsed &= temp;
    }
    
}

//default context attributions are set as follows
RenderContextAttrs RenderView::_renderContextAttrs = {8, 8, 8, 8, 24, 8, 0};

void RenderView::setRenderContextAttrs(RenderContextAttrs& renderContextAttrs)
{
    _renderContextAttrs = renderContextAttrs;
}

RenderContextAttrs RenderView::getRenderContextAttrs()
{
    return _renderContextAttrs;
}

RenderView::RenderView()
: _screenSize(0,0)
, _designResolutionSize(0,0)
, _scaleX(1.0f)
, _scaleY(1.0f)
, _resolutionPolicy(ResolutionPolicy::UNKNOWN)
{
}

RenderView::~RenderView()
{

}

void RenderView::pollEvents()
{
}

void RenderView::updateDesignResolutionSize()
{
    if (_screenSize.width > 0 && _screenSize.height > 0
        && _designResolutionSize.width > 0 && _designResolutionSize.height > 0)
    {
        _scaleX = (float)_screenSize.width / _designResolutionSize.width;
        _scaleY = (float)_screenSize.height / _designResolutionSize.height;
        
        if (_resolutionPolicy == ResolutionPolicy::NO_BORDER)
        {
            _scaleX = _scaleY = MAX(_scaleX, _scaleY);
        }
        
        else if (_resolutionPolicy == ResolutionPolicy::SHOW_ALL)
        {
            _scaleX = _scaleY = MIN(_scaleX, _scaleY);
        }
        
        else if ( _resolutionPolicy == ResolutionPolicy::FIXED_HEIGHT) {
            _scaleX = _scaleY;
            _designResolutionSize.width = ceilf(_screenSize.width/_scaleX);
        }
        
        else if ( _resolutionPolicy == ResolutionPolicy::FIXED_WIDTH) {
            _scaleY = _scaleX;
            _designResolutionSize.height = ceilf(_screenSize.height/_scaleY);
        }
        
        // calculate the rect of viewport
        float viewPortW = _designResolutionSize.width * _scaleX;
        float viewPortH = _designResolutionSize.height * _scaleY;
        
        _viewPortRect.setRect((_screenSize.width - viewPortW) / 2, (_screenSize.height - viewPortH) / 2, viewPortW, viewPortH);


        // reset director's member variables to fit visible rect
        auto director = Director::getInstance();
        director->_winSizeInPoints = getDesignResolutionSize();
        director->_isStatusLabelUpdated = true;
        director->setProjection(director->getProjection());

        // Github issue #16139
        // A default viewport is needed in order to display the FPS,
        // since the FPS are rendered in the Director, and there is no viewport there.
        // Everything, including the FPS should renderer in the Scene.
        //TODO: minggo
//        glViewport(0, 0, _screenSize.width, _screenSize.height);
    }
}

void RenderView::setDesignResolutionSize(float width, float height, ResolutionPolicy resolutionPolicy)
{
    CCASSERT(resolutionPolicy != ResolutionPolicy::UNKNOWN, "should set resolutionPolicy");
    
    if (width == 0.0f || height == 0.0f)
    {
        return;
    }

    _designResolutionSize.setSize(width, height);
    _resolutionPolicy = resolutionPolicy;
    
    updateDesignResolutionSize();
 }

const Size& RenderView::getDesignResolutionSize() const 
{
    return _designResolutionSize;
}

Size RenderView::getFrameSize() const
{
    return _screenSize;
}

void RenderView::setFrameSize(float width, float height)
{
    _screenSize = Size(width, height);

    // Github issue #16003 and #16485
    // only update the designResolution if it wasn't previously set
    if (_designResolutionSize.equals(Size::ZERO))
        _designResolutionSize = _screenSize;
}

Rect RenderView::getVisibleRect() const
{
    Rect ret;
    ret.size = getVisibleSize();
    ret.origin = getVisibleOrigin();
    return ret;
}

Rect RenderView::getSafeAreaRect() const
{
    return getVisibleRect();
}

Size RenderView::getVisibleSize() const
{
    if (_resolutionPolicy == ResolutionPolicy::NO_BORDER)
    {
        return Size(_screenSize.width/_scaleX, _screenSize.height/_scaleY);
    }
    else 
    {
        return _designResolutionSize;
    }
}

Vec2 RenderView::getVisibleOrigin() const
{
    if (_resolutionPolicy == ResolutionPolicy::NO_BORDER)
    {
        return Vec2((_designResolutionSize.width - _screenSize.width/_scaleX)/2, 
                           (_designResolutionSize.height - _screenSize.height/_scaleY)/2);
    }
    else 
    {
        return Vec2::ZERO;
    }
}

void RenderView::setViewPortInPoints(float x , float y , float w , float h)
{
    Viewport vp;
    vp.x = (int)(x * _scaleX + _viewPortRect.origin.x);
    vp.y = (int)(y * _scaleY + _viewPortRect.origin.y);
    vp.w = (unsigned int)(w * _scaleX);
    vp.h = (unsigned int)(h * _scaleY);
    Camera::setDefaultViewport(vp);
}

void RenderView::setScissorInPoints(float x , float y , float w , float h)
{
    auto renderer = Director::getInstance()->getRenderer();
    renderer->setScissorRect((int)(x * _scaleX + _viewPortRect.origin.x),
                             (int)(y * _scaleY + _viewPortRect.origin.y),
                             (unsigned int)(w * _scaleX),
                             (unsigned int)(h * _scaleY));
}

bool RenderView::isScissorEnabled()
{
    auto renderer = Director::getInstance()->getRenderer();
    return renderer->getScissorTest();
}

Rect RenderView::getScissorRect() const
{
    auto renderer = Director::getInstance()->getRenderer();
    auto& rect = renderer->getScissorRect();

    float x = (rect.x - _viewPortRect.origin.x) / _scaleX;
    float y = (rect.y- _viewPortRect.origin.y) / _scaleY;
    float w = rect.width/ _scaleX;
    float h = rect.height / _scaleY;
    return Rect(x, y, w, h);
}

void RenderView::setViewName(const std::string& viewname )
{
    _viewName = viewname;
}

const std::string& RenderView::getViewName() const
{
    return _viewName;
}

void RenderView::handleTouchesBegin(int num, intptr_t ids[], float xs[], float ys[])
{
    intptr_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
    int unusedIndex = 0;
    EventTouch touchEvent;
    
    for (int i = 0; i < num; ++i)
    {
        id = ids[i];
        x = xs[i];
        y = ys[i];

        auto iter = g_touchIdReorderMap.find(id);

        // it is a new touch
        if (iter == g_touchIdReorderMap.end())
        {
            unusedIndex = getUnUsedIndex();

            // The touches is more than MAX_TOUCHES ?
            if (unusedIndex == -1) {
                NAU_LOG_INFO("The touches is more than MAX_TOUCHES, unusedIndex = {}", unusedIndex);
                continue;
            }

            Touch* touch = g_touches[unusedIndex] = new (std::nothrow) Touch();
            touch->setTouchInfo(unusedIndex, (x - _viewPortRect.origin.x) / _scaleX,
                                     (y - _viewPortRect.origin.y) / _scaleY);
            
            CCLOGINFO("x = %f y = %f", touch->getLocationInView().x, touch->getLocationInView().y);
            
            g_touchIdReorderMap.emplace(id, unusedIndex);
            touchEvent._touches.push_back(touch);
        }
    }

    if (touchEvent._touches.size() == 0)
    {
        NAU_LOG_INFO("touchesBegan: size = 0");
        return;
    }
    
    touchEvent._eventCode = EventTouch::EventCode::BEGAN;
    auto dispatcher = Director::getInstance()->getEventDispatcher();
    dispatcher->dispatchEvent(&touchEvent);
}

void RenderView::handleTouchesMove(int num, intptr_t ids[], float xs[], float ys[])
{
    handleTouchesMove(num, ids, xs, ys, nullptr, nullptr);
}

void RenderView::handleTouchesMove(int num, intptr_t ids[], float xs[], float ys[], float fs[], float ms[])
{
    intptr_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
    float force = 0.0f;
    float maxForce = 0.0f;
    EventTouch touchEvent;
    
    for (int i = 0; i < num; ++i)
    {
        id = ids[i];
        x = xs[i];
        y = ys[i];
        force = fs ? fs[i] : 0.0f;
        maxForce = ms ? ms[i] : 0.0f;

        auto iter = g_touchIdReorderMap.find(id);
        if (iter == g_touchIdReorderMap.end())
        {
            NAU_LOG_INFO("if the index doesn't exist, it is an error");
            continue;
        }

        CCLOGINFO("Moving touches with id: %d, x=%f, y=%f, force=%f, maxFource=%f", (int)id, x, y, force, maxForce);
        Touch* touch = g_touches[iter->second];
        if (touch)
        {
            touch->setTouchInfo(iter->second, (x - _viewPortRect.origin.x) / _scaleX,
                                (y - _viewPortRect.origin.y) / _scaleY, force, maxForce);
            
            touchEvent._touches.push_back(touch);
        }
        else
        {
            // It is error, should return.
            CCLOG("Moving touches with id: %ld error", (long int)id);
            return;
        }
    }

    if (touchEvent._touches.size() == 0)
    {
        NAU_LOG_INFO("touchesMoved: size = 0");
        return;
    }
    
    touchEvent._eventCode = EventTouch::EventCode::MOVED;
    auto dispatcher = Director::getInstance()->getEventDispatcher();
    dispatcher->dispatchEvent(&touchEvent);
}

void RenderView::handleTouchesOfEndOrCancel(EventTouch::EventCode eventCode, int num, intptr_t ids[], float xs[], float ys[])
{
    intptr_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
    EventTouch touchEvent;
    
    for (int i = 0; i < num; ++i)
    {
        id = ids[i];
        x = xs[i];
        y = ys[i];

        auto iter = g_touchIdReorderMap.find(id);
        if (iter == g_touchIdReorderMap.end())
        {
            NAU_LOG_INFO("if the index doesn't exist, it is an error");
            continue;
        }
        
        /* Add to the set to send to the director */
        Touch* touch = g_touches[iter->second];
        if (touch)
        {
            CCLOGINFO("Ending touches with id: %d, x=%f, y=%f", (int)id, x, y);
            touch->setTouchInfo(iter->second, (x - _viewPortRect.origin.x) / _scaleX,
                                (y - _viewPortRect.origin.y) / _scaleY);

            touchEvent._touches.push_back(touch);
            
            g_touches[iter->second] = nullptr;
            removeUsedIndexBit(iter->second);

            g_touchIdReorderMap.erase(id);
        } 
        else
        {
            CCLOG("Ending touches with id: %ld error", static_cast<long>(id));
            return;
        } 

    }

    if (touchEvent._touches.size() == 0)
    {
        NAU_LOG_INFO("touchesEnded or touchesCancel: size = 0");
        return;
    }
    
    touchEvent._eventCode = eventCode;
    auto dispatcher = Director::getInstance()->getEventDispatcher();
    dispatcher->dispatchEvent(&touchEvent);
    
    for (auto& touch : touchEvent._touches)
    {
        // release the touch object.
        touch->release();
    }
}

void RenderView::handleTouchesEnd(int num, intptr_t ids[], float xs[], float ys[])
{
    handleTouchesOfEndOrCancel(EventTouch::EventCode::ENDED, num, ids, xs, ys);
}

void RenderView::handleTouchesCancel(int num, intptr_t ids[], float xs[], float ys[])
{
    handleTouchesOfEndOrCancel(EventTouch::EventCode::CANCELLED, num, ids, xs, ys);
}

const Rect& RenderView::getViewPortRect() const
{
    return _viewPortRect;
}

std::vector<Touch*> RenderView::getAllTouches() const
{
    return getAllTouchesVector();
}

float RenderView::getScaleX() const
{
    return _scaleX;
}

float RenderView::getScaleY() const
{
    return _scaleY;
}

void RenderView::renderScene(Scene* scene, Renderer* renderer)
{
    CCASSERT(scene, "Invalid Scene");
    CCASSERT(renderer, "Invalid Renderer");

    scene->render(renderer, Mat4::IDENTITY, nullptr);
}

NS_CC_END
