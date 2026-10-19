//
// Created by troplo on 09/21/26.
//

#include "BFView/Settings/SettingsManager.hpp"

#include <iostream>

#include "BFView/Theme/ThemeManager.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace BFView {
    SettingsManager& SettingsManager::GetInstance() {
        static SettingsManager instance;
        return instance;
    }

    SettingsManager::SettingsManager() {
        m_tempExtractPath = QDir::tempPath() + "/BFView_tmp";
        Load();
    }

    QString SettingsManager::GetSettingsFilePath() const {
        QString configDir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
        return configDir + "/settings.json";
    }

    QString SettingsManager::GetProjectsDirectory() const {
        if (!m_projectsDirectory.isEmpty()) {
            return m_projectsDirectory;
        }
        auto defaultDir =
            Nomad::Archives::ProjectManager::GetInstance().GetProjectsRoot();
        return QString::fromStdString(defaultDir.string());
    }

    void SettingsManager::SetProjectsDirectory(const QString& dir) {
        m_projectsDirectory = dir.trimmed();
        if (!m_projectsDirectory.isEmpty()) {
            Nomad::Archives::ProjectManager::GetInstance().SetProjectsRoot(
                m_projectsDirectory.toStdString());
        } else {
            Nomad::Archives::ProjectManager::GetInstance().SetProjectsRoot("");
        }
    }

    QString
    SettingsManager::GetDefaultProject(Nomad::Archives::ArchiveVersion version,
                                       uint32_t rawVersion) const {
        switch (version) {
        case Nomad::Archives::ArchiveVersion::FATv2:
            if (rawVersion == 7)
                return m_defaultProjectFATv2_v7;
            return m_defaultProjectFATv2_v8;
        case Nomad::Archives::ArchiveVersion::FATv3:
            if (rawVersion == 7)
                return m_defaultProjectFATv3_v7;
            return m_defaultProjectFATv3_v8;
        case Nomad::Archives::ArchiveVersion::FATv5:
            if (rawVersion == 11)
                return m_defaultProjectFATv5_v11;
            if (rawVersion == 12)
                return m_defaultProjectFATv5_v12;
            return m_defaultProjectFATv5_v13;
        default:
            return ALWAYS_ASK;
        }
    }

    void
    SettingsManager::SetDefaultProject(Nomad::Archives::ArchiveVersion version,
                                       const QString& project) {
        SetDefaultProject(version, 0, project);
    }

    void
    SettingsManager::SetDefaultProject(Nomad::Archives::ArchiveVersion version,
                                       uint32_t rawVersion,
                                       const QString& project) {
        switch (version) {
        case Nomad::Archives::ArchiveVersion::FATv2:
            if (rawVersion == 7)
                m_defaultProjectFATv2_v7 = project;
            else if (rawVersion == 8)
                m_defaultProjectFATv2_v8 = project;
            else {
                m_defaultProjectFATv2_v7 = project;
                m_defaultProjectFATv2_v8 = project;
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv3:
            if (rawVersion == 7)
                m_defaultProjectFATv3_v7 = project;
            else if (rawVersion == 8)
                m_defaultProjectFATv3_v8 = project;
            else {
                m_defaultProjectFATv3_v7 = project;
                m_defaultProjectFATv3_v8 = project;
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv5:
            if (rawVersion == 11)
                m_defaultProjectFATv5_v11 = project;
            else if (rawVersion == 12)
                m_defaultProjectFATv5_v12 = project;
            else if (rawVersion == 13)
                m_defaultProjectFATv5_v13 = project;
            else {
                m_defaultProjectFATv5_v11 = project;
                m_defaultProjectFATv5_v12 = project;
                m_defaultProjectFATv5_v13 = project;
            }
            break;
        default:
            break;
        }
    }

    Nomad::Archives::CompressionScheme SettingsManager::GetDefaultCompression(
        Nomad::Archives::ArchiveVersion version, uint32_t rawVersion) const {
        switch (version) {
        case Nomad::Archives::ArchiveVersion::FATv2:
            if (rawVersion == 7)
                return m_defaultCompressionFATv2_v7;
            return m_defaultCompressionFATv2_v8;
        case Nomad::Archives::ArchiveVersion::FATv3:
            if (rawVersion == 7)
                return m_defaultCompressionFATv3_v7;
            return m_defaultCompressionFATv3_v8;
        case Nomad::Archives::ArchiveVersion::FATv5:
            if (rawVersion == 11)
                return m_defaultCompressionFATv5_v11;
            if (rawVersion == 12)
                return m_defaultCompressionFATv5_v12;
            return m_defaultCompressionFATv5_v13;
        default:
            return Nomad::Archives::CompressionScheme::None;
        }
    }

    void SettingsManager::SetDefaultCompression(
        Nomad::Archives::ArchiveVersion version,
        Nomad::Archives::CompressionScheme scheme) {
        SetDefaultCompression(version, 0, scheme);
    }

    void SettingsManager::SetDefaultCompression(
        Nomad::Archives::ArchiveVersion version, uint32_t rawVersion,
        Nomad::Archives::CompressionScheme scheme) {
        switch (version) {
        case Nomad::Archives::ArchiveVersion::FATv2:
            if (rawVersion == 7)
                m_defaultCompressionFATv2_v7 = scheme;
            else if (rawVersion == 8)
                m_defaultCompressionFATv2_v8 = scheme;
            else {
                m_defaultCompressionFATv2_v7 = scheme;
                m_defaultCompressionFATv2_v8 = scheme;
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv3:
            if (rawVersion == 7)
                m_defaultCompressionFATv3_v7 = scheme;
            else if (rawVersion == 8)
                m_defaultCompressionFATv3_v8 = scheme;
            else {
                m_defaultCompressionFATv3_v7 = scheme;
                m_defaultCompressionFATv3_v8 = scheme;
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv5:
            if (rawVersion == 11)
                m_defaultCompressionFATv5_v11 = scheme;
            else if (rawVersion == 12)
                m_defaultCompressionFATv5_v12 = scheme;
            else if (rawVersion == 13)
                m_defaultCompressionFATv5_v13 = scheme;
            else {
                m_defaultCompressionFATv5_v11 = scheme;
                m_defaultCompressionFATv5_v12 = scheme;
                m_defaultCompressionFATv5_v13 = scheme;
            }
            break;
        default:
            break;
        }
    }

    void SettingsManager::Load() {
        QString filePath = GetSettingsFilePath();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            return;
        }

        QJsonObject obj = doc.object();
        if (obj.contains("defaultExtractPath")) {
            m_defaultExtractPath = obj["defaultExtractPath"].toString();
        }
        if (obj.contains("confirmOnDelete")) {
            m_confirmDelete = obj["confirmOnDelete"].toBool(true);
        }
        if (obj.contains("confirmOnAdd")) {
            m_confirmOnAdd = obj["confirmOnAdd"].toBool(true);
        }
        if (obj.contains("useCustomHandlers")) {
            m_useCustomHandlers = obj["useCustomHandlers"].toBool(true);
        }
        if (obj.contains("tempExtractPath")) {
            m_tempExtractPath = obj["tempExtractPath"].toString(
                QDir::tempPath() + "/BFView_tmp");
        }
        if (obj.contains("projectsDirectory")) {
            m_projectsDirectory = obj["projectsDirectory"].toString().trimmed();
            if (!m_projectsDirectory.isEmpty()) {
                Nomad::Archives::ProjectManager::GetInstance().SetProjectsRoot(
                    m_projectsDirectory.toStdString());
            } else {
                Nomad::Archives::ProjectManager::GetInstance().SetProjectsRoot(
                    "");
            }
        }

        QString legacyProjV2 =
            obj.value("defaultProjectFATv2").toString(ALWAYS_ASK);
        QString legacyProjV3 =
            obj.value("defaultProjectFATv3").toString(ALWAYS_ASK);
        QString legacyProjV5 =
            obj.value("defaultProjectFATv5").toString(ALWAYS_ASK);

        m_defaultProjectFATv2_v7 =
            obj.value("defaultProjectFATv2_v7").toString(legacyProjV2);
        m_defaultProjectFATv2_v8 =
            obj.value("defaultProjectFATv2_v8").toString(legacyProjV2);
        m_defaultProjectFATv3_v7 =
            obj.value("defaultProjectFATv3_v7").toString(legacyProjV3);
        m_defaultProjectFATv3_v8 =
            obj.value("defaultProjectFATv3_v8").toString(legacyProjV3);
        m_defaultProjectFATv5_v11 =
            obj.value("defaultProjectFATv5_v11").toString(legacyProjV5);
        m_defaultProjectFATv5_v12 =
            obj.value("defaultProjectFATv5_v12").toString(legacyProjV5);
        m_defaultProjectFATv5_v13 =
            obj.value("defaultProjectFATv5_v13").toString(legacyProjV5);

        auto parseComp = [](const QJsonValue& val,
                            Nomad::Archives::CompressionScheme fallback) {
            if (!val.isString())
                return fallback;
            auto parsed = Nomad::Archives::ParseCompressionScheme(
                val.toString().toStdString());
            return (parsed != Nomad::Archives::CompressionScheme::Unknown)
                       ? parsed
                       : fallback;
        };

        Nomad::Archives::CompressionScheme legacyCompV2 =
            parseComp(obj["defaultCompressionFATv2"],
                      Nomad::Archives::CompressionScheme::XMemCompress);
        Nomad::Archives::CompressionScheme legacyCompV3 =
            parseComp(obj["defaultCompressionFATv3"],
                      Nomad::Archives::CompressionScheme::XMemCompress);
        Nomad::Archives::CompressionScheme legacyCompV5 =
            parseComp(obj["defaultCompressionFATv5"],
                      Nomad::Archives::CompressionScheme::LZ4LW);

        m_defaultCompressionFATv2_v7 =
            parseComp(obj["defaultCompressionFATv2_v7"], legacyCompV2);
        m_defaultCompressionFATv2_v8 =
            parseComp(obj["defaultCompressionFATv2_v8"], legacyCompV2);
        m_defaultCompressionFATv3_v7 =
            parseComp(obj["defaultCompressionFATv3_v7"], legacyCompV3);
        m_defaultCompressionFATv3_v8 =
            parseComp(obj["defaultCompressionFATv3_v8"], legacyCompV3);
        m_defaultCompressionFATv5_v11 =
            parseComp(obj["defaultCompressionFATv5_v11"], legacyCompV5);
        m_defaultCompressionFATv5_v12 =
            parseComp(obj["defaultCompressionFATv5_v12"], legacyCompV5);
        m_defaultCompressionFATv5_v13 =
            parseComp(obj["defaultCompressionFATv5_v13"], legacyCompV5);

        if (obj.contains("appTheme")) {
            QString defaultTheme =
#if defined(_WIN32) || defined(__WIN32__)
                "BreezeDark";
#else
                "System";
#endif
            QString loadedTheme = obj["appTheme"].toString(defaultTheme);
            AppTheme parsed = ThemeManager::ThemeFromString(loadedTheme);
            m_appTheme = ThemeManager::ThemeToString(parsed);
        }
        if (obj.contains("checkAssociationOnLaunch")) {
            m_checkAssociationOnLaunch =
                obj["checkAssociationOnLaunch"].toBool(true);
        } else if (obj.contains("autoRegisterAssociation")) {
            m_checkAssociationOnLaunch =
                obj["autoRegisterAssociation"].toBool(true);
        }
        if (obj.contains("geometry")) {
            m_geometry =
                QByteArray::fromBase64(obj["geometry"].toString().toUtf8());
        }
    }

    void SettingsManager::Save() const {
        QString filePath = GetSettingsFilePath();
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }

        QJsonObject obj;
        obj["defaultExtractPath"] = m_defaultExtractPath;
        obj["confirmOnDelete"] = m_confirmDelete;
        obj["confirmOnAdd"] = m_confirmOnAdd;
        obj["useCustomHandlers"] = m_useCustomHandlers;
        obj["tempExtractPath"] = m_tempExtractPath;
        obj["projectsDirectory"] = m_projectsDirectory;

        obj["defaultProjectFATv2"] = m_defaultProjectFATv2_v8;
        obj["defaultProjectFATv3"] = m_defaultProjectFATv3_v8;
        obj["defaultProjectFATv5"] = m_defaultProjectFATv5_v13;

        obj["defaultProjectFATv2_v7"] = m_defaultProjectFATv2_v7;
        obj["defaultProjectFATv2_v8"] = m_defaultProjectFATv2_v8;
        obj["defaultProjectFATv3_v7"] = m_defaultProjectFATv3_v7;
        obj["defaultProjectFATv3_v8"] = m_defaultProjectFATv3_v8;
        obj["defaultProjectFATv5_v11"] = m_defaultProjectFATv5_v11;
        obj["defaultProjectFATv5_v12"] = m_defaultProjectFATv5_v12;
        obj["defaultProjectFATv5_v13"] = m_defaultProjectFATv5_v13;

        auto compToStr = [](Nomad::Archives::CompressionScheme s) {
            return QString::fromUtf8(
                Nomad::Archives::CompressionToString(s).data(),
                static_cast<qsizetype>(
                    Nomad::Archives::CompressionToString(s).size()));
        };

        obj["defaultCompressionFATv2"] =
            compToStr(m_defaultCompressionFATv2_v8);
        obj["defaultCompressionFATv3"] =
            compToStr(m_defaultCompressionFATv3_v8);
        obj["defaultCompressionFATv5"] =
            compToStr(m_defaultCompressionFATv5_v13);

        obj["defaultCompressionFATv2_v7"] =
            compToStr(m_defaultCompressionFATv2_v7);
        obj["defaultCompressionFATv2_v8"] =
            compToStr(m_defaultCompressionFATv2_v8);
        obj["defaultCompressionFATv3_v7"] =
            compToStr(m_defaultCompressionFATv3_v7);
        obj["defaultCompressionFATv3_v8"] =
            compToStr(m_defaultCompressionFATv3_v8);
        obj["defaultCompressionFATv5_v11"] =
            compToStr(m_defaultCompressionFATv5_v11);
        obj["defaultCompressionFATv5_v12"] =
            compToStr(m_defaultCompressionFATv5_v12);
        obj["defaultCompressionFATv5_v13"] =
            compToStr(m_defaultCompressionFATv5_v13);

        obj["appTheme"] = m_appTheme;
        obj["checkAssociationOnLaunch"] = m_checkAssociationOnLaunch;
        obj["autoRegisterAssociation"] = m_checkAssociationOnLaunch;
        obj["geometry"] = QString::fromUtf8(m_geometry.toBase64());

        file.write(QJsonDocument(obj).toJson());
    }

} // namespace BFView
