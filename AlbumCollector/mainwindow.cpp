#include "mainwindow.h"
#include <QMessageBox>
#include <QScrollArea>
#include <QDialog>
#include <QGridLayout>
#include <QPixmap>
#include <QHBoxLayout>
#include <QFile>
#include <QDataStream>
#include <QBuffer>
#include <QDebug>
#include <QTimer>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QComboBox>
#include <QDate>
#include <QColorDialog>
#include <QSettings>
#include <QMenu>
#include <QPainter>
#include <QApplication>
#include <QScrollBar>

AlbumListItem::AlbumListItem(const Album& album, QWidget* parent, bool showRating)
    : QWidget(parent), m_album(album)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QHBoxLayout* topLayout = new QHBoxLayout();
    
    // Image label
    imageLabel = new QLabel;
    imageLabel->setFixedSize(60, 60);
    imageLabel->setScaledContents(true);
    topLayout->addWidget(imageLabel);
    
    QString formattedDate = qobject_cast<MainWindow*>(window()) 
        ? qobject_cast<MainWindow*>(window())->formatDate(album.release_date)
        : QString::fromStdString(album.release_date);
    
    // Text label with word wrap
    textLabel = new QLabel(QString("%1 - %2 (%3)")
        .arg(QString::fromStdString(album.name))
        .arg(QString::fromStdString(album.artist))
        .arg(formattedDate));
    textLabel->setWordWrap(true);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topLayout->addWidget(textLabel, 1);

    // Add to Library button
    addToLibraryButton = new QPushButton("Add to Library");
    addToLibraryButton->setFixedWidth(120);
    addToLibraryButton->setStyleSheet("font-size: 11px;");
    topLayout->addWidget(addToLibraryButton);
    
    mainLayout->addLayout(topLayout);
    
    // Only add rating widget if showRating is true
    if (showRating) {
        ratingWidget = new RatingWidget(this);
        ratingWidget->setRating(m_album.rating);
        mainLayout->addWidget(ratingWidget);

        connect(ratingWidget, &RatingWidget::ratingChanged, this, [this](int newRating) {
            m_album.rating = newRating;
            MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
            if (mainWindow) {
                mainWindow->updateAlbumRating(m_album.id, newRating);
            }
        });
    } else {
        ratingWidget = nullptr;
    }

    connect(addToLibraryButton, &QPushButton::clicked, [this]() {
        MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
        if (mainWindow) {
            QPixmap pixmap = imageLabel->pixmap(Qt::ReturnByValue);
            mainWindow->addToLibrary(m_album, pixmap);
            setAddToLibraryState(false, "Added");
        }
    });
}

void AlbumListItem::setImage(const QPixmap& pixmap) {
    imageLabel->setPixmap(pixmap);
}

void AlbumListItem::setAddToLibraryVisible(bool visible) {
    addToLibraryButton->setVisible(visible);
}

void AlbumListItem::setAddToLibraryState(bool enabled, const QString& text)
{
    addToLibraryButton->setEnabled(enabled);
    addToLibraryButton->setText(text);
    
    // Set a dynamic property based on the button state
    addToLibraryButton->setProperty("isAdded", text == "Added");
    
    // Trigger style update
    addToLibraryButton->style()->unpolish(addToLibraryButton);
    addToLibraryButton->style()->polish(addToLibraryButton);
    addToLibraryButton->update();
}

void AlbumListItem::setRating(int rating) {
    if (ratingWidget) {
        ratingWidget->setRating(rating);
    }
}

int AlbumListItem::getRating() const {
    return ratingWidget ? ratingWidget->getRating() : 0;
}

RatingWidget::RatingWidget(QWidget* parent) : QWidget(parent), currentRating(0) {
    // Load record images
    normalRecord = QPixmap(":/images/BlackWhiteRecord.png");
    grayedRecord = QPixmap(":/images/BlackWhiteRecord.png");
    
    // Create grayed out version
    QPixmap temp = grayedRecord;
    QPainter painter(&temp);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(temp.rect(), QColor(0, 0, 0, 128));
    grayedRecord = temp;

    // Scale records to a reasonable size
    normalRecord = normalRecord.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    grayedRecord = grayedRecord.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    layout = new QHBoxLayout(this);
    layout->setSpacing(0);  // Set initial spacing to 0
    layout->setContentsMargins(0, 0, 0, 0);

    // Create rating label
    ratingLabel = new QLabel(this);
    ratingLabel->setVisible(false);
    layout->addWidget(ratingLabel);

    // Create icon label with no margins
    iconLabel = new QLabel(this);
    iconLabel->setPixmap(normalRecord);
    iconLabel->setContentsMargins(0, 0, 0, 0);
    iconLabel->setVisible(false);
    layout->addWidget(iconLabel);
    
    // Add stretch at the end to push everything to the left
    layout->addStretch();
    
    setupRecords();
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint&) {
        resetRating();
    });
}

