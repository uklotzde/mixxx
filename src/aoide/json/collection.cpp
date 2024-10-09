#include "aoide/json/collection.h"

#include "util/assert.h"

namespace aoide {

namespace json {

//static
ContentPathConfig ContentPathConfig::forLocalFiles(
        const std::optional<mixxx::EncodedUrl>& rootUrl) {
    ContentPathConfig config;
    if (rootUrl) {
        config.setPathKind(kSourcePathKindVirtualFilePath);
        config.setRootUrl(rootUrl);
    } else {
        config.setPathKind(kSourcePathKindFileUrl);
    }
    return config;
}

ContentPathConfig::ContentPathConfig(QJsonObject jsonObject)
        : Object(std::move(jsonObject)) {
}

int ContentPathConfig::pathKind() const {
    return m_jsonObject.value(QLatin1String("pathKind")).toInt(kSourcePathKindUri);
}

void ContentPathConfig::setPathKind(int pathKind) {
    m_jsonObject.insert(QLatin1String("pathKind"), pathKind);
}

std::optional<mixxx::EncodedUrl> ContentPathConfig::rootUrl() const {
    const auto value = m_jsonObject.value(QLatin1String("rootUrl"));
    if (value.isUndefined() || value.isNull()) {
        return std::nullopt;
    }
    return mixxx::EncodedUrl::fromEncodedQByteArray(value.toString().toUtf8());
}

void ContentPathConfig::setRootUrl(
        const std::optional<mixxx::EncodedUrl>& rootUrl) {
    if (rootUrl) {
        putOptionalNonEmpty(QLatin1String("rootUrl"), rootUrl->toQString());
    } else {
        m_jsonObject.remove(QLatin1String("rootUrl"));
    }
}

QString ContentPathConfig::virtualFilePathFromFileInfo(
        const mixxx::FileInfo& fileInfo) const {
    VERIFY_OR_DEBUG_ASSERT(pathKind() == kSourcePathKindVirtualFilePath) {
        return QString{};
    }
    const auto location = fileInfo.location();
    if (!rootUrl()) {
        // Pass-through
        return location;
    }
    const auto baseLocation = mixxx::FileInfo::fromQUrl(rootUrl()->toQUrl()).location();
    DEBUG_ASSERT(!baseLocation.isEmpty());
    VERIFY_OR_DEBUG_ASSERT(location.startsWith(baseLocation)) {
        return QString();
    }
    DEBUG_ASSERT(location.size() > baseLocation.size());
    return location.right(location.size() - baseLocation.size());
}

mixxx::FileInfo ContentPathConfig::fileInfoFromVirtualFilePath(
        const QString& path) const {
    VERIFY_OR_DEBUG_ASSERT(pathKind() == kSourcePathKindVirtualFilePath) {
        return mixxx::FileInfo{};
    }
    if (!rootUrl()) {
        // Pass-through
        return mixxx::FileInfo{path};
    }
    const auto baseFileInfo = mixxx::FileInfo::fromQUrl(rootUrl()->toQUrl());
    return mixxx::FileInfo{baseFileInfo.toQDir(), path};
}

//static
MediaSourceConfig MediaSourceConfig::forLocalFiles(
        const std::optional<mixxx::EncodedUrl>& rootUrl) {
    MediaSourceConfig config;
    config.setContentPath(ContentPathConfig::forLocalFiles(rootUrl));
    return config;
}

MediaSourceConfig::MediaSourceConfig(QJsonObject jsonObject)
        : Object(std::move(jsonObject)) {
}

ContentPathConfig MediaSourceConfig::contentPath() const {
    return ContentPathConfig(
            m_jsonObject.value(QLatin1String("contentPath")).toObject());
}

void MediaSourceConfig::setContentPath(
        ContentPathConfig contentPath) {
    if (contentPath.isEmpty()) {
        m_jsonObject.remove(
                QLatin1String("contentPath"));
    } else {
        m_jsonObject.insert(
                QLatin1String("contentPath"),
                contentPath.intoQJsonValue());
    }
}

Collection::Collection(QJsonObject jsonObject)
        : Object(std::move(jsonObject)) {
}

QString Collection::title() const {
    return m_jsonObject.value(QLatin1String("title")).toString();
}

void Collection::setTitle(const QString& title) {
    putOptionalNonEmpty(QLatin1String("title"), title);
}

QString Collection::kind() const {
    return m_jsonObject.value(QLatin1String("kind")).toString();
}

void Collection::setKind(QString kind) {
    putOptionalNonEmpty(QLatin1String("kind"), std::move(kind));
}

mixxx::RgbColor::optional_t Collection::color() const {
    return mixxx::RgbColor::fromQString(m_jsonObject.value(QLatin1String("color"))
                                                .toObject()
                                                .value(QLatin1String("rgb"))
                                                .toString());
}

void Collection::setColor(mixxx::RgbColor::optional_t color) {
    if (color) {
        m_jsonObject.insert(QLatin1String("color"),
                QJsonObject{
                        {QLatin1String("rgb"),
                                mixxx::RgbColor::toQString(*color)},
                });
    } else {
        m_jsonObject.remove(QLatin1String("color"));
    }
}

QString Collection::notes() const {
    return m_jsonObject.value(QLatin1String("notes")).toString();
}

void Collection::setNotes(const QString& notes) {
    putOptionalNonEmpty(QLatin1String("notes"), notes);
}

MediaSourceConfig Collection::mediaSourceConfig() const {
    return MediaSourceConfig(
            m_jsonObject.value(QLatin1String("mediaSourceConfig")).toObject());
}

void Collection::setMediaSourceConfig(
        MediaSourceConfig mediaSourceConfig) {
    m_jsonObject.insert(
            QLatin1String("mediaSourceConfig"),
            mediaSourceConfig.intoQJsonValue());
}

EntityHeader CollectionEntity::header() const {
    return EntityHeader(m_jsonArray.at(0).toArray());
}

Collection CollectionEntity::body() const {
    return Collection(m_jsonArray.at(1).toObject());
}

} // namespace json

} // namespace aoide
