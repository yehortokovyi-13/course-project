#include "mainwindow.h"
#include "ui_mainwindow.h"

// --- ДОДАНІ БІБЛІОТЕКИ ДЛЯ PDF ---
#include <QPdfWriter>
#include <QPainter>
#include <QDate>
// ---------------------------------

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>
#include <QFont>
#include <QUrlQuery>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowIcon(QIcon("icon.png"));

    // 1. Ініціалізація мережі
    m_netManager = new QNetworkAccessManager(this);

    // 2. Налаштування таблиці
    ui->tableWidget->setColumnCount(6);
    QStringList headers = {"Cover", "Title", "Author", "Genre", "Year", "Qty"};
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // Налаштування вигляду таблиці
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(90);
    ui->tableWidget->setIconSize(QSize(60, 80));

    // --- БЛОК РОЗТЯГУВАННЯ КОЛОНОК ---
    QHeaderView *header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch); // Всі тягнуться
    header->setSectionResizeMode(0, QHeaderView::Fixed); // Перша (Cover) фіксована
    ui->tableWidget->setColumnWidth(0, 80);

    // 3. Створення папки для обкладинок
    if (!QDir("covers").exists()) {
        QDir().mkdir("covers");
    }

    // ВАЖЛИВО: Я видалив ручний connect для exportPdfButton,
    // щоб вікно не відкривалося двічі.

    // 4. Завантаження даних при старті
    loadBooks();
    refreshTable(m_books);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- ПОШУК (API) ---

void MainWindow::on_searchButton_clicked()
{
    QString query = ui->searchEdit->text().trimmed();
    if (query.isEmpty()) {
        QMessageBox::information(this, "Info", "Введіть назву книги для пошуку.");
        return;
    }

    QUrl url("https://www.googleapis.com/books/v1/volumes");
    QUrlQuery params;
    params.addQueryItem("q", "intitle:" + query);
    params.addQueryItem("maxResults", "1");
    url.setQuery(params);

    QNetworkRequest req(url);
    QNetworkReply *reply = m_netManager->get(req);

    disconnect(m_netManager, nullptr, nullptr, nullptr);

    connect(reply, &QNetworkReply::finished, [this, reply](){ onApiSearchResult(reply); });
}

void MainWindow::onApiSearchResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "API Error", "Помилка пошуку: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();

    QJsonObject root = doc.object();
    if (!root.contains("items")) {
        QMessageBox::information(this, "Not Found", "Книг не знайдено.");
        return;
    }

    QJsonArray items = root["items"].toArray();
    if (items.isEmpty()) return;

    QJsonObject volumeInfo = items[0].toObject()["volumeInfo"].toObject();

    ui->titleEdit->setText(volumeInfo["title"].toString());

    if (volumeInfo.contains("authors")) {
        QJsonArray authors = volumeInfo["authors"].toArray();
        if (!authors.isEmpty()) ui->authorEdit->setText(authors[0].toString());
    } else {
        ui->authorEdit->setText("Невідомий автор");
    }

    if (volumeInfo.contains("categories")) {
        QJsonArray cats = volumeInfo["categories"].toArray();
        if (!cats.isEmpty()) ui->genreEdit->setText(cats[0].toString());
    } else {
        ui->genreEdit->setText("General");
    }

    if (volumeInfo.contains("publishedDate")) {
        ui->yearEdit->setText(volumeInfo["publishedDate"].toString().left(4));
    }

    ui->quantityEdit->clear();
    ui->quantityEdit->setPlaceholderText("Введіть кількість...");
    ui->quantityEdit->setFocus();

    if (volumeInfo.contains("imageLinks")) {
        QString imgUrl = volumeInfo["imageLinks"].toObject()["thumbnail"].toString();
        imgUrl.replace("http://", "https://");

        QNetworkRequest req((QUrl(imgUrl)));
        QNetworkReply *imgReply = m_netManager->get(req);
        connect(imgReply, &QNetworkReply::finished, [this, imgReply](){ onCoverDownloadFinished(imgReply); });
    } else {
        m_currentTempImage = "";
        displayCoverInLabel("");
    }
}

void MainWindow::onCoverDownloadFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();

        QString tempName = "temp_download_cover.jpg";
        QFile file(tempName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            m_currentTempImage = tempName;
            displayCoverInLabel(m_currentTempImage);
        }
    }
    reply->deleteLater();
}