void RatingWidget::setupRecords() {
    for (int i = 0; i < MAX_RATING; ++i) {
        QLabel* label = new QLabel;
        label->setPixmap(grayedRecord);
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
        recordLabels.append(label);
        layout->addWidget(label);
    }
}

void RatingWidget::setRating(int value) {
    currentRating = qBound(0, value, MAX_RATING);
    updateDisplay();
    emit ratingChanged(currentRating);
}

void RatingWidget::resetRating() {
    // First reset the rating to 0
    currentRating = 0;
    
    // Clear the rating text display
    ratingLabel->setVisible(false);
    
    // Show the record labels for new rating
    showRecords();
    
    // Emit the rating changed signal
    emit ratingChanged(currentRating);
}

void RatingWidget::updateDisplay() {
    if (currentRating > 0) {
        showRatingText();
    } else {
        showRecords();
    }
}

void RatingWidget::showRecords() {
    // Hide rating label and icon
    ratingLabel->setVisible(false);
    iconLabel->setVisible(false);

    // If we need to recreate the record labels
    if (recordLabels.isEmpty()) {
        setupRecords();
    }

    // Show all record labels and update their state
    for (int i = 0; i < recordLabels.size(); ++i) {
        recordLabels[i]->setVisible(true);
        recordLabels[i]->setPixmap(i < currentRating ? normalRecord : grayedRecord);
    }

    // Ensure layout is properly updated
    layout->update();
}

