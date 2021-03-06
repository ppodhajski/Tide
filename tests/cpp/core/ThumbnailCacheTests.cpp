/*********************************************************************/
/* Copyright (c) 2017, EPFL/Blue Brain Project                       */
/*                     Pawel Podhajski <pawel.podhajski@epfl.ch>     */
/*                     Raphael Dumusc <raphael.dumusc@epfl.ch>       */
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

#define BOOST_TEST_MODULE ThumbnailCacheTests

#include <boost/test/unit_test.hpp>

#include "rest/ThumbnailCache.h"
#include "scene/ContentFactory.h"
#include "scene/DisplayGroup.h"
#include "thumbnail/thumbnail.h"

#include <rockets/http/response.h>

#include <QBuffer>
#include <QByteArray>

namespace
{
const QString imageUri{"wall.png"};
const QSize thumbnailSize{512, 512};
const QSize wallSize{1000, 1000};

std::string _getTestThumbnail()
{
    const auto image = thumbnail::create(imageUri, thumbnailSize);
    QByteArray imageArray;
    QBuffer buffer(&imageArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();
    return "data:image/png;base64," + imageArray.toBase64().toStdString();
}
}

BOOST_AUTO_TEST_CASE(testWindowInfo)
{
    auto group = DisplayGroup::create(wallSize);
    auto scene = Scene::create(group);

    ThumbnailCache cache{*scene};

    auto content = ContentFactory::getContent(imageUri);
    auto window = std::make_shared<ContentWindow>(std::move(content));
    group->addContentWindow(window);

    // Thumbnail not ready yet
    auto response = cache.getThumbnail(window->getID()).get();
    BOOST_CHECK_EQUAL(response.code, 204);

    // Wait for async thumnbnail generation to finish
    sleep(2);
    response = cache.getThumbnail(window->getID()).get();
    BOOST_CHECK_EQUAL(response.code, 200);
    BOOST_CHECK_EQUAL(response.body, _getTestThumbnail());
    BOOST_CHECK_EQUAL(response.headers[rockets::http::Header::CONTENT_TYPE],
                      "image/png");

    group->removeContentWindow(window);
    response = cache.getThumbnail(window->getID()).get();
    BOOST_CHECK_EQUAL(response.code, 404);
}
