//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_SETTINGSMANAGER_H
#define BFVIEW_SETTINGSMANAGER_H

#include "Nomad/Archives/Types.hpp"
#include <QSettings>
#include <QString>

namespace BFView {
    class SettingsManager {
      public:
        static SettingsManager& GetInstance();

        static inline const QString ALWAYS_ASK = "ALWAYS_ASK";
        static inline const QString NONE_PROJECT = "NONE";

        void Load();
        void Save() const;

        bool GetConfirmOnDelete() const { return m_confirmDelete; }
        void SetConfirmOnDelete(bool confirm) { m_confirmDelete = confirm; }

        bool GetConfirmOnAdd() const { return m_confirmOnAdd; }
        void SetConfirmOnAdd(bool confirm) { m_confirmOnAdd = confirm; }

        bool GetUseCustomHandlers() const { return m_useCustomHandlers; }
        void SetUseCustomHandlers(bool enable) { m_useCustomHandlers = enable; }

        QString GetTempExtractPath() const { return m_tempExtractPath; }
        void SetTempExtractPath(const QString& path) {
            m_tempExtractPath = path;
        }

        QString GetDefaultExtractPath() const { return m_defaultExtractPath; }
        void SetDefaultExtractPath(const QString& path) {
            m_defaultExtractPath = path;
        }

        QString GetProjectsDirectory() const;
        QString GetCustomProjectsDirectory() const {
            return m_projectsDirectory;
        }
        void SetProjectsDirectory(const QString& dir);

        QString GetDefaultProject(Nomad::Archives::ArchiveVersion version,
                                  uint32_t rawVersion = 0) const;
        void SetDefaultProject(Nomad::Archives::ArchiveVersion version,
                               const QString& project);
        void SetDefaultProject(Nomad::Archives::ArchiveVersion version,
                               uint32_t rawVersion, const QString& project);

        Nomad::Archives::CompressionScheme
        GetDefaultCompression(Nomad::Archives::ArchiveVersion version,
                              uint32_t rawVersion = 0) const;
        void SetDefaultCompression(Nomad::Archives::ArchiveVersion version,
                                   Nomad::Archives::CompressionScheme scheme);
        void SetDefaultCompression(Nomad::Archives::ArchiveVersion version,
                                   uint32_t rawVersion,
                                   Nomad::Archives::CompressionScheme scheme);

        QString GetAppTheme() const { return m_appTheme; }
        void SetAppTheme(const QString& theme) { m_appTheme = theme; }

        bool GetCheckAssociationOnLaunch() const {
            return m_checkAssociationOnLaunch;
        }
        void SetCheckAssociationOnLaunch(bool enable) {
            m_checkAssociationOnLaunch = enable;
        }

        bool GetAutoRegisterAssociation() const {
            return m_checkAssociationOnLaunch;
        }
        void SetAutoRegisterAssociation(bool enable) {
            m_checkAssociationOnLaunch = enable;
        }

        QByteArray GetGeometry() const { return m_geometry; }
        void SetGeometry(QByteArray geometry) { m_geometry = geometry; }

      private:
        SettingsManager();
        ~SettingsManager() = default;

        SettingsManager(const SettingsManager&) = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        QString GetSettingsFilePath() const;

        bool m_confirmDelete = true;
        bool m_confirmOnAdd = true;
        bool m_useCustomHandlers = true;
        QString m_tempExtractPath;
        QString m_defaultExtractPath;
        QString m_projectsDirectory;
        bool m_checkAssociationOnLaunch = true;
#if defined(_WIN32) || defined(__WIN32__)
        QString m_appTheme = "BreezeDark";
#else
        QString m_appTheme = "System";
#endif

        QString m_defaultProjectFATv2_v7 = ALWAYS_ASK;
        QString m_defaultProjectFATv2_v8 = ALWAYS_ASK;
        QString m_defaultProjectFATv3_v7 = ALWAYS_ASK;
        QString m_defaultProjectFATv3_v8 = ALWAYS_ASK;
        QString m_defaultProjectFATv5_v11 = ALWAYS_ASK;
        QString m_defaultProjectFATv5_v12 = ALWAYS_ASK;
        QString m_defaultProjectFATv5_v13 = ALWAYS_ASK;

        Nomad::Archives::CompressionScheme m_defaultCompressionFATv2_v7 =
            Nomad::Archives::CompressionScheme::XMemCompress;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv2_v8 =
            Nomad::Archives::CompressionScheme::XMemCompress;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv3_v7 =
            Nomad::Archives::CompressionScheme::XMemCompress;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv3_v8 =
            Nomad::Archives::CompressionScheme::XMemCompress;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv5_v11 =
            Nomad::Archives::CompressionScheme::LZ4LW;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv5_v12 =
            Nomad::Archives::CompressionScheme::LZ4LW;
        Nomad::Archives::CompressionScheme m_defaultCompressionFATv5_v13 =
            Nomad::Archives::CompressionScheme::LZ4LW;

        QByteArray m_geometry = {};
    };

} // namespace BFView

#endif // BFVIEW_SETTINGSMANAGER_H