void RatingWidget::showRatingText() {
    // Hide record labels
    for (QLabel* label : recordLabels) {
        label->setVisible(false);
        label->deleteLater();
    }
    recordLabels.clear();

    // Show rating text with record icon
    QString ratingText = QString("%1/10").arg(currentRating);
    ratingLabel->setText(ratingText);
    ratingLabel->setVisible(true);

    // Adjust icon label spacing and position
    iconLabel->setContentsMargins(0, 0, 0, 0);  // Remove all margins
    iconLabel->setVisible(true);
    
    // Create a tighter layout
    layout->setSpacing(0);  // Remove spacing between widgets
    
    // Add a small fixed width spacer (2-3 pixels) between text and icon
    QSpacerItem* spacer = new QSpacerItem(2, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    layout->addSpacerItem(spacer);

    // Ensure layout is properly updated
    layout->update();
}

bool RatingWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        QLabel* clickedLabel = qobject_cast<QLabel*>(obj);
        if (clickedLabel) {
            int newRating = recordLabels.indexOf(clickedLabel) + 1;
            setRating(newRating);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , spotify("9c18388b794041aca87c4f3d975e580e", "f0228bebde384425865f5a6bc93dd979")
{
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::handleImageDownloaded);

    // Initialize search debounce timer
    searchDebounceTimer = new QTimer(this);
    searchDebounceTimer->setSingleShot(true);
    searchDebounceTimer->setInterval(300); // 300ms debounce
    connect(searchDebounceTimer, &QTimer::timeout, this, [this]() {
        performSearch(false);
    });

    setupUi();
    loadLibrary();
}

void MainWindow::setupUi()
{
    setWindowTitle("Album Collector");
    setMinimumSize(800, 500);

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Setup sidebar
    setupSidebar();

    // Setup stacked widget
    stackedWidget = new QStackedWidget;
    mainLayout->addWidget(stackedWidget);

    // Setup pages
    setupSearchPage();
    setupLibraryPage();
    setupSettingsPage();

    stackedWidget->addWidget(searchPage);
    stackedWidget->addWidget(libraryPage);
    stackedWidget->addWidget(settingsPage);

    // Set stretch factors
    mainLayout->setStretchFactor(sidebar, 0);
    mainLayout->setStretchFactor(stackedWidget, 1);

    // Add right-click menu to library list
    libraryList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(libraryList, &QListWidget::customContextMenuRequested, 
            this, [this](const QPoint& pos) {
        QMenu contextMenu(tr("Context menu"), this);
        QAction* removeAction = contextMenu.addAction("Remove Album");
        
        connect(removeAction, &QAction::triggered, 
                this, &MainWindow::removeSelectedAlbum);
        
        contextMenu.exec(libraryList->mapToGlobal(pos));
    });
}

void MainWindow::setupSidebar()
{
    // Create a container widget for the sidebar
    QWidget* sidebarContainer = new QWidget;
    sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // Create main navigation list
    sidebar = new QListWidget;
    sidebar->setFixedWidth(150);
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setSpacing(1);  // Add spacing between items for dividers
    
    QListWidgetItem* searchItem = new QListWidgetItem("Search");
    searchItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    QListWidgetItem* libraryItem = new QListWidgetItem("Library");
    libraryItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    sidebar->addItem(searchItem);
    sidebar->addItem(libraryItem);

    // Create settings button at bottom
    QListWidgetItem* settingsItem = new QListWidgetItem("Settings");
    settingsItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    // Create bottom list for settings
    QListWidget* bottomList = new QListWidget;
    bottomList->setFixedWidth(150);
    bottomList->setFrameShape(QFrame::NoFrame);
    bottomList->setMaximumHeight(50);
    bottomList->addItem(settingsItem);
    bottomList->setObjectName("bottomList"); // For finding it later

    // Add main navigation to layout
    sidebarLayout->addWidget(sidebar);
    sidebarLayout->addStretch();
    sidebarLayout->addWidget(bottomList);

    // Connect both lists to handle navigation
    auto handleNavigation = [this](QListWidget* list) {
        connect(list, &QListWidget::itemClicked, this, [this, list](QListWidgetItem* current) {
            if (!current) return;
            
            // Deselect the other list
            if (list == sidebar) {
                if (QListWidget* bottomList = findChild<QListWidget*>("bottomList")) {
                    bottomList->clearSelection();
                }
            } else {
                sidebar->clearSelection();
            }
            
            QString text = current->text();
            if (text == "Search") switchToSearch();
            else if (text == "Library") switchToLibrary();
            else if (text == "Settings") {
                switchToSettings();
                if (stackedWidget && settingsPage) {
                    stackedWidget->setCurrentWidget(settingsPage);
                }
            }
        });
    };

    handleNavigation(sidebar);
    handleNavigation(bottomList);

    mainLayout->addWidget(sidebarContainer);

    // Set initial selection
    QTimer::singleShot(0, [this]() {
        if (sidebar) sidebar->setCurrentRow(0);
    });
}

void MainWindow::setupSearchPage()
{
    searchPage = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(searchPage);
    layout->setContentsMargins(10, 10, 20, 10);
    layout->setSpacing(10);  // Restore normal spacing

    // Search section
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchBox = new QLineEdit();
    searchBox->setPlaceholderText("Enter album or artist name...");
    searchButton = new QPushButton("Search");
    searchButton->setFixedWidth(100);
    searchLayout->addWidget(searchBox);
    searchLayout->addWidget(searchButton);

    // Results list
    resultsList = new QListWidget();
    resultsList->setObjectName("resultsList");
    resultsList->setFrameShape(QFrame::NoFrame);
    resultsList->setSpacing(5);
    resultsList->setUniformItemSizes(false);
    resultsList->setResizeMode(QListView::Adjust);
    resultsList->setWordWrap(true);
    resultsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Load More button
    loadMoreButton = new QPushButton("Load More Results");
    loadMoreButton->setVisible(false);

    // Create a container for the results and load more button
    QWidget* resultsContainer = new QWidget;
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsContainer);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    resultsLayout->setSpacing(10);
    resultsLayout->addWidget(resultsList);
    resultsLayout->addWidget(loadMoreButton);

    // Status label
    statusLabel = new QLabel("");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setContentsMargins(0, 5, 0, 5);

    // Add everything to main layout
    layout->addLayout(searchLayout);
    layout->addWidget(resultsContainer, 1);
    layout->addWidget(statusLabel);

    // Update the connections to use debouncing
    connect(searchButton, &QPushButton::clicked, this, [this]() {
        if (!isSearching) {
            searchDebounceTimer->start();
        }
    });
    
    connect(searchBox, &QLineEdit::returnPressed, this, [this]() {
        if (!isSearching) {
            searchDebounceTimer->start();
        }
    });

    connect(resultsList, &QListWidget::itemDoubleClicked, this, &MainWindow::showAlbumDetails);

    // Connect Load More button
    connect(loadMoreButton, &QPushButton::clicked, this, [this]() {
        if (!isSearching) {
            currentSearchOffset += 10;
            performSearch(true);
        }
    });

    // Connect scroll event to check position
    connect(resultsList->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &MainWindow::checkScrollPosition);

    // Hide load more button by default
    loadMoreButton->setVisible(false);
}

void MainWindow::checkScrollPosition()
{
    if (!resultsList || !loadMoreButton) return;

    // Only show the load more button if:
    // 1. There are more results available
    // 2. User has scrolled near the bottom
    bool hasMoreResults = resultsList->count() < totalResults;
    bool nearBottom = (resultsList->verticalScrollBar()->value() >= 
                      resultsList->verticalScrollBar()->maximum() - 50);

    loadMoreButton->setVisible(hasMoreResults && nearBottom);
}

