#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStackedWidget>
#include <QListWidget>
#include <QVector>
#include <QBuffer>
#include "SpotifyClient.h"
#include <QComboBox>
#include <QDate>
#include <QColorDialog>
#include <QSettings>
#include <unordered_set>
#include <QTimer>
#include <QHBoxLayout>

class RatingWidget : public QWidget {
    Q_OBJECT
public:
    RatingWidget(QWidget* parent = nullptr);
    void setRating(int value);
    int getRating() const { return currentRating; }
    void resetRating();

signals:
    void ratingChanged(int newRating);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    static const int MAX_RATING = 10;
    QHBoxLayout* layout;
    QLabel* ratingLabel;
    QVector<QLabel*> recordLabels;
    QPixmap normalRecord;
    QPixmap grayedRecord;
    int currentRating;
    QLabel* iconLabel;
    
    void setupRecords();
    void updateDisplay();
    void showRecords();
    void showRatingText();
};

class AlbumListItem : public QWidget {
    Q_OBJECT
public:
    AlbumListItem(const Album& album, QWidget* parent = nullptr, bool showRating = false);
    void setImage(const QPixmap& pixmap);
    const Album& album() const { return m_album; }
    void setAddToLibraryVisible(bool visible);
    void setAddToLibraryState(bool enabled, const QString& text = "Add to Library");
    void setRating(int rating);
    int getRating() const;

private:
    Album m_album;
    QLabel* imageLabel;
    QLabel* textLabel;
    QPushButton* addToLibraryButton;
    RatingWidget* ratingWidget;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    struct LibraryAlbum {
        Album album;
        QByteArray imageData;
        LibraryAlbum(const Album& a, const QByteArray& img) 
            : album(a), imageData(img) {}
    };

    struct ThemeColors {
        QString name;
        QColor background;
        QColor sidebar;
        QColor text;
        QColor accent;
        
        ThemeColors(const QString& n, const QColor& bg, const QColor& sb, 
                   const QColor& txt, const QColor& acc)
            : name(n), background(bg), sidebar(sb), text(txt), accent(acc) {}
    };

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void saveLibrary();
    QString formatDate(const std::string& dateStr);
    void updateAlbumRating(const std::string& albumId, int rating);

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void addToLibrary(const Album& album, const QPixmap& albumArt);

private slots:
    void performSearch(bool loadingMore = false);
    void showAlbumDetails(QListWidgetItem* item);
    void handleImageDownloaded(QNetworkReply* reply);
    void switchToSearch();
    void switchToLibrary();
    void loadLibrary();
    void sortLibrary(int sortIndex);
    void switchToSettings();
    void applyTheme(const ThemeColors& theme);

private:
    QWidget* centralWidget;
    QLineEdit* searchBox;
    QPushButton* searchButton;
    QListWidget* resultsList;
    QLabel* statusLabel;
    SpotifyClient spotify;
    std::vector<Album> currentResults;
    QNetworkAccessManager* networkManager;
    
    // New UI elements
    QListWidget* sidebar;
    QStackedWidget* stackedWidget;
    QWidget* searchPage;
    QWidget* libraryPage;
    QListWidget* libraryList;
    QVector<LibraryAlbum> libraryAlbums;
    QComboBox* sortComboBox;
    QWidget* settingsWidget;
    QPushButton* colorThemeButton;
    QVBoxLayout* sidebarLayout;
    QHBoxLayout* mainLayout;

    QWidget* settingsPage;
    QListWidget* themeList;
    QVector<ThemeColors> themes;

    std::unordered_set<std::string> libraryAlbumIds;

    bool isSearching = false;
    QTimer* searchDebounceTimer;

    QString currentSearchQuery;
    int currentSearchOffset = 0;
    QPushButton* loadMoreButton;
    int totalResults = 0;

    void setupUi();
    void setupSidebar();
    void setupSearchPage();
    void setupLibraryPage();
    void setupSettingsPage();
    void displayResults(const std::vector<Album>& albums, bool append = false);
    void downloadAlbumArt(const QString& url, AlbumListItem* item);
    void refreshLibraryDisplay();
    void removeSelectedAlbum();
    void checkScrollPosition();
};

#endif // MAINWINDOW_H 