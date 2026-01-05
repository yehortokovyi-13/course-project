#ifndef BOOK_H
#define BOOK_H

#include <QString>
#include <QJsonObject>

class Book
{
public:
    Book(QString title = "", QString author = "", QString genre = "",
         int year = 0, int quantity = 0, QString imagePath = "");

    QString title() const;
    QString author() const;
    QString genre() const;
    int year() const;
    int quantity() const;
    QString imagePath() const; // Геттер

    void setTitle(const QString &);
    void setAuthor(const QString &);
    void setGenre(const QString &);
    void setYear(int);
    void setQuantity(int);
    void setImagePath(const QString &); // Сеттер

    QJsonObject toJson() const;
    static Book fromJson(const QJsonObject &obj);

private:
    QString m_title;
    QString m_author;
    QString m_genre;
    int m_year;
    int m_quantity;
    QString m_imagePath; // Шлях до картинки
};

#endif // BOOK_H
