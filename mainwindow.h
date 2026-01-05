#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "book.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоти кнопок
    void on_addButton_clicked();
    void on_editButton_clicked();
    void on_deleteButton_clicked();
    void on_saveButton_clicked();
    void on_loadButton_clicked();
    void on_exportPdfButton_clicked();
    void on_searchButton_clicked();     // Кнопка пошуку API
    void on_uploadCoverButton_clicked();// Кнопка завантаження свого фото

    // --- Слоти таблиці та фільтрів ---
    void on_tableWidget_itemSelectionChanged();
    void on_filterButton_clicked();
    void on_clearFilterButton_clicked();

    // --- Слоти для роботи з мережею (API) ---
    void onApiSearchResult(QNetworkReply *reply);
    void onCoverDownloadFinished(QNetworkReply *reply);

private:
    Ui::MainWindow *ui;

    // Список книг
    QVector<Book> m_books;

    // Менеджер для інтернету
    QNetworkAccessManager *m_netManager;

    // Тимчасовий шлях до поточної картинки (поки не натиснули Add)
    QString m_currentTempImage;

    // --- Допоміжні функції ---
    void refreshTable(const QVector<Book> &list);
    void loadBooks();
    void saveBooks();
    void showSelectedBookInInputs();

    // Відобразити картинку в лівому віконці
    void displayCoverInLabel(const QString &path);

    // Зберегти картинку в папку covers
    QString saveImageToCoversFolder(const QString &tempPath, const QString &title);

    // [ВАЖЛИВО] Ось функція, якої не вистачало:
    QString generateDefaultCover(const QString &title);
};

#endif // MAINWINDOW_H
