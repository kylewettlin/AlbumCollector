#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

class Application : public QApplication {
public:
    Application(int &argc, char **argv) : QApplication(argc, argv) {}
    
    MainWindow* mainWindow = nullptr;

    bool event(QEvent* event) override {
        if (event->type() == QEvent::Close || event->type() == QEvent::Quit) {
            qDebug() << "Application event received:" << event->type();
            if (mainWindow) {
                qDebug() << "Saving library from application event handler";
                mainWindow->saveLibrary();
            } else {
                qDebug() << "Warning: mainWindow is null in event handler";
            }
        }
        return QApplication::event(event);
    }
};

int main(int argc, char *argv[])
{
    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        Application app(argc, argv);
        
        MainWindow window;
        app.mainWindow = &window;
        window.show();
        
        // Handle macOS specific quit events
        QObject::connect(&app, &QApplication::aboutToQuit, [&window]() {
            qDebug() << "aboutToQuit signal received";
            window.saveLibrary();
        });
        
        int result = app.exec();
        curl_global_cleanup();
        return result;
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Startup Error",
            QString("Failed to start application: %1").arg(e.what()));
        return 1;
    } catch (...) {
        QMessageBox::critical(nullptr, "Startup Error",
            "Failed to start application: Unknown error");
        return 1;
    }
}
