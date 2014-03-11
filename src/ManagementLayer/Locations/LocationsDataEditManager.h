#ifndef LOCATIONSDATAEDITMANAGER_H
#define LOCATIONSDATAEDITMANAGER_H

#include <QObject>

namespace UserInterface {
	class LocationsDataEdit;
}


namespace ManagementLayer
{
	/**
	 * @brief Управляющий данными локации
	 */
	class LocationsDataEditManager : public QObject
	{
		Q_OBJECT

	public:
		explicit LocationsDataEditManager(QObject* _parent, QWidget* _parentWidget);

		QWidget* view() const;

		/**
		 * @brief Подготовить редактор к работе
		 */
		void clean();

		/**
		 * @brief Редактировать локацию
		 */
		void editLocation(const QString& _name);

	signals:
		/**
		 * @brief Было изменено название локации
		 */
		void locationNameChanged(const QString& _oldName, const QString& _newName);

	private slots:
		/**
		 * @brief Сохранить изменения
		 */
		void aboutSave();

		/**
		 * @brief Отменить изменения
		 */
		void aboutDontSave();

	private:
		/**
		 * @brief Настроить представление
		 */
		void initView();

		/**
		 * @brief Настроить соединения
		 */
		void initConnections();

	private:
		/**
		 * @brief Редактор данных
		 */
		UserInterface::LocationsDataEdit* m_editor;

		/**
		 * @brief Название последней редактируемой локации
		 */
		QString m_locationName;
	};
}

#endif // LOCATIONSDATAEDITMANAGER_H
