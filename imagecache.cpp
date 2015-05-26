#include "imagecache.h"

ImageCache::ImageCache() {
    cachedImages = new QList<CacheObject*>();
    applySettings();
    connect(globalSettings, SIGNAL(settingsChanged()),
            this, SLOT(applySettings()));
}

// call when changing directory
void ImageCache::init(QString directory, QStringList list) {
    dir = directory;
    int time = clock();
    qDebug() << "init cache";
    // also should free memory
    lock();
    cachedImages->clear();
    for(int i=0; i<list.length(); i++) {
        cachedImages->append(new CacheObject(list.at(i)));
    }
    unlock();
    qDebug() << "init finished in " << clock() - time;
    emit initialized(length());
}

QString ImageCache::directory() {
    return dir;
}

void ImageCache::unloadAll() {
    lock();
    int time = clock();
    for(int i=0; i<cachedImages->length(); i++) {
        cachedImages->at(i)->unload();
    }
    unlock();
}

void ImageCache::loadAt(int pos) {
    lock();
    cachedImages->at(pos)->load();
    unlock();
}

Image* ImageCache::imageAt(int pos) {
    return cachedImages->at(pos)->image();
}

const QPixmap* ImageCache::thumbnailAt(int pos) const {
    return cachedImages->at(pos)->getThumbnail();
}

void ImageCache::generateAllThumbnails() {
    QtConcurrent::map(*cachedImages, [](CacheObject* obj) {obj->generateThumbnail();} );
}

const FileInfo* ImageCache::infoAt(int pos) {
    return cachedImages->at(pos)->getInfo();
}

int ImageCache::length() const {
    return cachedImages->length();
}

void ImageCache::readSettings() {
    lock();
    maxCacheSize = globalSettings->s.value("cacheSize").toInt();
    unlock();
}

void ImageCache::applySettings() {
    readSettings();
    //shrinkTo(maxCacheSize);
}

void ImageCache::lock() {
    mutex.lock();
}

void ImageCache::unlock() {
    mutex.unlock();
}

ImageCache::~ImageCache() {

}