void MainWindow::setupLibraryPage()
{
    libraryPage = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(libraryPage);
    layout->setContentsMargins(10, 10, 20, 10);
    
    // Create toolbar container
    QWidget* toolbarWidget = new QWidget;
    toolbarWidget->setObjectName("toolbarWidget");
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(10, 5, 10, 5);
    
    // Create sort controls with better formatting
    QLabel* sortLabel = new QLabel("Sort by:");
    sortLabel->setObjectName("sortLabel");
    sortLabel->setFixedWidth(50);
    
    sortComboBox = new QComboBox;
    sortComboBox->setObjectName("sortComboBox");
    sortComboBox->setFixedWidth(150);  // Increased width to fit "Album Name"
    sortComboBox->setStyleSheet("QComboBox { padding: 2px 5px; }");  // Add some padding
    
    // Add items with consistent formatting
    sortComboBox->addItem("Artist");
    sortComboBox->addItem("Album Name");
    sortComboBox->addItem("Release Date");
    sortComboBox->addItem("Rating ↓");
    
    // Ensure the popup list is wide enough
    sortComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    
    toolbarLayout->addWidget(sortLabel);
    toolbarLayout->addWidget(sortComboBox);
    toolbarLayout->addStretch();
    
    layout->addWidget(toolbarWidget);
    
    // Create and setup library list
    libraryList = new QListWidget;
    libraryList->setObjectName("libraryList");
    libraryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    libraryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    libraryList->setSpacing(5);
    libraryList->setUniformItemSizes(false);
    libraryList->setResizeMode(QListView::Adjust);
    libraryList->setWordWrap(true);
    
    layout->addWidget(libraryList);
    
    // Connect signals
    connect(libraryList, &QListWidget::itemDoubleClicked, 
            this, &MainWindow::showAlbumDetails);
    connect(sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::sortLibrary);
    
    // Set default sort to Artist
    sortComboBox->setCurrentIndex(0);
}

void MainWindow::setupSettingsPage()
{
    settingsPage = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(settingsPage);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(20);

    // Header
    QLabel* header = new QLabel("Settings");
    header->setStyleSheet("font-size: 24px; font-weight: bold;");
    layout->addWidget(header);

    // Theme section
    QLabel* themeHeader = new QLabel("Theme");
    themeHeader->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(themeHeader);

    // Create theme list with adjusted dimensions
    themeList = new QListWidget;
    themeList->setObjectName("themeList");
    themeList->setViewMode(QListWidget::IconMode);
    themeList->setIconSize(QSize(120, 80));      // Keep preview size
    themeList->setSpacing(15);                   // Slightly increase spacing
    themeList->setResizeMode(QListWidget::Adjust);
    themeList->setMovement(QListWidget::Static);
    themeList->setWrapping(true);
    themeList->setUniformItemSizes(true);
    themeList->setGridSize(QSize(150, 110));     // Increase grid size to fit text
    themeList->setMinimumHeight(400);            // Increase minimum height to show more items

    // Update themes with more variety
    themes = {
        ThemeColors("Glacier Blue", 
            QColor("#EEF5FF"), // background
            QColor("#C4DFFF"), // sidebar
            QColor("#2C3E50"), // text
            QColor("#2E5C9A")  // accent
        ),
        ThemeColors("Volcanic Dark", 
            QColor("#1A1A1A"), // background
            QColor("#2D2D2D"), // sidebar
            QColor("#FFFFFF"), // text
            QColor("#FF4B2B")  // accent
        ),
        ThemeColors("Forest Depths", 
            QColor("#1E2923"), // background
            QColor("#2A3831"), // sidebar
            QColor("#E8F3E9"), // text
            QColor("#43A047")  // accent
        ),
        ThemeColors("Royal Purple", 
            QColor("#21152C"), // background
            QColor("#2D1B3D"), // sidebar
            QColor("#F8F0FF"), // text
            QColor("#9C27B0")  // accent
        ),
        ThemeColors("Ocean Deep", 
            QColor("#0A192F"), // background
            QColor("#112240"), // sidebar
            QColor("#E6F1FF"), // text
            QColor("#64FFDA")  // accent
        ),
        ThemeColors("Desert Night", 
            QColor("#2B2118"), // background
            QColor("#3D2E21"), // sidebar
            QColor("#FFF8F0"), // text
            QColor("#FF9800")  // accent
        ),
        ThemeColors("Cherry Blossom", 
            QColor("#FFF0F3"), // background
            QColor("#FFE1E7"), // sidebar
            QColor("#4A1B24"), // text
            QColor("#FF4D6D")  // accent
        ),
        ThemeColors("Emerald City", 
            QColor("#004D40"), // background
            QColor("#00695C"), // sidebar
            QColor("#E0F2F1"), // text
            QColor("#1DE9B6")  // accent
        ),
        ThemeColors("Nordic Frost", 
            QColor("#2E3440"), // background
            QColor("#3B4252"), // sidebar
            QColor("#ECEFF4"), // text
            QColor("#88C0D0")  // accent
        ),
        ThemeColors("Golden Hour", 
            QColor("#2C1810"), // background
            QColor("#3D2217"), // sidebar
            QColor("#FFF3E0"), // text
            QColor("#FFB74D")  // accent
        ),
        ThemeColors("Electric Dreams", 
            QColor("#16161E"), // background
            QColor("#1E1E2A"), // sidebar
            QColor("#E0E0FF"), // text
            QColor("#FF2E97")  // accent
        ),
        ThemeColors("Mint Chocolate", 
            QColor("#1B2B21"), // background
            QColor("#2A3D30"), // sidebar
            QColor("#E8F5EC"), // text
            QColor("#00BFA5")  // accent
        ),
        ThemeColors("Cosmic Purple", 
            QColor("#13111C"), // background
            QColor("#1D1A26"), // sidebar
            QColor("#E6E4F0"), // text
            QColor("#B388FF")  // accent
        ),
        ThemeColors("Ruby Red", 
            QColor("#2D1418"), // background
            QColor("#3D1D22"), // sidebar
            QColor("#FFE6E9"), // text
            QColor("#FF1744")  // accent
        ),
        ThemeColors("Teal Depths", 
            QColor("#0B3F3F"), // background
            QColor("#0F4C4C"), // sidebar
            QColor("#E0F7F7"), // text
            QColor("#26C6DA")  // accent
        ),
        ThemeColors("Creamsicle", 
            QColor("#F7E8D0"), // background - warm cream
            QColor("#E7C697"), // sidebar - soft tan
            QColor("#2C4B3B"), // text - deep forest green
            QColor("#FF7B54")  // accent - coral orange
        )
    };

    // Add theme previews with better text visibility
    for (const auto& theme : themes) {
        QPixmap preview(120, 80);
        QPainter painter(&preview);
        
        // Draw background
        painter.fillRect(0, 0, 120, 80, theme.background);
        
        // Draw sidebar representation
        painter.fillRect(0, 0, 20, 80, theme.sidebar);
        
        // Draw accent color sample
        painter.fillRect(95, 5, 20, 20, theme.accent);
        
        QListWidgetItem* item = new QListWidgetItem(theme.name);
        item->setIcon(QIcon(preview));
        themeList->addItem(item);
    }

    layout->addWidget(themeList);
    
    // Load and apply saved theme
    QSettings settings("YourCompany", "AlbumCollector");
    int savedThemeIndex = settings.value("themeIndex", 0).toInt();
    themeList->setCurrentRow(savedThemeIndex);
    applyTheme(themes[savedThemeIndex]);

    // Connect theme selection
    connect(themeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < themes.size()) {
            applyTheme(themes[row]);
            QSettings settings("YourCompany", "AlbumCollector");
            settings.setValue("themeIndex", row);
        }
    });

    layout->addStretch();
}

