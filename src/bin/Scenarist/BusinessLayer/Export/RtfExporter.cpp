#include "RtfExporter.h"

#include <DataLayer/DataStorageLayer/StorageFacade.h>
#include <DataLayer/DataStorageLayer/SettingsStorage.h>

#include <BusinessLayer/ScenarioDocument/ScenarioDocument.h>
#include <BusinessLayer/ScenarioDocument/ScenarioTextDocument.h>
#include <BusinessLayer/ScenarioDocument/ScenarioStyle.h>

#include <Domain/Scenario.h>

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QFile>
#include <QChar>

using namespace BusinessLogic;

namespace {
	/**
	 * @brief Стиль экспорта
	 */
	static ScenarioStyle exportStyle() {
		return ScenarioStyleFacade::style(
					DataStorageLayer::StorageFacade::settingsStorage()->value(
						"export/style",
						DataStorageLayer::SettingsStorage::ApplicationSettings)
					);
	}

	/**
	 * @brief Конец строки
	 */
	const char* END_OF_LINE = "\r\n";

	/**
	 * @brief Пустая RTF-строка
	 */
	const char* EMPTY_LINE = "\\pard\\plain\\par\r\n";

	/**
	 * @brief Перевести миллиметры в твипсы (мера длины в формате RTF)
	 */
	static int mmToTwips(qreal _mm) {
		return 56.692913386 * _mm;
	}

	/**
	 * @brief Пронумерованный список типов блоков
	 */
	static QMap<int, ScenarioBlockStyle::Type> blockTypes() {
		static QMap<int, ScenarioBlockStyle::Type> s_types;
		if (s_types.isEmpty()) {
			s_types.insert(0, ScenarioBlockStyle::Undefined);
			s_types.insert(1, ScenarioBlockStyle::TimeAndPlace);
			s_types.insert(2, ScenarioBlockStyle::SceneCharacters);
			s_types.insert(3, ScenarioBlockStyle::Action);
			s_types.insert(4, ScenarioBlockStyle::Character);
			s_types.insert(5, ScenarioBlockStyle::Parenthetical);
			s_types.insert(6, ScenarioBlockStyle::Dialog);
			s_types.insert(7, ScenarioBlockStyle::Transition);
			s_types.insert(8, ScenarioBlockStyle::Note);
			s_types.insert(9, ScenarioBlockStyle::TitleHeader);
			s_types.insert(10, ScenarioBlockStyle::Title);
			s_types.insert(11, ScenarioBlockStyle::SceneGroupHeader);
			s_types.insert(12, ScenarioBlockStyle::SceneGroupFooter);
		}
		return s_types;
	}

	/**
	 * @brief Сформировать строку RTF-шрифта для заданного номера в таблице шрифтов и шрифта
	 */
	static QString rtfBlockFont(const ScenarioBlockStyle& _style) {
		QString blockFont = QString("\\f%1").arg(::blockTypes().key(_style.type()));
		switch (_style.font().styleHint()) {
			case QFont::Serif: {
				blockFont.append("\\froman");
				break;
			}

			case QFont::SansSerif: {
				blockFont.append("\\fswiss");
				break;
			}

			case QFont::TypeWriter: {
				blockFont.append("\\fmodern");
				break;
			}

			case QFont::Cursive: {
				blockFont.append("\\fscript");
				break;
			}

			case QFont::OldEnglish:
			case QFont::Fantasy: {
				blockFont.append("\\fdecor");
				break;
			}

			case QFont::Monospace: {
				blockFont.append("\\ftech");
				break;
			}

			default: {
				blockFont.append("\\fnil");
				break;
			}
		}
		blockFont.append("\\fcharset0");
		blockFont.append(" ");
		blockFont.append(_style.font().family());
		blockFont.append(";");

		return blockFont;
	}