// --- КНОПКИ ---

void MainWindow::on_addButton_clicked()
{
    QString title = ui->titleEdit->text().trimmed();
    QString qtyText = ui->quantityEdit->text().trimmed();

    if (title.isEmpty()) {
        QMessageBox::warning(this, "Помилка", "Назва книги не може бути порожньою.");
        return;
    }
    if (qtyText.isEmpty()) {
        QMessageBox::warning(this, "Увага", "Будь ласка, введіть кількість книг.");
        ui->quantityEdit->setFocus();
        return;
    }

    QString finalPath;

    if (!m_currentTempImage.isEmpty() && QFile::exists(m_currentTempImage)) {
        finalPath = saveImageToCoversFolder(m_currentTempImage, title);
    } else {
        finalPath = generateDefaultCover(title);
    }

    Book b(title, ui->authorEdit->text(), ui->genreEdit->text(),
           ui->yearEdit->text().toInt(), qtyText.toInt(), finalPath);

    m_books.append(b);
    refreshTable(m_books);
    saveBooks();

    ui->titleEdit->clear(); ui->authorEdit->clear(); ui->genreEdit->clear();
    ui->yearEdit->clear(); ui->quantityEdit->clear(); ui->searchEdit->clear();
    m_currentTempImage = "";
    displayCoverInLabel("");
}

void MainWindow::on_editButton_clicked()
{
    int row = ui->tableWidget->currentRow();
    if (row < 0 || row >= m_books.size()) {
        QMessageBox::warning(this, "Увага", "Виберіть книгу для редагування.");
        return;
    }

    QString title = ui->titleEdit->text();
    QString oldPath = m_books[row].imagePath();
    QString finalPath = oldPath;

    if (m_currentTempImage != oldPath && !m_currentTempImage.isEmpty()) {
        finalPath = saveImageToCoversFolder(m_currentTempImage, title);
    }

    m_books[row] = Book(title, ui->authorEdit->text(), ui->genreEdit->text(),
                        ui->yearEdit->text().toInt(), ui->quantityEdit->text().toInt(), finalPath);

    refreshTable(m_books);
    saveBooks();
}

void MainWindow::on_deleteButton_clicked()
{
    int row = ui->tableWidget->currentRow();
    if (row >= 0 && row < m_books.size()) {
        auto reply = QMessageBox::question(this, "Видалення", "Видалити обрану книгу?", QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_books.removeAt(row);
            refreshTable(m_books);
            saveBooks();
            showSelectedBookInInputs();
        }
    } else {
        QMessageBox::warning(this, "Увага", "Виберіть книгу для видалення.");
    }
}

void MainWindow::on_saveButton_clicked()
{
    saveBooks();
    QMessageBox::information(this, "Інфо", "Список книг збережено!");
}

void MainWindow::on_loadButton_clicked()
{
    loadBooks();
    refreshTable(m_books);
}

void MainWindow::on_uploadCoverButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Оберіть обкладинку", "", "Images (*.jpg *.png *.jpeg)");
    if (!fileName.isEmpty()) {
        m_currentTempImage = fileName;
        displayCoverInLabel(m_currentTempImage);
    }
}

// --- ДОПОМІЖНІ ФУНКЦІЇ ---

QString MainWindow::generateDefaultCover(const QString &title)
{
    QPixmap pixmap(150, 200);
    pixmap.fill(QColor("#454545"));

    QPainter painter(&pixmap);
    painter.setPen(Qt::white);
    painter.drawRect(5, 5, 140, 190);

    QFont font = painter.font();
    font.setPixelSize(60);
    font.setBold(true);
    painter.setFont(font);

    QString letter = title.left(1).toUpper();
    painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);

    QString tempName = "temp_default_gen.jpg";
    pixmap.save(tempName);

    return saveImageToCoversFolder(tempName, title);
}