void MainWindow::applyTheme(const ThemeColors& theme)
{
    // Main window style
    QString mainStyle = QString(
        "QMainWindow, QWidget#centralWidget {"
        "    background-color: %1;"
        "    color: %2;"
        "}"
        "QPushButton {"
        "    background-color: %3;"
        "    color: %2;"
        "    border: none;"
        "    padding: 5px 15px;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "    color: white;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #CCCCCC;"
        "    color: #666666;"
        "}"
        "QLineEdit {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %4;"
        "    padding: 5px;"
        "    border-radius: 4px;"
        "}"
        "QListWidget {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: none;"
        "}"
        "QLabel {"
        "    color: %2;"
        "}"
        "QComboBox {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %4;"
        "    padding: 5px;"
        "    border-radius: 4px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 5px solid %2;"
        "    border-right: 5px solid transparent;"
        "    border-top: 5px solid %2;"
        "    width: 0;"
        "    height: 0;"
        "    margin-right: 5px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: %1;"
        "    color: %2;"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
        "QWidget#toolbarWidget {"
        "    background-color: %1;"
        "    color: %2;"
        "    border-bottom: 1px solid %4;"
        "}"
        "QWidget#toolbarWidget QLabel {"
        "    color: %2;"
        "}"
        "QComboBox#sortComboBox {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %4;"
        "    padding: 5px;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QComboBox#sortComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"
        "QComboBox#sortComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 5px solid %2;"
        "    border-right: 5px solid transparent;"
        "    border-top: 5px solid %2;"
        "    width: 0;"
        "    height: 0;"
        "    margin-right: 5px;"
        "}"
        "QComboBox#sortComboBox QAbstractItemView {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %4;"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
        "QComboBox#sortComboBox:hover {"
        "    border-color: %4;"
        "}"
        "QComboBox#sortComboBox:focus {"
        "    border-color: %4;"
        "    border-width: 2px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: %4;"
        "    color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: %1;"
        "    border: 1px solid %4;"
        "}"
        "QListWidget#resultsList::item:selected, QListWidget#libraryList::item:selected {"
        "    background-color: %4;"
        "}"
        "QListWidget#resultsList::item:hover:!selected, QListWidget#libraryList::item:hover:!selected {"
        "    background-color: %1;"
        "    border: 1px solid %4;"
        "}"
        "QListWidget#themeList {"
        "    background-color: %1;"
        "    color: %2;"
        "}"
        "QListWidget#themeList::item {"
        "    color: %2;"
        "    background-color: transparent;"
        "}"
        "QListWidget#themeList::item:selected {"
        "    background-color: %4;"
        "    color: white;"
        "}"
        "QPushButton[isAdded=\"true\"] {"
        "    font-size: 11px;"
        "    background-color: %4;"
        "    color: white;"
        "}"
        "QPushButton[isAdded=\"false\"] {"
        "    font-size: 11px;"
        "    background-color: %3;"
        "    color: %2;"
        "}"
        "QWidget {"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
        "QLineEdit {"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
        "QLabel {"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
        "QListWidget {"
        "    selection-background-color: %4;"
        "    selection-color: white;"
        "}"
    ).arg(theme.background.name(),
          theme.text.name(),
          theme.sidebar.name(),
          theme.accent.name());

    // Sidebar specific style
    QString sidebarStyle = QString(
        "QListWidget {"
        "    background-color: %1;"
        "    border: none;"
        "    color: %2;"
        "}"
        "QListWidget::item {"
        "    padding: 12px 15px;"
        "    border: none;"
        "    color: %2;"
        "    font-size: 16px;"     // Increased from 14px to 16px
        "    font-weight: bold;"    // Added bold
        "    border-bottom: 1px solid %4;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: %3;"
        "    color: white;"
        "    border-bottom: 1px solid %3;"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: %4;"
        "    color: white;"
        "}"
    ).arg(theme.sidebar.name(),
          theme.text.name(),
          theme.accent.name(),
          theme.sidebar.darker(120).name());

    // Apply styles
    this->setStyleSheet(mainStyle);
    
    // Apply sidebar style to both lists
    sidebar->setStyleSheet(sidebarStyle);
    if (QListWidget* bottomList = sidebar->parentWidget()->findChild<QListWidget*>("bottomList")) {
        bottomList->setStyleSheet(sidebarStyle);
    }

    // Apply container style to ensure full height coloring
    sidebar->parentWidget()->setStyleSheet(QString(
        "background-color: %1;"
    ).arg(theme.sidebar.name()));

    // Ensure the settings page is properly initialized
    if (!settingsPage) {
        setupSettingsPage();
    }

    // Update the application palette for dynamic colors
    QPalette pal = palette();
    pal.setColor(QPalette::Highlight, theme.accent);
    pal.setColor(QPalette::Button, theme.sidebar);
    QApplication::setPalette(pal);
}