	/**
	 * @brief Сформировать строку RTF-стиля из стиля блока
	 */
	static QString rtfBlockStyle(const ScenarioBlockStyle& _style, bool _withHeader = false) {
		//
		// Общее
		//
		QString blockStyle = QString("\\s%1\\f%1").arg(::blockTypes().key(_style.type()));

		if (_style.type() != ScenarioBlockStyle::Undefined) {
			//
			// Шрифт
			//
			if (_style.font().capitalization() == QFont::AllUppercase) {
				blockStyle.append("\\caps");
			}
			blockStyle.append(QString("\\fs%1").arg(_style.font().pointSize() * 2));

			//
			// Отступы
			//
			blockStyle.append("\\fi0");
			blockStyle.append(QString("\\li%1").arg(::mmToTwips(_style.leftMargin())));
			blockStyle.append(QString("\\ri%1").arg(::mmToTwips(_style.rightMargin())));

			//
			// Выравнивание
			//
			switch (_style.blockFormat().alignment()) {
				case Qt::AlignRight: {
					blockStyle.append("\\qr");
					break;
				}

				case Qt::AlignHCenter: {
					blockStyle.append("\\qc");
					break;
				}

				default: {
					blockStyle.append("\\ql");
					break;
				}
			}

			//
			// Дополнительная настройка
			//
			if (_style.font().bold()) {
				blockStyle.append("\\b");
			}
			if (_style.font().italic()) {
				blockStyle.append("\\i");
			}
			if (_style.font().underline()) {
				blockStyle.append("\\ul");
			}

			blockStyle.append("\\sbasedon0");

			//
			// Наименование
			//
			if (_withHeader) {
				blockStyle.append(" ");
				blockStyle.append(
							ScenarioBlockStyle::typeName(_style.type()).toUpper().replace("_", " ")
							);
			}
		} else {
			//
			// Для неопределённого стиля формируется простая заглушка
			//
			blockStyle.append("\\sbasedon222\\snext0 Normal");
		}

		return blockStyle;
	}
}

RtfExporter::RtfExporter()
{
}

void RtfExporter::exportTo(ScenarioDocument* _scenario, const ExportParameters& _exportParameters) const
{
	ScenarioStyle scenarioStyle = ::exportStyle();

	//
	// Открываем документ на запись
	//
	QFile rtfFile(_exportParameters.filePath);
	if (rtfFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		rtfFile.write("{");
		rtfFile.write(header().toUtf8().data());
		rtfFile.write(END_OF_LINE);

		//
		// Формирование титульной страницы
		//
		/*{
			const char* CENTER_LINE = "\\pard\\plain \\s0\\f0\\qc ";
			const char* RIGHT_LINE = "\\pard\\plain \\s0\\f0\\qr ";
			const char* END_OF_PAGE = "\\page\r\n";

			//
			// 12 пустых строк
			//
			int emptyLines = 12;
			while ((emptyLines--) > 0) {
				rtfFile.write(EMPTY_LINE);
			}
			//
			// Название
			//
			rtfFile.write(CENTER_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->name()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			//
			// Доп. инфо
			//
			rtfFile.write(EMPTY_LINE);
			rtfFile.write(CENTER_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->additionalInfo()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			//
			// Жанр
			//
			rtfFile.write(EMPTY_LINE);
			rtfFile.write(CENTER_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->genre()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			//
			// Автор
			//
			rtfFile.write(EMPTY_LINE);
			rtfFile.write(CENTER_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->author()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			//
			// 19 пустых строк
			//
			emptyLines = 19;
			while ((emptyLines--) > 0) {
				rtfFile.write(EMPTY_LINE);
			}
			//
			// Контакты
			//
			rtfFile.write(RIGHT_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->contacts()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			//
			// 1 пустых строки
			//
			emptyLines = 1;
			while ((emptyLines--) > 0) {
				rtfFile.write(EMPTY_LINE);
			}
			//
			// Год
			//
			rtfFile.write(CENTER_LINE);
			rtfFile.write(_scenario->scenario() != 0 ? stringToUtfCode(_scenario->scenario()->year()).toUtf8().data() : "");
			rtfFile.write("\\par");
			rtfFile.write(END_OF_LINE);
			rtfFile.write(END_OF_PAGE);
		}*/

		//
		// Данные считываются из исходного документа, определяется тип блока
		// и записываются прямо в файл
		//
		QTextCursor documentCursor(_scenario->document());

		ScenarioBlockStyle::Type currentBlockType = ScenarioBlockStyle::Undefined;
		int currentSceneNumber = 1;
		while (!documentCursor.atEnd()) {
			//
			// Получим тип текущего блока под курсором
			//
			currentBlockType = ScenarioBlockStyle::forBlock(documentCursor.block());

			//
			// Если блок содержит текст, который необходимо вывести на печать
			//
			if (currentBlockType != ScenarioBlockStyle::NoprintableText
				&& currentBlockType != ScenarioBlockStyle::FolderHeader
				&& currentBlockType != ScenarioBlockStyle::FolderFooter) {
				//
				// Если вставляется не первый блок текста и нужно сделать отступ
				//
				if (!documentCursor.atStart()
					&& !documentCursor.block().text().isEmpty()) {
					int emptyLines = scenarioStyle.blockStyle(currentBlockType).topSpace();
					while (emptyLines-- > 0) {
						rtfFile.write(EMPTY_LINE);
					}
				}

				//
				// Определить стиль блока
				//
				QString blockStyle = style(currentBlockType);

				//
				// Вставить текст
				//
				rtfFile.write("\\pard\\plain ");
				rtfFile.write(blockStyle.toUtf8().data());
				rtfFile.write(" ");

				//
				// Для блока "Время и место"
				//
				if (currentBlockType == ScenarioBlockStyle::TimeAndPlace) {
					//
					// Префикс экспорта
					//
					rtfFile.write(stringToUtfCode(_exportParameters.scenesPrefix).toUtf8().data());
					//
					// Номер сцены, если необходимо
					//
					if (_exportParameters.printScenesNubers) {
						QString sceneNumber = QString("%1. ").arg(currentSceneNumber);
						rtfFile.write(stringToUtfCode(sceneNumber).toUtf8().data());
						++currentSceneNumber;
					}
				}

				//
				// Сам текст блока
				//
				rtfFile.write(stringToUtfCode(documentCursor.block().text()).toUtf8().data());
				rtfFile.write("\\par");
				rtfFile.write(END_OF_LINE);
			}

			//
			// Переходим к следующему блоку
			//
			documentCursor.movePosition(QTextCursor::EndOfBlock);
			documentCursor.movePosition(QTextCursor::NextBlock);
		}

		rtfFile.write("}");
		rtfFile.close();
	}
}

