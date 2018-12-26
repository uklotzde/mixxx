#include "aoide/settings.h"

#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide Settings");

const QString kGroup = "[aoide]";

const ConfigKey kCollectionUidKey(kGroup, "collectionUid");

const QString kCollectionUidDefaultValue = QString();

const ConfigKey kProtocolKey(kGroup, "protocol");

const QString kProtocolDefaultValue = "http";

const ConfigKey kHostKey(kGroup, "host");

const QString kHostDefaultValue = "[::1]";

const ConfigKey kPortKey(kGroup, "port");

const int kPortDefaultValue = 7878;

const ConfigKey kMultiGenreSeparatorKey(kGroup, "multiGenreSeparator");

const QString kMultiGenreSeparatorDefaultValue = " - ";

const ConfigKey kMultiGenreAttenuationKey(kGroup, "multiGenreAttenuation");

const double kMultiGenreAttenuationDefaultValue = 0.75;

} // anonymous namespace

Settings::Settings(UserSettingsPointer userSettings) : m_userSettings(std::move(userSettings)) {
    DEBUG_ASSERT(m_userSettings);
}

QString Settings::collectionUid() const {
    return m_userSettings->getValue(kCollectionUidKey, kCollectionUidDefaultValue);
}

void Settings::setCollectionUid(QString collectionUid) const {
    m_userSettings->setValue(kCollectionUidKey, collectionUid);
}

QString Settings::protocol() const {
    return m_userSettings->getValue(kProtocolKey, kProtocolDefaultValue);
}

void Settings::setProtocol(QString protocol) const {
    m_userSettings->setValue(kProtocolKey, protocol);
}

QString Settings::host() const {
    return m_userSettings->getValue(kHostKey, kHostDefaultValue);
}

void Settings::setHost(QString host) const {
    m_userSettings->setValue(kHostKey, host);
}

int Settings::port() const {
    return m_userSettings->getValue(kPortKey, kPortDefaultValue);
}

void Settings::setPort(int port) const {
    m_userSettings->setValue(kPortKey, port);
}

QUrl Settings::url() const {
    QUrl url;
    url.setScheme(protocol());
    url.setHost(host());
    url.setPort(port());
    return url;
}

QString Settings::multiGenreSeparator() const {
    return m_userSettings->getValue(kMultiGenreSeparatorKey, kMultiGenreSeparatorDefaultValue);
}

void Settings::setMultiGenreSeparator(QString multiGenreSeparator) const {
    DEBUG_ASSERT(!multiGenreSeparator.isEmpty());
    m_userSettings->setValue(kMultiGenreSeparatorKey, std::move(multiGenreSeparator));
}

double Settings::multiGenreAttenuation() const {
    return m_userSettings->getValue(kMultiGenreAttenuationKey, kMultiGenreAttenuationDefaultValue);
}

void Settings::setMultiGenreAttenuation(double multiGenreAttenuation) const {
    DEBUG_ASSERT(multiGenreAttenuation > 0);
    DEBUG_ASSERT(multiGenreAttenuation <= 1);
    m_userSettings->setValue(kMultiGenreAttenuationKey, std::move(multiGenreAttenuation));
}

} // namespace aoide

} // namespace mixxx