void MainWindow::performSearch(bool loadingMore)
{
    if (isSearching) return;

    if (!loadingMore) {
        // New search, reset everything
        currentSearchQuery = searchBox->text().trimmed();
        currentSearchOffset = 0;
        if (currentSearchQuery.isEmpty()) {
        statusLabel->setText("Please enter a search term");
        return;
        }
        resultsList->clear();
        resultsList->scrollToTop();  // Ensure we start at the top for new searches
    }

    // Disable controls
    isSearching = true;
    searchBox->setEnabled(false);
    searchButton->setEnabled(false);
    loadMoreButton->setEnabled(false);
    statusLabel->setText("Searching...");

    try {
        auto searchResult = spotify.searchAlbums(currentSearchQuery.toStdString(), currentSearchOffset);
        
        if (!loadingMore) {
            totalResults = searchResult.total;
        }

        displayResults(searchResult.albums, loadingMore);
        
        // Update status
        statusLabel->setText(QString("Showing %1 of %2 results")
            .arg(resultsList->count())
            .arg(totalResults));
        
        // Check scroll position after new results are added
        checkScrollPosition();
    }
    catch (const std::exception& e) {
        statusLabel->setText("Error performing search");
        QMessageBox::critical(this, "Error", e.what());
    }

    // Re-enable controls
    isSearching = false;
    searchBox->setEnabled(true);
    searchButton->setEnabled(true);
    loadMoreButton->setEnabled(true);
}

void MainWindow::displayResults(const std::vector<Album>& albums, bool append)
{
    if (!append) {
        resultsList->clear();
    }

    for (const auto& album : albums) {
        QListWidgetItem* item = new QListWidgetItem(resultsList);
        AlbumListItem* widget = new AlbumListItem(album, nullptr, false);  // false for search results
        
        bool isInLibrary = libraryAlbumIds.find(album.id) != libraryAlbumIds.end();
        if (isInLibrary) {
            widget->setAddToLibraryState(false, "Added");
        }
        
        item->setSizeHint(widget->sizeHint());
        resultsList->addItem(item);
        resultsList->setItemWidget(item, widget);
        
        downloadAlbumArt(QString::fromStdString(album.image_url), widget);
    }
}

