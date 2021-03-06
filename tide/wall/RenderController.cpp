/*********************************************************************/
/* Copyright (c) 2014-2018, EPFL/Blue Brain Project                  */
/*                          Raphael Dumusc <raphael.dumusc@epfl.ch>  */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of Ecole polytechnique federale de Lausanne.          */
/*********************************************************************/

#include "RenderController.h"

#include "DataProvider.h"
#include "DisplayGroupRenderer.h"
#include "WallWindow.h"
#include "network/WallToWallChannel.h"
#include "scene/Background.h"
#include "scene/CountdownStatus.h"
#include "scene/Options.h"
#include "scene/Scene.h"
#include "scene/ScreenLock.h"

RenderController::RenderController(std::vector<WallWindow*> windows,
                                   DataProvider& provider,
                                   WallToWallChannel& wallChannel,
                                   const SwapSync type)
    : _windows{std::move(windows)}
    , _provider{provider}
    , _wallChannel{wallChannel}
    , _syncCountdownStatus{std::make_shared<CountdownStatus>()}
    , _syncScene{Scene::create(QSize())}
    , _syncLock(ScreenLock::create())
    , _syncOptions{Options::create()}
{
    _syncScene.setCallback([this](ScenePtr scene) {
        _provider.updateDataSources(*scene);
        const auto& surfaces = scene->getSurfaces();
        for (auto window : _windows)
        {
            const auto& surface = surfaces[window->getSurfaceIndex()];
            window->setDisplayGroup(surface.getGroupPtr());
            window->setBackground(surface.getBackgroundPtr());
        }
    });
    _syncLock.setCallback([this](ScreenLockPtr lock) {
        for (auto window : _windows)
            window->setScreenLock(lock);
    });
    _syncCountdownStatus.setCallback([this](CountdownStatusPtr status) {
        for (auto window : _windows)
            window->setCountdownStatus(status);
    });
    _syncMarkers.setCallback([this](MarkersPtr markers) {
        for (auto window : _windows)
            window->setMarkers(markers);
    });
    _syncOptions.setCallback([this](OptionsPtr options) {
        for (auto window : _windows)
            window->setRenderOptions(options);
    });

    connect(&_provider, &DataProvider::imageLoaded, this,
            [this] { _needRedraw = true; }, Qt::QueuedConnection);

    for (auto window : _windows)
    {
        connect(window, &WallWindow::imageGrabbed, this,
                &RenderController::screenshotRendered);
    }

    _setupSwapSynchronization(type);
}

void RenderController::_setupSwapSynchronization(const SwapSync type)
{
    _swapSynchronizer =
        SwapSynchronizerFactory::get(type)->create(_wallChannel,
                                                   _windows.size());
    for (auto window : _windows)
        window->setSwapSynchronizer(_swapSynchronizer.get());
}

void RenderController::timerEvent(QTimerEvent* qtEvent)
{
    if (qtEvent->timerId() == _renderTimer)
        _syncAndRender();
    else if (qtEvent->timerId() == _idleRedrawTimer)
        requestRender();
    else if (qtEvent->timerId() == _stopRenderingDelayTimer)
    {
        killTimer(_renderTimer);
        killTimer(_stopRenderingDelayTimer);
        _renderTimer = 0;
        _stopRenderingDelayTimer = 0;

        // Redraw screen every minute so that the on-screen clock is up to date
        if (_idleRedrawTimer == 0)
            _idleRedrawTimer = startTimer(60000 /*ms*/);
    }
}

void RenderController::requestRender()
{
    killTimer(_stopRenderingDelayTimer);
    _stopRenderingDelayTimer = 0;
    killTimer(_idleRedrawTimer);
    _idleRedrawTimer = 0;

    if (_renderTimer == 0)
        _renderTimer = startTimer(5, Qt::PreciseTimer);
}

void RenderController::_syncAndRender()
{
    auto versionCheckFunc = std::bind(&WallToWallChannel::checkVersion,
                                      &_wallChannel, std::placeholders::_1);
    _syncQuit.sync(versionCheckFunc);
    if (_syncQuit.get())
    {
        killTimer(_renderTimer);
        killTimer(_stopRenderingDelayTimer);
        for (auto window : _windows)
            window->deleteLater();
        return;
    }

    _synchronizeObjects(versionCheckFunc);

    const bool grab = _syncScreenshot.get();
    if (grab)
        _syncScreenshot = SwapSyncObject<bool>{false};

    if (_syncAndRenderWindows(grab))
    {
        if (_stopRenderingDelayTimer == 0)
            _stopRenderingDelayTimer = startTimer(5000 /*ms*/);
    }
    else
        requestRender();

    _needRedraw = false;
}

bool RenderController::_syncAndRenderWindows(const bool grab)
{
    _wallChannel.synchronizeClock();

    _provider.synchronizeTilesSwap(_wallChannel);

    for (auto window : _windows)
    {
        if (!window->isInitialized())
            return false;

        window->render(grab);
        _needRedraw = _needRedraw || window->needRedraw();
    }
    return _wallChannel.allReady(!_needRedraw);
}

void RenderController::updateCountdownStatus(CountdownStatusPtr status)
{
    _syncCountdownStatus.update(status);
    requestRender();
}

void RenderController::updateScene(ScenePtr scene)
{
    _syncScene.update(scene);
    requestRender();
}

void RenderController::updateMarkers(MarkersPtr markers)
{
    _syncMarkers.update(markers);
    requestRender();
}

void RenderController::updateLock(ScreenLockPtr lock)
{
    _syncLock.update(lock);
    requestRender();
}

void RenderController::updateOptions(OptionsPtr options)
{
    _syncOptions.update(options);
    requestRender();
}

void RenderController::updateRequestScreenshot()
{
    _syncScreenshot.update(true);
    requestRender();
}

void RenderController::updateQuit()
{
    _syncQuit.update(true);
    requestRender();
}

void RenderController::_synchronizeObjects(const SyncFunction& versionCheckFunc)
{
    _syncCountdownStatus.sync(versionCheckFunc);
    _syncScene.sync(versionCheckFunc);
    _syncLock.sync(versionCheckFunc);
    _syncMarkers.sync(versionCheckFunc);
    _syncOptions.sync(versionCheckFunc);
    _syncScreenshot.sync(versionCheckFunc);
}
