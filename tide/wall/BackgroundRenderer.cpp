/*********************************************************************/
/* Copyright (c) 2017-2018, EPFL/Blue Brain Project                  */
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

#include "BackgroundRenderer.h"

#include "ContentWindowRenderer.h"
#include "DataProvider.h"
#include "VisibilityHelper.h"
#include "WallRenderContext.h"
#include "geometry.h"
#include "qmlUtils.h"
#include "scene/Background.h"
#include "scene/DisplayGroup.h"

BackgroundRenderer::BackgroundRenderer(const Background& background,
                                       const WallRenderContext& context,
                                       QQuickItem& parentItem)
{
    auto content = background.getContent()->clone();
    const auto& uuid = background.getContentUUID();
    auto window = std::make_shared<ContentWindow>(std::move(content), uuid);

    const auto wallRect = QRect{QPoint(), context.wallSize};
    window->setCoordinates(geometry::adjustAndCenter(*window, wallRect));
    auto sync = context.provider.createSynchronizer(*window, context.view);

    _renderer.reset(
        new ContentWindowRenderer(std::move(sync), window, parentItem,
                                  context.engine.rootContext(), true));

    DisplayGroup emptyGroup(context.screenRect.size());
    const VisibilityHelper helper(emptyGroup, context.screenRect);
    _renderer->update(window, helper.getVisibleArea(*window));
}

BackgroundRenderer::~BackgroundRenderer()
{
}