void MainWindow::downloadAlbumArt(const QString& url, AlbumListItem* item)
{
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    reply->setProperty("itemPtr", QVariant::fromValue(reinterpret_cast<quintptr>(item)));
}

void MainWindow::handleImageDownloaded(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QPixmap pixmap;
        pixmap.loadFromData(data);
        
        AlbumListItem* item = reinterpret_cast<AlbumListItem*>(
            reply->property("itemPtr").value<quintptr>());
        if (item) {
            item->setImage(pixmap);
        }
    }
    reply->deleteLater();
}

void MainWindow::showAlbumDetails(QListWidgetItem* item)
{
    AlbumListItem* widget = qobject_cast<AlbumListItem*>(
        item->listWidget()->itemWidget(item));
    if (!widget) return;
    
    const Album& album = widget->album();
    
    QDialog* detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle("Album Details");
    detailsDialog->setMinimumWidth(400);

    QGridLayout* layout = new QGridLayout(detailsDialog);
    
    layout->addWidget(new QLabel("Album:"), 0, 0);
    layout->addWidget(new QLabel(QString::fromStdString(album.name)), 0, 1);
    
    layout->addWidget(new QLabel("Artist:"), 1, 0);
    layout->addWidget(new QLabel(QString::fromStdString(album.artist)), 1, 1);
    
    layout->addWidget(new QLabel("Release Date:"), 2, 0);
    layout->addWidget(new QLabel(formatDate(album.release_date)), 2, 1);
    
    layout->addWidget(new QLabel("Spotify ID:"), 3, 0);
    layout->addWidget(new QLabel(QString::fromStdString(album.id)), 3, 1);

    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    
    layout->addWidget(closeButton, 4, 0, 1, 2);
    
    detailsDialog->exec();
}

void MainWindow::switchToSearch()
{
    if (stackedWidget && searchPage) {
        stackedWidget->setCurrentWidget(searchPage);
    }
}

void MainWindow::switchToLibrary()
{
    if (stackedWidget && libraryPage) {
        stackedWidget->setCurrentWidget(libraryPage);
    }
}

void MainWindow::switchToSettings()
{
    if (stackedWidget && settingsPage) {
        stackedWidget->setCurrentWidget(settingsPage);
    }
}

void MainWindow::addToLibrary(const Album& album, const QPixmap& albumArt)
{
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    albumArt.save(&buffer, "PNG");

    libraryAlbums.append(LibraryAlbum(album, imageData));
    libraryAlbumIds.insert(album.id); // Insert into the set
    
    // Apply current sorting before refreshing display
    sortLibrary(sortComboBox->currentIndex());
    saveLibrary();  // Save after adding
}

void MainWindow::refreshLibraryDisplay()
{
    libraryList->clear();
    for (const auto& libAlbum : libraryAlbums) {
        QListWidgetItem* item = new QListWidgetItem(libraryList);
        AlbumListItem* widget = new AlbumListItem(libAlbum.album, nullptr, true);  // true for library view
        
        QPixmap pixmap;
        pixmap.loadFromData(libAlbum.imageData);
        widget->setImage(pixmap);
        widget->setAddToLibraryVisible(false);
        widget->setRating(libAlbum.album.rating);
        
        item->setSizeHint(widget->sizeHint());
        libraryList->addItem(item);
        libraryList->setItemWidget(item, widget);
    }
}

void MainWindow::saveLibrary()
{
    QString libraryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(libraryPath);
    QString filePath = libraryPath + "/library.dat";
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out.setVersion(QDataStream::Qt_6_0);
        
        const quint32 magic = 0x41434D47;
        const quint32 version = 2;  // Version 2 includes ratings
        out << magic << version;
        out << quint32(libraryAlbums.size());
        
        for (const auto& libAlbum : libraryAlbums) {
            out << QString::fromStdString(libAlbum.album.name)
                << QString::fromStdString(libAlbum.album.artist)
                << QString::fromStdString(libAlbum.album.id)
                << QString::fromStdString(libAlbum.album.release_date)
                << QString::fromStdString(libAlbum.album.image_url)
                << libAlbum.imageData
                << qint32(libAlbum.album.rating);  // Explicitly save as qint32
        }
        file.close();
    }
}

