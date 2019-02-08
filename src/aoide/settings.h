#pragma once

#include <QUrl>

#include "preferences/usersettings.h"

namespace mixxx {

namespace aoide {

class Settings {
  public:
    explicit Settings(
            UserSettingsPointer userSettings);

    QString collectionUid() const;
    void setCollectionUid(QString collectionUid) const;

    QString protocol() const;
    void setProtocol(QString protocol) const;

    QString host() const;
    void setHost(QString host) const;

    int port() const;
    void setPort(int port) const;

    // protocol + host + port
    QUrl url() const;

    QString multiGenreSeparator() const;
    void setMultiGenreSeparator(QString multiGenreSeparator) const;

    double multiGenreAttenuation() const;
    void setMultiGenreAttenuation(double multiGenreAttenuation) const;

  private:
    const UserSettingsPointer m_userSettings;
};

} // namespace aoide

} // namespace mixxx
