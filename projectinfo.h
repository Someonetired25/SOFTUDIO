#ifndef PROJECTINFO_H
#define PROJECTINFO_H

#include <QString>
#include <QMetaType>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QHash>
#include <utility> // Added for std::move

struct ProjectInfo {
    QString name;
    QString path;
    QString uid;
    QString type;
    bool isSoftudioProjectFlag = false;
    bool isValidatedSoftudioProject = false;
    bool heuristicallyFound = false;
    QIcon icon;

    ProjectInfo() = default;

    ProjectInfo(QString pPath, QString pType = "unknown")
        : path(std::move(pPath)), type(std::move(pType)) { // Line 24 in your error log
        if (name.isEmpty() && !this->path.isEmpty()) {
            QFileInfo fileInfo(this->path);
            if (fileInfo.isDir()) {
                name = fileInfo.dir().dirName();
                if (name == "." || name == "..") {
                     QDir dir(this->path);
                     dir.cdUp();
                     name = dir.dirName();
                }
            } else {
                name = fileInfo.fileName();
            }
        }
    }

    bool operator==(const ProjectInfo& other) const {
        return path == other.path && (uid.isEmpty() || other.uid.isEmpty() || uid == other.uid);
    }
}; // Ensure this semicolon is present

Q_DECLARE_METATYPE(ProjectInfo)

inline uint qHash(const ProjectInfo &key, size_t seed = 0) {
    size_t h1 = qHash(key.path, seed);
    size_t h2 = qHash(key.uid, seed >> 1);
    return static_cast<uint>(h1 ^ h2);
}

#endif // PROJECTINFO_H