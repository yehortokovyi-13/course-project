#include "book.h"

Book::Book(QString title, QString author, QString genre, int year, int quantity, QString imagePath)
    : m_title(title), m_author(author), m_genre(genre),
    m_year(year), m_quantity(quantity), m_imagePath(imagePath) {}

QString Book::title() const { return m_title; }
QString Book::author() const { return m_author; }
QString Book::genre() const { return m_genre; }
int Book::year() const { return m_year; }
int Book::quantity() const { return m_quantity; }
QString Book::imagePath() const { return m_imagePath; }

void Book::setTitle(const QString &t) { m_title = t; }
void Book::setAuthor(const QString &a) { m_author = a; }
void Book::setGenre(const QString &g) { m_genre = g; }
void Book::setYear(int y) { m_year = y; }
void Book::setQuantity(int q) { m_quantity = q; }
void Book::setImagePath(const QString &p) { m_imagePath = p; }

QJsonObject Book::toJson() const {
    QJsonObject obj;
    obj["title"] = m_title;
    obj["author"] = m_author;
    obj["genre"] = m_genre;
    obj["year"] = m_year;
    obj["quantity"] = m_quantity;
    obj["imagePath"] = m_imagePath;
    return obj;
}

Book Book::fromJson(const QJsonObject &obj) {
    return Book(
        obj["title"].toString(),
        obj["author"].toString(),
        obj["genre"].toString(),
        obj["year"].toInt(),
        obj["quantity"].toInt(),
        obj["imagePath"].toString()
        );
}