QString RtfExporter::header() const
{
	QString header = "\\rtf1\\ansi";

	//
	// Настройки в соответсвии со стилем
	//
	ScenarioStyle style = ::exportStyle();

	//
	// Настройки шрифтов
	//
	header.append("{\\fonttbl");
	foreach (int blockNumber, ::blockTypes().keys()) {
		ScenarioBlockStyle blockStyle = style.blockStyle(::blockTypes().value(blockNumber));
		header.append(::rtfBlockFont(blockStyle));
	}
	header.append("}\n");

	//
	// Настройки полей документа
	//
	header.append(
				QString("\\margl%1\\margr%2\\margt%3\\margb%4")
				.arg(::mmToTwips(style.pageMargins().left()))
				.arg(::mmToTwips(style.pageMargins().right()))
				.arg(::mmToTwips(style.pageMargins().top()))
				.arg(::mmToTwips(style.pageMargins().bottom()))
				);

	//
	// Настройки используемых стилей
	//
	header.append("{\\stylesheet");
	foreach (int blockNumber, ::blockTypes().keys()) {
		ScenarioBlockStyle blockStyle = style.blockStyle(::blockTypes().value(blockNumber));
		header.append(QString("{%1;}").arg(::rtfBlockStyle(blockStyle, true)));
	}
	header.append("}");

	return header;
}

QString RtfExporter::style(ScenarioBlockStyle::Type _type) const
{
	ScenarioStyle scenarioStyle = ::exportStyle();
	ScenarioBlockStyle blockStyle = scenarioStyle.blockStyle(_type);
	return ::rtfBlockStyle(blockStyle);
}

QString RtfExporter::stringToUtfCode(const QString& _text) const
{
	QString result;

	QString utfCharacter;
	foreach (QChar character, _text)	{
		utfCharacter.setNum(character.unicode());
		utfCharacter.prepend("\\u");
		utfCharacter.append("'");
		result += utfCharacter;
	}

	return result;
}
