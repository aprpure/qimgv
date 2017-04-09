#include "core.h"

Core::Core() :
    QObject(),
    imageLoader(NULL),
    dirManager(NULL),
    currentImageAnimated(NULL),
    currentVideo(NULL),
    mCurrentIndex(0),
    mPreviousIndex(0),
    mImageCount(0),
    infiniteScrolling(false)
{
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

void Core::init() {
    initVariables();
    connectSlots();
    imageLoader->setCache(cache);
    readSettings();
    connect(settings, SIGNAL(settingsChanged()), this, SLOT(readSettings()));
}

QString Core::getCurrentFilePath() {
    QString filePath = "";
    if(currentImage()) {
        filePath = currentImage()->getPath();
    }
    return filePath;
}

int Core::imageCount() {
    if(!dirManager)
        return 0;
    return dirManager->fileCount();
}

// ##############################################################
// ####################### PUBLIC SLOTS #########################
// ##############################################################

void Core::updateInfoString() {
    Image* img = currentImage();
    QString infoString = "";
    infoString.append("[ " +
                      QString::number(mCurrentIndex + 1) +
                      "/" +
                      QString::number(dirManager->fileCount()) +
                      " ]   ");
    if(img) {
        QString name, fullName = img->info()->fileName();
        if(fullName.size()>95) {
            name = fullName.left(95);
            name.append(" (...) ");
            name.append(fullName.right(12));
        } else {
            name = fullName;
        }
        infoString.append(name + "  ");
        infoString.append("(" +
                          QString::number(img->width()) +
                          "x" +
                          QString::number(img->height()) +
                          "  ");
        infoString.append(QString::number(img->info()->fileSize()) + " KB)");
    }
    emit infoStringChanged(infoString);
}


void Core::initCache() {
        // construct file path list
        QStringList filePaths;
        for(int i = 0; i < dirManager->fileCount(); i++) {
            filePaths << dirManager->filePathAt(i);
        }
        cache->init(dirManager->currentDirectory(), &filePaths);
}

void Core::loadImage(QString filePath, bool blocking) {
    if(dirManager->isImage(filePath)) {
        FileInfo *info = new FileInfo(filePath);
        int index = dirManager->indexOf(info->fileName());
        if(index == -1) {
            dirManager->setDirectory(info->directoryPath());
            index = dirManager->indexOf(info->fileName());
            if(index == -1) {
                qDebug() << "Core: could not open file. This is a bug.";
                return;
            }
        }
        mCurrentIndex = index;
        mImageCount = dirManager->fileCount();
        if(cache->currentDirectory() != dirManager->currentDirectory()) {
            this->initCache();
        }
        stopAnimation();
        if(blocking)
            imageLoader->openBlocking(mCurrentIndex);
        else
            imageLoader->open(mCurrentIndex);
    } else {
        qDebug() << "Core: invalid/missing file.";
    }
}

void Core::loadImage(QString filePath) {
    loadImage(filePath, false);
}

void Core::loadImageBlocking(QString filePath) {
    loadImage(filePath, true);
}

void Core::loadImageByPos(int pos) {
    if(pos >=0 && pos < dirManager->fileCount()) {
        mCurrentIndex = pos;
        stopAnimation();
        imageLoader->open(pos);
    }
    else
        qDebug() << "Core::loadImageByPos - argument out of range.";
}

void Core::slotNextImage() {
    if(dirManager->containsImages()) {
        stopAnimation();
        int nextPos = mCurrentIndex + 1;
        if(nextPos >= dirManager->fileCount()) {
            if(infiniteScrolling)
                nextPos = 0;
            else
                return;
        }
        mCurrentIndex = nextPos;
        imageLoader->open(nextPos);
        if(dirManager->checkRange(nextPos + 1))
            imageLoader->preload(nextPos + 1);
    }
}

void Core::slotPrevImage() {
    if(dirManager->containsImages()) {
        stopAnimation();
        int nextPos = mCurrentIndex - 1;
        if(nextPos < 0) {
            if(infiniteScrolling)
                nextPos = dirManager->fileCount() - 1;
            else
                return;
        }
        mCurrentIndex = nextPos;
        imageLoader->open(nextPos);
        if(dirManager->checkRange(nextPos - 1))
            imageLoader->preload(nextPos - 1);
    }
}

void Core::saveImage() {
    if(currentImage()) {
        currentImage()->save();
    }
}

void Core::saveImage(QString path) {
    if(currentImage()) {
        currentImage()->save(path);
    }
}

void Core::setCurrentDir(QString path) {
    dirManager->setDirectory(path);
}

void Core::rotateImage(int degrees) {
    if(currentImage() != NULL) {
        currentImage()->rotate(degrees);
        ImageStatic *staticImage;
        if((staticImage = dynamic_cast<ImageStatic *>(currentImage())) != NULL) {
            emit imageAltered(currentImage()->getPixmap());
        }
        else if ((currentVideo = dynamic_cast<Video *>(currentImage())) != NULL) {
            emit videoAltered(currentVideo->getClip());
        }
        updateInfoString();
    }
}

void Core::removeFile() {
    if(dirManager->removeAt(mCurrentIndex)) {
        loadImageByPos(0);
    }
}

void Core::setWallpaper(QRect wpRect) {
    if(currentImage()) {
        ImageStatic *staticImage;
        if((staticImage = dynamic_cast<ImageStatic *>(currentImage())) != NULL) {
            QImage *cropped = NULL;
            QRect screenRes = QApplication::desktop()->screenGeometry();
            if(cropped = staticImage->cropped(wpRect, screenRes, true)) {
                QString savePath = QDir::homePath() + "/" + ".wallpaper.png";
                cropped->save(savePath);
                WallpaperSetter::setWallpaper(savePath);
                delete cropped;
            }
        }
    }
}

void Core::rescaleForZoom(QSize newSize) {
    if(currentImage() && currentImage()->isLoaded()) {
        ImageLib imgLib;
        float sourceSize = (float) currentImage()->width() *
                           currentImage()->height() / 1000000;
        float size = (float) newSize.width() *
                     newSize.height() / 1000000;
        QPixmap *pixmap;
        float currentScale = (float) sourceSize / size;
        if(currentScale == 1.0) {
            pixmap = currentImage()->getPixmap();
        } else {
            pixmap = new QPixmap(newSize);
            if(settings->useFastScale() || currentScale < 1.0) {
                imgLib.bilinearScale(pixmap, currentImage()->getPixmap(), newSize, true);
            } else {
                imgLib.bilinearScale(pixmap, currentImage()->getPixmap(), newSize, true);
                //imgLib.bicubicScale(pixmap, currentImage()->getImage(), newSize.width(), newSize.height());
            }
        }
        emit scalingFinished(pixmap);
    }
}

void Core::startAnimation() {
    if(currentImageAnimated) {
        currentImageAnimated->animationStart();
        connect(currentImageAnimated, SIGNAL(frameChanged(QPixmap *)),
                this, SIGNAL(frameChanged(QPixmap *)), Qt::UniqueConnection);
    }
}

void Core::stopAnimation() {
    /*
    if(currentImageAnimated) {
        currentImageAnimated->animationStop();
        // TODO: fix mess with null pointers here
        disconnect(currentImageAnimated, SIGNAL(frameChanged(QPixmap *)),
                   this, SIGNAL(frameChanged(QPixmap *)));
    }
    */

    if((currentImageAnimated = dynamic_cast<ImageAnimated *>(cache->imageAt(mPreviousIndex))) != NULL) {
        currentImageAnimated->animationStop();
        disconnect(currentImageAnimated, SIGNAL(frameChanged(QPixmap *)),
                   this, SIGNAL(frameChanged(QPixmap *)));
    }

    if(currentVideo) {
        emit stopVideo();
    }
}

// ##############################################################
// ####################### PRIVATE METHODS ######################
// ##############################################################

void Core::initVariables() {
    loadingTimer = new QTimer();
    loadingTimer->setSingleShot(true);
    loadingTimer->setInterval(250); // TODO: test on slower pc & adjust timeout
    dirManager = new DirectoryManager();
    cache = new ImageCache();
    imageLoader = new NewLoader(dirManager);
}

void Core::connectSlots() {
    connect(loadingTimer, SIGNAL(timeout()), this, SLOT(onLoadingTimeout()));
    connect(imageLoader, SIGNAL(loadStarted()),
            this, SLOT(onLoadStarted()));
    connect(imageLoader, SIGNAL(loadFinished(Image *, int)),
            this, SLOT(onLoadFinished(Image *, int)));
    connect(this, SIGNAL(thumbnailRequested(int, long)),
            imageLoader, SLOT(generateThumbnailFor(int, long)));
    connect(imageLoader, SIGNAL(thumbnailReady(long, Thumbnail *)),
            this, SIGNAL(thumbnailReady(long, Thumbnail *)));
    connect(cache, SIGNAL(initialized(int)), this, SIGNAL(cacheInitialized(int)), Qt::DirectConnection);
    connect(dirManager, SIGNAL(fileRemoved(int)), cache, SLOT(removeAt(int)), Qt::DirectConnection);
    connect(cache, SIGNAL(itemRemoved(int)), this, SIGNAL(itemRemoved(int)), Qt::DirectConnection);
}

Image* Core::currentImage() {
    return cache->imageAt(mCurrentIndex);
}

void Core::onLoadingTimeout() {
    stopAnimation();
    emit loadingTimeout();
}

// ##############################################################
// ####################### PRIVATE SLOTS ########################
// ##############################################################

void Core::onLoadStarted() {
    updateInfoString();
    loadingTimer->start();
}

void Core::onLoadFinished(Image *img, int pos) {
    mutex.lock();
    emit signalUnsetImage();
    loadingTimer->stop();
    currentImageAnimated = NULL;
    currentVideo = NULL;
    mPreviousIndex = pos;
    if((currentImageAnimated = dynamic_cast<ImageAnimated *>(cache->imageAt(pos))) != NULL) {
        startAnimation();
    }    
    if((currentVideo = dynamic_cast<Video *>(img)) != NULL) {
        emit videoChanged(currentVideo->getClip());
    }
    if(!currentVideo && img) {    //static image
        emit signalSetImage(img->getPixmap());
    }

    emit imageChanged(pos);
    updateInfoString();
    mutex.unlock();
}

void Core::crop(QRect newRect) {
    if(currentImage()) {
        ImageStatic *staticImage;
        if((staticImage = dynamic_cast<ImageStatic *>(currentImage())) != NULL) {
            staticImage->crop(newRect);
            updateInfoString();
            emit imageAltered(currentImage()->getPixmap());
        }
        else if ((currentVideo = dynamic_cast<Video *>(currentImage())) != NULL) {
            currentVideo->crop(newRect);
            updateInfoString();
            emit videoAltered(currentVideo->getClip());
        }
    }

}

void Core::readSettings() {
    infiniteScrolling = settings->infiniteScrolling();
}
