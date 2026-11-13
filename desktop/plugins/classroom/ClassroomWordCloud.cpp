/*
 * ClassroomWordCloud.cpp - per student interest word cloud (plain SVG, no external deps)
 *
 * This file is part of the Ikeback classroom modification.
 */

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QMap>
#include <QPainter>
#include <QRect>
#include <QRegularExpression>
#include <QSet>

#include "ClassroomWordCloud.h"


QStringList ClassroomWordCloud::defaultStopWords()
{
	return { QStringLiteral("窗口"), QStringLiteral("程序"), QStringLiteral("桌面"), QStringLiteral("任务"),
			 QStringLiteral("浏览"), QStringLiteral("页面"), QStringLiteral("文件"), QStringLiteral("打开"),
			 QStringLiteral("使用"), QStringLiteral("正在"), QStringLiteral("当前"), QStringLiteral("活动"),
			 QStringLiteral("学生"), QStringLiteral("老师"), QStringLiteral("登录"), QStringLiteral("连接"),
			 QStringLiteral("系统"), QStringLiteral("屏幕"), QStringLiteral("电脑"), QStringLiteral("未知"),
			 QStringLiteral("没有"), QStringLiteral("一个"), QStringLiteral("可以"), QStringLiteral("这个"),
			 QStringLiteral("com"), QStringLiteral("www"), QStringLiteral("http"), QStringLiteral("https") };
}


QVector<ClassroomWordCloud::Word> ClassroomWordCloud::tokenize( const QStringList& lines,
																const QStringList& extraKeywords,
																int maxWords,
																const QStringList& extraStopWords )
{
	const QString text = lines.join( QLatin1Char(' ') ).toLower();
	QSet<QString> stopWords;
	const auto stopWordList = defaultStopWords();
	for( const auto& word : stopWordList ) { stopWords.insert( word ); }
	for( const auto& word : extraStopWords )
	{
		const auto normalized = word.trimmed().toLower();
		if( normalized.isEmpty() == false ) { stopWords.insert( normalized ); }
	}

	QMap<QString, int> counts;

	QStringList keywords = extraKeywords;
	std::sort( keywords.begin(), keywords.end(), []( const QString& a, const QString& b ) {
		return a.size() > b.size();
	} );
	for( const auto& keyword : keywords )
	{
		const auto normalized = keyword.trimmed().toLower();
		if( normalized.size() < 2 ) { continue; }
		const auto occurrences = text.count( normalized );
		if( occurrences > 0 ) { counts[normalized] += occurrences * 2; }
	}

	const QRegularExpression chunkPattern( QStringLiteral("[\\x{4e00}-\\x{9fff}]+|[a-z][a-z0-9_.+-]{1,}") );
	auto chunkIt = chunkPattern.globalMatch( text );
	while( chunkIt.hasNext() )
	{
		const auto chunk = chunkIt.next().captured( 0 );
		if( chunk.at( 0 ).unicode() < 0x4e00 || chunk.at( 0 ).unicode() > 0x9fff )
		{
			if( stopWords.contains( chunk ) == false ) { counts[chunk] += 1; }
			continue;
		}

		for( int size = 2; size <= 4 && size <= chunk.size(); ++size )
		{
			for( int start = 0; start + size <= chunk.size(); ++start )
			{
				const auto gram = chunk.mid( start, size );
				if( stopWords.contains( gram ) ) { continue; }
				// Longer n-grams carry more meaning; weight them so short high-frequency
				// fragments such as "软件" do not bury "画图软件".
				counts[gram] += size >= 4 ? 2 : 1;
			}
		}
	}

	QVector<Word> words;
	for( auto it = counts.constBegin(); it != counts.constEnd(); ++it )
	{
		if( stopWords.contains( it.key() ) ) { continue; }
		// Keep single occurrences only when they are specific enough (4+ characters
		// or an explicit teacher keyword), otherwise they are almost always noise.
		if( it.value() < 2 && it.key().size() < 4 ) { continue; }
		words.append( { it.key(), it.value(), 0.0 } );
	}

	std::sort( words.begin(), words.end(), []( const Word& a, const Word& b ) {
		const auto score = []( const Word& word ) {
			return static_cast<double>( word.count ) * ( 1.0 + 0.35 * ( word.text.size() - 2 ) );
		};
		const auto scoreA = score( a );
		const auto scoreB = score( b );
		if( scoreA != scoreB ) { return scoreA > scoreB; }
		return a.text.size() > b.text.size();
	} );

	QVector<Word> selected;
	for( const auto& word : words )
	{
		bool shadowed = false;
		for( const auto& kept : selected )
		{
			if( kept.text.contains( word.text ) &&
				static_cast<double>( kept.count ) >= static_cast<double>( word.count ) * 0.75 )
			{
				shadowed = true;
				break;
			}
		}
		if( shadowed ) { continue; }
		selected.append( word );
		if( selected.size() >= maxWords ) { break; }
	}

	if( selected.isEmpty() == false )
	{
		const double maximum = static_cast<double>( selected.first().count );
		for( auto& word : selected )
		{
			word.weight = maximum > 0.0 ? std::sqrt( static_cast<double>( word.count ) / maximum ) : 0.0;
		}
	}
	return selected;
}