void MainWindow::displayCoverInLabel(const QString &path)
{
    QPixmap pix;
    if (!path.isEmpty() && QFile::exists(path)) {
        pix.load(path);
    } else {
        pix = QPixmap(150, 200);
        pix.fill(Qt::transparent);
    }

    if (ui->coverLabel)
        ui->coverLabel->setPixmap(pix.scaled(ui->coverLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QString MainWindow::saveImageToCoversFolder(const QString &tempPath, const QString &title)
{
    if (tempPath.isEmpty() || !QFile::exists(tempPath)) return "";

    QString safeTitle = title;
    safeTitle.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");

    QString newName = QString("covers/%1_%2.jpg")
                          .arg(safeTitle)
                          .arg(QDateTime::currentMSecsSinceEpoch());

    if (QFile::copy(tempPath, newName)) {
        return newName;
    }
    return tempPath;
}

void MainWindow::refreshTable(const QVector<Book> &list)
{
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(list.size());

    for (int i = 0; i < list.size(); ++i) {
        QTableWidgetItem *imgItem = new QTableWidgetItem;
        QPixmap p(list[i].imagePath());
        if (p.isNull()) {
            p = QPixmap(60, 80); p.fill(Qt::gray);
        }
        imgItem->setIcon(QIcon(p));
        ui->tableWidget->setItem(i, 0, imgItem);

        ui->tableWidget->setItem(i, 1, new QTableWidgetItem(list[i].title()));
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(list[i].author()));
        ui->tableWidget->setItem(i, 3, new QTableWidgetItem(list[i].genre()));
        ui->tableWidget->setItem(i, 4, new QTableWidgetItem(QString::number(list[i].year())));
        ui->tableWidget->setItem(i, 5, new QTableWidgetItem(QString::number(list[i].quantity())));
    }
}

void MainWindow::showSelectedBookInInputs()
{
    int row = ui->tableWidget->currentRow();
    if (row < 0 || row >= m_books.size()) {
        ui->titleEdit->clear(); ui->authorEdit->clear();
        ui->genreEdit->clear(); ui->yearEdit->clear(); ui->quantityEdit->clear();
        m_currentTempImage = "";
        displayCoverInLabel("");
        return;
    }

    const Book &b = m_books[row];
    ui->titleEdit->setText(b.title());
    ui->authorEdit->setText(b.author());
    ui->genreEdit->setText(b.genre());
    ui->yearEdit->setText(QString::number(b.year()));
    ui->quantityEdit->setText(QString::number(b.quantity()));

    m_currentTempImage = b.imagePath();
    displayCoverInLabel(m_currentTempImage);
}

void MainWindow::on_tableWidget_itemSelectionChanged()
{
    showSelectedBookInInputs();
}

void MainWindow::loadBooks()
{
    QFile file("books.json");
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_books.clear();
    QJsonArray arr = doc.array();
    for (const auto &v : arr) m_books.append(Book::fromJson(v.toObject()));
    file.close();
}

void MainWindow::saveBooks()
{
    QJsonArray arr;
    for (const auto &b : m_books) arr.append(b.toJson());
    QFile file("books.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
    }
    file.close();
}

void MainWindow::on_filterButton_clicked()
{
    QString author = ui->authorFilterEdit->text().trimmed();
    QString genre = ui->genreFilterEdit->text().trimmed();
    QString yearText = ui->yearFilterEdit->text().trimmed();

    QVector<Book> filtered;
    for (const Book &b : m_books) {
        bool ok = true;
        if (!author.isEmpty() && !b.author().contains(author, Qt::CaseInsensitive)) ok = false;
        if (!genre.isEmpty() && !b.genre().contains(genre, Qt::CaseInsensitive)) ok = false;
        if (!yearText.isEmpty() && QString::number(b.year()) != yearText) ok = false;
        if (ok) filtered.append(b);
    }
    refreshTable(filtered);
}

void MainWindow::on_clearFilterButton_clicked()
{
    ui->authorFilterEdit->clear();
    ui->genreFilterEdit->clear();
    ui->yearFilterEdit->clear();
    refreshTable(m_books);
}

// --- ЕКСПОРТ В PDF (Виправлений) ---
void MainWindow::on_exportPdfButton_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Зберегти звіт у PDF", "", "PDF Files (*.pdf)");
    if (fileName.isEmpty()) return;

    QPdfWriter writer(fileName);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setCreator("Book Manager App");

    QPainter painter(&writer);
    int pageWidth = writer.width();
    int pageHeight = writer.height();
    int y = 1000;

    // --- ЗАГОЛОВОК ---
    QFont titleFont("Arial", 20, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, y, pageWidth, 1000), Qt::AlignCenter, "Звіт бібліотеки");
    y += 1500;

    QFont dateFont("Arial", 10);
    painter.setFont(dateFont);
    painter.drawText(QRect(0, y, pageWidth, 500), Qt::AlignRight, "Дата: " + QDate::currentDate().toString("dd.MM.yyyy") + "   ");
    y += 800;

    // --- ШАПКА ТАБЛИЦІ ---
    int colTitleW = pageWidth * 0.35;
    int colAuthorW = pageWidth * 0.25;
    int colGenreW = pageWidth * 0.20;
    int colYearW = pageWidth * 0.10;
    int colQtyW = pageWidth * 0.10;

    int xTitle = 0;
    int xAuthor = xTitle + colTitleW;
    int xGenre = xAuthor + colAuthorW;
    int xYear = xGenre + colGenreW;
    int xQty = xYear + colYearW;

    QFont headerFont("Arial", 10, QFont::Bold);
    painter.setFont(headerFont);

    int headerHeight = 500;
    painter.drawText(QRect(xTitle, y, colTitleW, headerHeight), Qt::AlignVCenter | Qt::AlignLeft, " Назва");
    painter.drawText(QRect(xAuthor, y, colAuthorW, headerHeight), Qt::AlignVCenter | Qt::AlignLeft, " Автор");
    painter.drawText(QRect(xGenre, y, colGenreW, headerHeight), Qt::AlignVCenter | Qt::AlignLeft, " Жанр");
    painter.drawText(QRect(xYear, y, colYearW, headerHeight), Qt::AlignVCenter | Qt::AlignCenter, "Рік");
    painter.drawText(QRect(xQty, y, colQtyW, headerHeight), Qt::AlignVCenter | Qt::AlignCenter, "К-сть");

    y += headerHeight;
    painter.drawLine(0, y, pageWidth, y);
    y += 200;

    // --- ДАНІ (З АВТОМАТИЧНОЮ ВИСОТОЮ І ПЕРЕНОСОМ) ---
    QFont textFont("Arial", 10);
    painter.setFont(textFont);

    // Вмикаємо перенос слів
    int textFlags = Qt::TextWordWrap | Qt::AlignTop | Qt::AlignLeft;

    for (const Book &b : m_books) {
        // 1. Розраховуємо необхідну висоту рядка
        // Ми запитуємо у painter: "Скільки місця займе цей текст, якщо його ширина буде ось така?"
        QRect rTitle = painter.boundingRect(QRect(0, 0, colTitleW, 0), textFlags, " " + b.title());
        QRect rAuthor = painter.boundingRect(QRect(0, 0, colAuthorW, 0), textFlags, " " + b.author());
        QRect rGenre = painter.boundingRect(QRect(0, 0, colGenreW, 0), textFlags, " " + b.genre());

        // Вибираємо максимальну висоту, щоб нічого не налізло, але не менше 400
        int rowHeight = 400;
        rowHeight = qMax(rowHeight, rTitle.height());
        rowHeight = qMax(rowHeight, rAuthor.height());
        rowHeight = qMax(rowHeight, rGenre.height());
        rowHeight += 100; // Трохи повітря знизу

        // 2. Перевірка на кінець сторінки
        if (y + rowHeight > pageHeight - 1000) {
            writer.newPage();
            y = 1000;
        }

        // 3. Малюємо дані в прямокутниках з розрахованою висотою
        painter.drawText(QRect(xTitle, y, colTitleW, rowHeight), textFlags, " " + b.title());
        painter.drawText(QRect(xAuthor, y, colAuthorW, rowHeight), textFlags, " " + b.author());
        painter.drawText(QRect(xGenre, y, colGenreW, rowHeight), textFlags, " " + b.genre());

        // Рік і кількість (центруємо по вертикалі)
        painter.drawText(QRect(xYear, y, colYearW, rowHeight), Qt::AlignTop | Qt::AlignCenter, QString::number(b.year()));
        painter.drawText(QRect(xQty, y, colQtyW, rowHeight), Qt::AlignTop | Qt::AlignCenter, QString::number(b.quantity()));

        // Лінія-розділювач (сіра)
        painter.setPen(QPen(Qt::lightGray));
        painter.drawLine(0, y + rowHeight, pageWidth, y + rowHeight);
        painter.setPen(QPen(Qt::black));

        y += rowHeight + 100; // Наступний рядок
    }

    painter.end();
    QMessageBox::information(this, "Успіх", "Звіт успішно збережено у PDF!");
}
