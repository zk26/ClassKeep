/*
 * ClassroomWordCloud.h - per student interest word cloud (plain SVG, no external deps)
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QByteArray>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVector>

class ClassroomWordCloud
{
public:
	struct Word
	{
		QString text;
		int count = 0;
		double weight = 0.0;
	};

	struct PlacedWord
	{
		QString text;
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		int fontSize = 12;
		QString color;
	};

	static QStringList defaultStopWords();
	static QVector<Word> tokenize( const QStringList& lines,
								   const QStringList& extraKeywords = QStringList{},
								   int maxWords = 60,
								   const QStringList& extraStopWords = QStringList{} );
	static QVector<PlacedWord> layout( const QVector<Word>& words, int width = 900, int height = 560 );
	static QByteArray toSvg( const QVector<Word>& words, int width = 900, int height = 560 );
	static QPixmap toPixmap( const QVector<Word>& words, int width = 900, int height = 560 );
};