void MainWindow::loadLibrary()
{
    QString libraryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString filePath = libraryPath + "/library.dat";
    
    if (QFile::exists(filePath)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QDataStream in(&file);
            in.setVersion(QDataStream::Qt_6_0);
            
            quint32 magic, version;
            in >> magic >> version;
            
            if (magic != 0x41434D47) return;
            
            libraryAlbums.clear();
            libraryAlbumIds.clear();
            
            quint32 size;
            in >> size;
            
            for (quint32 i = 0; i < size; ++i) {
                QString name, artist, id, release_date, image_url;
                QByteArray imageData;
                qint32 rating = 0;  // Use qint32 for consistency
                
                in >> name >> artist >> id >> release_date >> image_url >> imageData;
                
                if (version >= 2) {
                    in >> rating;
                }
                
                Album album(name.toStdString(), artist.toStdString(), 
                           id.toStdString(), release_date.toStdString(), 
                           image_url.toStdString());
                album.rating = rating;
                libraryAlbums.append(LibraryAlbum(album, imageData));
                libraryAlbumIds.insert(album.id);
            }
            
            file.close();
            refreshLibraryDisplay();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveLibrary();
    event->accept();
}

QString MainWindow::formatDate(const std::string& dateStr)
{
    QDate date = QDate::fromString(QString::fromStdString(dateStr), "yyyy-MM-dd");
    if (date.isValid()) {
        return date.toString("MMMM d, yyyy");
    }
    return QString::fromStdString(dateStr);
}

void MainWindow::sortLibrary(int sortIndex)
{
    switch (sortIndex) {
        case 0: // Artist
            std::sort(libraryAlbums.begin(), libraryAlbums.end(),
                [](const LibraryAlbum& a, const LibraryAlbum& b) {
                    QString artistA = QString::fromStdString(a.album.artist).toLower();
                    QString artistB = QString::fromStdString(b.album.artist).toLower();
                    
                    if (artistA == artistB) {
                        // If same artist, sort by release date (newest first)
                        QDate dateA = QDate::fromString(QString::fromStdString(a.album.release_date), "yyyy-MM-dd");
                        QDate dateB = QDate::fromString(QString::fromStdString(b.album.release_date), "yyyy-MM-dd");
                        if (!dateA.isValid() || !dateB.isValid()) {
                            return a.album.release_date > b.album.release_date;
                        }
                        return dateA > dateB;
                    }
                    return artistA < artistB;
                });
            break;
            
        case 1: // Album Name
            std::sort(libraryAlbums.begin(), libraryAlbums.end(),
                [](const LibraryAlbum& a, const LibraryAlbum& b) {
                    return QString::fromStdString(a.album.name).toLower() < 
                           QString::fromStdString(b.album.name).toLower();
                });
            break;
            
        case 2: // Release Date
            std::sort(libraryAlbums.begin(), libraryAlbums.end(),
                [](const LibraryAlbum& a, const LibraryAlbum& b) {
                    QDate dateA = QDate::fromString(QString::fromStdString(a.album.release_date), "yyyy-MM-dd");
                    QDate dateB = QDate::fromString(QString::fromStdString(b.album.release_date), "yyyy-MM-dd");
                    if (!dateA.isValid() || !dateB.isValid()) {
                        return a.album.release_date < b.album.release_date;
                    }
                    return dateA < dateB;
                });
            break;
            
        case 3: // Rating (highest to lowest)
            std::sort(libraryAlbums.begin(), libraryAlbums.end(),
                [](const LibraryAlbum& a, const LibraryAlbum& b) {
                    // Put unrated albums at the bottom
                    if (a.album.rating == 0 && b.album.rating > 0) return false;
                    if (b.album.rating == 0 && a.album.rating > 0) return true;
                    
                    // Sort by rating (highest to lowest)
                    if (a.album.rating != b.album.rating) {
                        return a.album.rating > b.album.rating;
                    }
                    
                    // If ratings are equal, sort by artist name
                    return QString::fromStdString(a.album.artist).toLower() < 
                           QString::fromStdString(b.album.artist).toLower();
                });
            break;
    }
    
    refreshLibraryDisplay();
}

void MainWindow::removeSelectedAlbum()
{
    QListWidgetItem* currentItem = libraryList->currentItem();
    if (!currentItem) return;
    
    int row = libraryList->row(currentItem);
    if (row >= 0 && row < static_cast<int>(libraryAlbums.size())) {
        // Remove the album ID from the set
        libraryAlbumIds.erase(libraryAlbums[row].album.id);
        
        // Remove from both UI and data
        delete libraryList->takeItem(row);
        libraryAlbums.remove(row);
        saveLibrary(); // Save changes to file
    }
}

MainWindow::~MainWindow()
{
    saveLibrary();  // Ensure library is saved on destruction
}

void MainWindow::updateAlbumRating(const std::string& albumId, int rating) {
    for (auto& libAlbum : libraryAlbums) {
        if (libAlbum.album.id == albumId) {
            libAlbum.album.rating = rating;
            break;
        }
    }
    saveLibrary();
    
    // If currently sorted by rating, refresh the display
    if (sortComboBox->currentIndex() == 3) {
        sortLibrary(3);
    }
} 