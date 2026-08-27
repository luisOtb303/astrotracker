#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPixmap>
#include <QMap>
#include <QThread>
#include <atomic>
#include <vector>

class QThread;

// Filmstrip widget for photo sequences.
// Features: lazy-loading thumbnails, color-coded badges, export checkboxes.
class FilmstripWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit FilmstripWidget(QWidget* parent = nullptr);

    void loadThumbnails(const std::vector<std::string>& filePaths, int maxDim = 240);
    void clearAll();

    // Badge status per frame index.
    enum class BadgeStatus { None, Valid, Predicted, Lost, Locked };
    void setBadge(int index, BadgeStatus status, const QString& label = {});
    void setExportState(int index, bool checked);
    bool exportState(int index) const;

    void setCurrentFrame(int index);

signals:
    void frameActivated(int index);
    void exportToggled(int index, bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onThumbnailReady(int index, QPixmap pixmap);
    void onAllThumbnailsLoaded();

private:
    void startBackgroundLoader(const std::vector<std::string>& paths, int maxDim);

    QMap<int, QPixmap> thumbnails_;
    QMap<int, BadgeStatus> badges_;
    QMap<int, QString> badgeLabels_;
    std::atomic<bool> loading_{false};
    int currentFrame_ = -1;
};

// Background thread that loads thumbnails one by one.
class ThumbnailLoader : public QThread
{
    Q_OBJECT
public:
    ThumbnailLoader(const std::vector<std::string>& paths, int maxDim,
                    FilmstripWidget* target);

protected:
    void run() override;

signals:
    void thumbnailReady(int index, QPixmap pixmap);
    void allLoaded();

private:
    std::vector<std::string> paths_;
    int maxDim_;
    FilmstripWidget* target_;
};