QVector<ClassroomWordCloud::PlacedWord> ClassroomWordCloud::layout( const QVector<Word>& words,
																	 int width, int height )
{
	static const QStringList palette{
		QStringLiteral("#1f6f8b"), QStringLiteral("#c0392b"), QStringLiteral("#27ae60"),
		QStringLiteral("#8e44ad"), QStringLiteral("#d35400"), QStringLiteral("#2c3e50"),
		QStringLiteral("#16a085"), QStringLiteral("#b03a2e")
	};

	QVector<PlacedWord> placed;
	const double centerX = width / 2.0;
	const double centerY = height / 2.0;
	int colorIndex = 0;

	for( const auto& word : words )
	{
		const int fontSize = 14 + static_cast<int>( word.weight * 44.0 );
		int characterWidth = 0;
		for( const auto character : word.text )
		{
			characterWidth += character.unicode() > 0x2e80 ? fontSize : static_cast<int>( fontSize * 0.58 );
		}
		const int boxWidth = characterWidth + 10;
		const int boxHeight = static_cast<int>( fontSize * 1.25 ) + 6;

		PlacedWord candidate;
		for( int step = 0; step < 2400; ++step )
		{
			const double angle = step * 0.22;
			const double radius = 2.6 * angle;
			const int left = static_cast<int>( centerX + radius * std::cos( angle ) * 1.35 - boxWidth / 2.0 );
			const int top = static_cast<int>( centerY + radius * std::sin( angle ) * 0.72 - boxHeight / 2.0 );
			if( left < 4 || top < 4 || left + boxWidth > width - 4 || top + boxHeight > height - 4 ) { continue; }

			bool collides = false;
			for( const auto& other : placed )
			{
				if( left < other.x + other.width && left + boxWidth > other.x &&
					top < other.y + other.height && top + boxHeight > other.y )
				{
					collides = true;
					break;
				}
			}
			if( collides == false )
			{
				candidate = { word.text, left, top, boxWidth, boxHeight, fontSize,
							  palette.at( colorIndex % palette.size() ) };
				break;
			}
		}
		if( candidate.width == 0 ) { continue; }
		++colorIndex;
		placed.append( candidate );
	}
	return placed;
}


QByteArray ClassroomWordCloud::toSvg( const QVector<Word>& words, int width, int height )
{
	QString svg;
	svg += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	svg += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" "
						  "viewBox=\"0 0 %1 %2\">\n").arg( width ).arg( height );
	svg += QStringLiteral("<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n");

	if( words.isEmpty() )
	{
		const auto hint = QCoreApplication::translate( "ClassroomWordCloud", "No interest data yet" );
		svg += QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"20\" "
							  "font-family=\"Microsoft YaHei,sans-serif\" fill=\"#888888\">%3</text>\n")
					.arg( width / 2 ).arg( height / 2 ).arg( hint.toHtmlEscaped() );
		svg += QStringLiteral("</svg>\n");
		return svg.toUtf8();
	}

	for( const auto& word : layout( words, width, height ) )
	{
		const int baseline = word.y + static_cast<int>( word.fontSize * 1.02 ) + 2;
		svg += QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" font-size=\"%3\" "
							  "font-weight=\"bold\" font-family=\"Microsoft YaHei,sans-serif\" "
							  "fill=\"%4\">%5</text>\n")
					.arg( word.x + word.width / 2 )
					.arg( baseline )
					.arg( word.fontSize )
					.arg( word.color )
					.arg( word.text.toHtmlEscaped() );
	}

	svg += QStringLiteral("</svg>\n");
	return svg.toUtf8();
}


QPixmap ClassroomWordCloud::toPixmap( const QVector<Word>& words, int width, int height )
{
	QPixmap pixmap( width, height );
	pixmap.fill( QColor( QStringLiteral("#ffffff") ) );
	QPainter painter( &pixmap );

	if( words.isEmpty() )
	{
		const auto hint = QCoreApplication::translate( "ClassroomWordCloud", "No interest data yet" );
		QFont font( QStringLiteral("Microsoft YaHei") );
		font.setPixelSize( 20 );
		painter.setFont( font );
		painter.setPen( QColor( QStringLiteral("#888888") ) );
		painter.drawText( pixmap.rect(), Qt::AlignCenter, hint );
		return pixmap;
	}

	for( const auto& word : layout( words, width, height ) )
	{
		QFont font( QStringLiteral("Microsoft YaHei") );
		font.setPixelSize( word.fontSize );
		font.setBold( true );
		painter.setFont( font );
		painter.setPen( QColor( word.color ) );
		painter.drawText( QRect( word.x, word.y, word.width, word.height ), Qt::AlignCenter, word.text );
	}
	return pixmap;
}
