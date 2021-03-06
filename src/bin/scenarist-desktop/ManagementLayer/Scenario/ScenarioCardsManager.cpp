#include "ScenarioCardsManager.h"

#include <Domain/Scenario.h>

#include <BusinessLayer/ScenarioDocument/ScenarioModel.h>
#include <BusinessLayer/ScenarioDocument/ScenarioModelItem.h>

#include <DataLayer/DataStorageLayer/StorageFacade.h>
#include <DataLayer/DataStorageLayer/ScenarioStorage.h>
#include <DataLayer/DataStorageLayer/SettingsStorage.h>

#include <UserInterfaceLayer/Scenario/ScenarioCards/PrintCardsDialog.h>
#include <UserInterfaceLayer/Scenario/ScenarioCards/ScenarioCardsView.h>
#include <UserInterfaceLayer/Scenario/ScenarioItemDialog/ScenarioItemDialog.h>

#include <3rd_party/Helpers/ColorHelper.h>
#include <3rd_party/Helpers/TextEditHelper.h>
#include <3rd_party/Helpers/TextUtils.h>

#include <QApplication>
#include <QPainter>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QScopedPointer>

using ManagementLayer::ScenarioCardsManager;
using UserInterface::PrintCardsDialog;
using UserInterface::ScenarioCardsView;
using UserInterface::ScenarioItemDialog;

namespace {
    const bool IS_SCRIPT = false;
}


ScenarioCardsManager::ScenarioCardsManager(QObject* _parent, QWidget* _parentWidget) :
    QObject(_parent),
    m_view(new ScenarioCardsView(IS_SCRIPT, _parentWidget)),
    m_addItemDialog(new ScenarioItemDialog(_parentWidget)),
    m_printDialog(new PrintCardsDialog(_parentWidget))
{
    initConnections();
    reloadSettings();
}

QWidget* ScenarioCardsManager::view() const
{
    return m_view;
}

void ScenarioCardsManager::reloadSettings()
{
    m_view->setUseCorkboardBackground(
                DataStorageLayer::StorageFacade::settingsStorage()->value(
                    "cards/use-corkboard",
                    DataStorageLayer::SettingsStorage::ApplicationSettings)
                .toInt()
                );

    const bool useDarkTheme =
            DataStorageLayer::StorageFacade::settingsStorage()->value(
                "application/use-dark-theme",
                DataStorageLayer::SettingsStorage::ApplicationSettings)
            .toInt();
    const QString colorSuffix = useDarkTheme ? "-dark" : "";
    m_view->setBackgroundColor(
                QColor(
                    DataStorageLayer::StorageFacade::settingsStorage()->value(
                        "cards/background-color" + colorSuffix,
                        DataStorageLayer::SettingsStorage::ApplicationSettings)
                    )
                );
}

QString ScenarioCardsManager::save() const
{
    return m_view->save();
}

void ScenarioCardsManager::saveChanges(bool _hasChangesInText)
{
    m_view->saveChanges(_hasChangesInText);
}

void ScenarioCardsManager::load(BusinessLogic::ScenarioModel* _model, const QString& _xml)
{
    //
    // ???????????????? ????????????
    //
    if (m_model != _model) {
        m_model = _model;
        connect(m_model, &BusinessLogic::ScenarioModel::rowsInserted, this, [this] (const QModelIndex& _parent, int _first, int _last) {
            //
            // ?????????????????? ???????????? ?????????????????????? ??????????????
            //
            for (int row = _first; row <= _last; ++row) {
                const QModelIndex index = m_model->index(row, 0, _parent);
                BusinessLogic::ScenarioModelItem* item = m_model->itemForIndex(index);

                //
                // ... ???????????????????? ?????????? ???????? ???? ?????????????? ?????????????????????? ???????????? ??????????????
                //
                if (item->hasParent()
                    && item->parent()->hasParent()
                    && item->parent()->parent()->hasParent()) {
                    continue;
                }

                //
                // ... ?????????????????? ???????????????????? ??????????????
                //
                QModelIndex currentCardIndex = _parent;
                if (row > 0) {
                    //
                    // -1 ??.??. ?????????? ???????????????????? ??????????????
                    //
                    const int itemRow = row - 1;
                    if (_parent.isValid()) {
                        currentCardIndex = _parent.child(itemRow, 0);
                    } else {
                        currentCardIndex = m_model->index(itemRow, 0);
                    }
                }
                BusinessLogic::ScenarioModelItem* currentCard = m_model->itemForIndex(currentCardIndex);

                //
                // ... ??????????????????
                //
                const bool isEmbedded =
                        item->hasParent()
                        && item->parent()->type() != BusinessLogic::ScenarioModelItem::Scenario;
                m_view->insertCard(
                    item->uuid(),
                    item->type() == BusinessLogic::ScenarioModelItem::Folder,
                    item->sceneNumber(),
                    item->name().isEmpty() ? TextEditHelper::smartToUpper(item->header()) : TextEditHelper::smartToUpper(item->name()),
                    item->description().isEmpty() ? item->fullText() : item->description(),
                    item->stamp(),
                    item->colors(),
                    isEmbedded,
                    currentCard->uuid());
            }
        });
        connect(m_model, &BusinessLogic::ScenarioModel::rowsAboutToBeRemoved, this, [this] (const QModelIndex& _parent, int _first, int _last) {
            for (int row = _last; row >= _first; --row) {
                QModelIndex currentCardIndex = _parent;
                if (_parent.isValid()) {
                    currentCardIndex = _parent.child(row, 0);
                } else {
                    currentCardIndex = m_model->index(row, 0);
                }
                BusinessLogic::ScenarioModelItem* currentCard = m_model->itemForIndex(currentCardIndex);
                m_view->removeCard(currentCard->uuid());
            }
        });
        connect(m_model, &BusinessLogic::ScenarioModel::dataChanged, this, [this] (const QModelIndex& _topLeft, const QModelIndex& _bottomRight) {
            for (int row = _topLeft.row(); row <= _bottomRight.row(); ++row) {
                const QModelIndex index = m_model->index(row, 0, _topLeft.parent());
                const BusinessLogic::ScenarioModelItem* item = m_model->itemForIndex(index);
                const bool isAct =
                        item->type() == BusinessLogic::ScenarioModelItem::Folder
                        && item->hasParent()
                        && item->parent()->type() == BusinessLogic::ScenarioModelItem::Scenario;
                const bool isEmbedded =
                        item->hasParent()
                        && item->parent()->type() != BusinessLogic::ScenarioModelItem::Scenario;
                m_view->updateCard(
                    item->uuid(),
                    item->type() == BusinessLogic::ScenarioModelItem::Folder,
                    item->sceneNumber(),
                    item->name().isEmpty() ? TextEditHelper::smartToUpper(item->header()) : TextEditHelper::smartToUpper(item->name()),
                    item->description().isEmpty() ? item->fullText() : item->description(),
                    item->stamp(),
                    item->colors(),
                    isEmbedded,
                    isAct);
            }
        });
    }

    //
    // ???????????????? ????????????????
    //
    // ... ???????? ?????????? ????????, ???? ???????????? ?????????????????? ????
    //
    if (!_xml.isEmpty()) {
        m_view->load(_xml);
    }
    //
    // ... ?? ???????? ?????????? ??????????, ???????????????????? ???? ???? ???????????? ????????????
    //
    else {
        m_view->load(m_model->simpleScheme());
    }
}

void ScenarioCardsManager::clear()
{
    if (m_model != nullptr) {
        m_model->disconnect(this);
        m_model = nullptr;
    }
    m_view->clear();
}

void ScenarioCardsManager::undo()
{
    m_view->undo();
}

void ScenarioCardsManager::redo()
{
    m_view->redo();
}

void ScenarioCardsManager::setCommentOnly(bool _isCommentOnly)
{
    m_view->setCommentOnly(_isCommentOnly);
}

void ScenarioCardsManager::goToCard(const QString& _uuid)
{
    const QModelIndex indexForUpdate = m_model->indexForUuid(_uuid);
    emit goToCardRequest(indexForUpdate);
}

void ScenarioCardsManager::addCard()
{
    const bool useFolders = true;
    m_addItemDialog->prepareForAdding(!useFolders);

    //
    // ???????? ???????????????????????? ?????????????????????????? ?????????? ???????????????? ??????????????
    //
    if (m_addItemDialog->exec() == QLightBoxDialog::Accepted) {
        const int type = m_addItemDialog->itemType();
        const QString name = m_addItemDialog->itemName();
        const QString header = m_addItemDialog->itemHeader();
        const QString description = m_addItemDialog->itemDescription();
        const QString color = m_addItemDialog->itemColor();

        //
        // ?????????????????? ????????????????, ?????????? ?????????????? ?????????? ???????????????? ??????????????
        //
        QModelIndex index;
        const QString lastItemUuid = m_view->beforeNewItemUuid();
        if (!lastItemUuid.isEmpty()) {
            index = m_model->indexForUuid(lastItemUuid);
        }

        //
        // ???????? ?????????????????????? ??????, ???? ?????????? ?????????? ???????????????? ????????????
        //
        if (type == BusinessLogic::ScenarioModelItem::Folder
            && index.parent().isValid()) {
            index = index.parent();
        }

        emit addCardRequest(index, type, name, header, description, QColor(color));
    }
}

void ScenarioCardsManager::editCard(const QString& _uuid)
{
    //
    // ?????? ???????????????? ???????????????????????? ???????????????? ?? ?????????????????? ???????????? ???????????? ???????? ?? ??????????,
    // ?????????? ???????????????????????? ???????????????? ???????????? ?? ???????????????????? ?? ?????????? ???????????????? ???????????????? ????????????????
    // ?????????????????????????? ?????????? ???????????????? ???? ?? ?? ???????????? ????????
    //

    m_addItemDialog->prepareForEditing();

    const QModelIndex indexForUpdate = m_model->indexForUuid(_uuid);
    const auto* itemForUpdate = m_model->itemForIndex(indexForUpdate);
    m_addItemDialog->setItemType(itemForUpdate->type());
    m_addItemDialog->setItemName(itemForUpdate->name());
    const bool itemForUpdateHasEqualNameAndHeader = itemForUpdate->header() == itemForUpdate->name();
    if (!itemForUpdateHasEqualNameAndHeader) {
        m_addItemDialog->setItemHeader(itemForUpdate->header());
    }
    m_addItemDialog->setItemDescription(itemForUpdate->description());
    const QString firstColor = itemForUpdate->colors().split(";").first();
    m_addItemDialog->setItemColor(firstColor);

    //
    // ???????? ???????????????????????? ?????????????????????????? ?????????? ???????????????? ??????????????
    //
    if (m_addItemDialog->exec() == QLightBoxDialog::Accepted) {
        const int type = m_addItemDialog->itemType();
        const QString name = m_addItemDialog->itemName();
        QString header = m_addItemDialog->itemHeader();
        if (header.isEmpty()) {
            header = name;
        }
        const QString description = m_addItemDialog->itemDescription();
        QString colors = itemForUpdate->colors();
        if (firstColor != m_addItemDialog->itemColor()) {
            colors.replace(firstColor, m_addItemDialog->itemColor());
        }

        //
        // ?????????????????? ???????????? ???? ??????????????????
        //
        emit updateCardRequest(indexForUpdate, type, name, header, description, colors);
    }
}

void ScenarioCardsManager::removeCard(const QString& _uuid)
{
    emit removeCardRequest(m_model->indexForUuid(_uuid));
}

void ScenarioCardsManager::moveCard(const QString& _cardId, const QString& _actId, const QString& _previousCardId)
{
    if (!_cardId.isEmpty()) {
        const QModelIndex parentIndex = m_model->indexForUuid(_actId);
        const QModelIndex previousIndex = m_model->indexForUuid(_previousCardId);
        const QModelIndex movedIndex = m_model->indexForUuid(_cardId);

        //
        // ???????????????????????????? ?????????????????????? ?? ??????????????
        //
        int previousRow = 0;
        if (previousIndex.isValid()) {
            previousRow = previousIndex.row() + 1;
        }
        QMimeData* mime = m_model->mimeData({ movedIndex });
        m_model->dropMimeData(mime, Qt::MoveAction, previousRow, 0, parentIndex);
    }
}

void ScenarioCardsManager::moveCardToGroup(const QString& _cardId, const QString& _groupId)
{
    if (!_cardId.isEmpty()) {
        const QModelIndex parentIndex = m_model->indexForUuid(_groupId);
        const QModelIndex movedIndex = m_model->indexForUuid(_cardId);

        //
        // ???????????????????????????? ?????????????????????? ?? ??????????????
        //
        int previousRow = -1;
        QMimeData* mime = m_model->mimeData({ movedIndex });
        m_model->dropMimeData(mime, Qt::MoveAction, previousRow, 0, parentIndex);
    }
}

void ScenarioCardsManager::changeCardColors(const QString& _uuid, const QString& _colors)
{
    if (!_uuid.isEmpty()) {
        const QModelIndex index = m_model->indexForUuid(_uuid);
        emit cardColorsChanged({ index }, _colors);
    }
}

void ScenarioCardsManager::changeCardStamp(const QString& _uuid, const QString& _stamp)
{
    if (!_uuid.isEmpty()) {
        const QModelIndex index = m_model->indexForUuid(_uuid);
        emit cardStampChanged(index, _stamp);
    }
}

void ScenarioCardsManager::changeCardType(const QString& _uuid, bool _isFolder)
{
    if (!_uuid.isEmpty()) {
        const QModelIndex index = m_model->indexForUuid(_uuid);
        int mappedType = BusinessLogic::ScenarioModelItem::Scene;
        if (_isFolder) {
            mappedType = BusinessLogic::ScenarioModelItem::Folder;
        }
        emit cardTypeChanged(index, mappedType);
    }
}

void ScenarioCardsManager::print()
{
    //
    // ???????????????? ??????????????
    //
    QPrinter* printer = new QPrinter;
    printer->setPageOrientation(m_printDialog->isPortrait() ? QPageLayout::Portrait : QPageLayout::Landscape);

    //
    // ???????????????? ???????????? ??????????????????????????
    //
    QPrintPreviewDialog printDialog(printer, m_view);
    printDialog.setWindowState(Qt::WindowMaximized);
    connect(&printDialog, &QPrintPreviewDialog::paintRequested, this, &ScenarioCardsManager::printCards);

    //
    // ?????????????????? ????????????????????????
    //
    printDialog.exec();

    //
    // ?????????????? ????????????
    //
    delete printer;
}

void ScenarioCardsManager::printCards(QPrinter* _printer)
{
    //
    // ?????????????? ????????????????
    //
    m_printDialog->setEnabled(false);
    m_printDialog->setProgressValue(0);
    m_printDialog->showProgress(0, 0);

    //
    // ???????????????????? ???????????? ???????????????? ?????? ????????????
    //
    QMap<int, BusinessLogic::ScenarioModelItem*> items;
    {
        QVector<QModelIndex> parents { QModelIndex() };
        do {
            const int parentsSize = parents.size();
            for (int parentIndexRow = parentsSize - 1; parentIndexRow >= 0; --parentIndexRow) {
                //
                // ?????????????????? ???????????????? ??????????????????
                //
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

                //
                // ???????????????? ????????????????
                //
                const QModelIndex& parentIndex = parents.at(parentIndexRow);
                for (int row = 0; row < m_model->rowCount(parentIndex); ++row) {
                    const QModelIndex index = m_model->index(row, 0, parentIndex);
                    parents.append(index);

                    auto item = m_model->itemForIndex(index);
                    items.insert(item->position(), item);
                }
                parents.remove(parentIndexRow);
            }
        } while (!parents.isEmpty());
    }

    //
    // ?????????????? ????????????????
    //
    int progress = 0;
    m_printDialog->setProgressValue(progress);
    m_printDialog->showProgress(0, items.size());

    //
    // ???????????????? ????????????????
    //
    QPainter painter(_printer);

    //
    // ???????????????? ?????? ???????????????? ????-??????????????
    //
    bool isFirst = true;
    const int firstCardIndex = 0;
    const int cardsCount = m_printDialog->cardsCount();
    const qreal sideMargin = _printer->pageRect().x();
    int currentCardIndex = firstCardIndex;
    qreal lastY = 0;
    for (const int& key : items.keys()) {
        //
        // ?????????????????? ???????????????? ??????????????????
        //
        m_printDialog->setProgressValue(++progress);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        const BusinessLogic::ScenarioModelItem* item = items[key];
        const QRectF pageRect = _printer->paperRect().adjusted(0, 0, -2 * sideMargin, -2 * sideMargin);

        //
        // ???????? ????????, ?????????????????? ???? ?????????? ???????????????? ?? ???????????? ?????????? ??????????????
        //
        if (currentCardIndex == firstCardIndex) {
            if (isFirst) {
                isFirst = false;
            } else {
                _printer->newPage();
            }

            painter.setClipRect(pageRect);
            painter.save();
            painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
            switch (cardsCount) {
                default:
                case 1: {
                    //
                    // ?????? ?????????? ??????????????
                    //
                    break;
                }

                case 2: {
                    //
                    // ???????????????????????????? ??????????
                    //
                    const qreal height = pageRect.height() / 2.;
                    QPointF p1 = pageRect.topLeft() + QPointF(0, height);
                    QPointF p2 = pageRect.topRight() + QPointF(0, height);
                    painter.drawLine(p1, p2);
                    break;
                }

                case 4:
                case 6:
                case 8: {
                    //
                    // ???????????????????????????? ??????????
                    //
                    {
                        const qreal height = pageRect.height() / (cardsCount / 2.);
                        qreal summaryHeight = 0;
                        while (summaryHeight + height < pageRect.height()) {
                            summaryHeight += height;
                            const QPointF p1 = pageRect.topLeft() + QPointF(0, summaryHeight);
                            const QPointF p2 = pageRect.topRight() + QPointF(0, summaryHeight);
                            painter.drawLine(p1, p2);
                        }
                    }
                    //
                    // ???????????????????????? ??????????
                    //
                    {
                        const qreal width = pageRect.width() / 2.;
                        const QPointF p1 = pageRect.topLeft() + QPointF(width, 0);
                        const QPointF p2 = pageRect.bottomLeft() + QPointF(width, 0);
                        painter.drawLine(p1, p2);
                    }
                    break;
                }
            }
            painter.restore();
        }

        //
        // ???????????????????? ?????????????? ???? ????????????????
        //
        QRectF cardRect = pageRect;
        cardRect.moveTop(cardRect.top() + lastY);
        switch (cardsCount) {
            default:
            case 1: {
                //
                // ?????? ????????????????
                //
                break;
            }

            case 2: {
                const qreal height = cardRect.height() / 2.;
                cardRect.setHeight(height);
                lastY += height;
                break;
            }

            case 4:
            case 6:
            case 8: {
                const qreal width = cardRect.width() / 2.;
                cardRect.setWidth(width);
                const qreal height = cardRect.height() / (cardsCount / 2.);
                cardRect.setHeight(height);
                //
                // ???????? ?????????????? ?? ???????? ????????????????
                //
                if (currentCardIndex % 2 != 0) {
                    //
                    // ... ?????????????? ?????????????? ??????????????????
                    //
                    cardRect.moveLeft(cardRect.left() + width);
                    //
                    // ... ?? ?????????????????? ?? ???????????????????? ????????
                    //
                    lastY += height;
                }

                //
                // ???????????????????????????? ??????????, ?????????? ?????????????? ????????????????
                //
                if (currentCardIndex % 2 == 0) {
                    cardRect.setLeft(cardRect.left() + 1);
                } else {
                    cardRect.setRight(cardRect.right() - 1);
                }
                if (currentCardIndex % (cardsCount / 2) <= 1) {
                    cardRect.setTop(cardRect.top() + 1);
                } else if (currentCardIndex % (cardsCount / 2) >= (cardsCount / 2 - 1)) {
                    cardRect.setBottom(cardRect.bottom() - 1);
                }
                break;
            }
        }

        //
        // ???????????????????????????? ?????????????? ???? ????????????????
        //
        // ... ??????????
        //
        if (cardRect.bottom() != pageRect.bottom()) {
            cardRect.setBottom(cardRect.bottom() - sideMargin);
        }
        //
        // ... ????????????
        //
        if (cardRect.top() != pageRect.top()) {
            cardRect.setTop(cardRect.top() + sideMargin);
        }
        //
        // ... ??????????
        //
        if (cardRect.left() != pageRect.left()) {
            cardRect.setLeft(cardRect.left() + sideMargin);
        }
        //
        // ... ?? ????????????
        //
        if (cardRect.right() != pageRect.right()) {
            cardRect.setRight(cardRect.right() - sideMargin);
        }

        //
        // ???????????????????????????? ?????????????? ?????? ??????????????????????????????
        //
        const int contentMargin = 6;
        cardRect.setBottom(cardRect.bottom() - contentMargin);
        cardRect.setTop(cardRect.top() + contentMargin);
        cardRect.setLeft(cardRect.left() + contentMargin);
        cardRect.setRight(cardRect.right() - contentMargin);

        //
        // ???????????? ????????????????
        //
        {
            if (m_printDialog->printColorCards()) {
                const auto cardColor = item->colors().split(";").first();
                if (!cardColor.isEmpty()) {
                    const qreal delta = contentMargin + sideMargin;
                    QRectF fullCardRect = cardRect.adjusted(-delta, -delta, delta, delta);
                    painter.fillRect(fullCardRect, cardColor);
                    painter.setPen(ColorHelper::textColor(cardColor));
                } else {
                    painter.setPen(Qt::black);
                }
            }

            //
            // ???????????? ??????????????????
            //
            QTextOption textoption;
            textoption.setAlignment(Qt::AlignTop | Qt::AlignLeft);
            textoption.setWrapMode(QTextOption::NoWrap);
            QFont font = painter.font();
            font.setBold(true);
            painter.setFont(font);
            const int titleHeight = painter.fontMetrics().height();
            const QRectF titleRect(cardRect.left(), cardRect.top(), cardRect.width(), titleHeight);
            QString titleText = item->name().isEmpty() ? item->header() : item->name();
            if (item->type() == BusinessLogic::ScenarioModelItem::Scene) {
                titleText.prepend(QString("%1. ").arg(item->sceneNumber()));
            }
            titleText = TextUtils::elidedText(TextEditHelper::smartToUpper(titleText), painter.font(), titleRect.size(), textoption);
            painter.drawText(titleRect, titleText, textoption);

            //
            // ???????????? ????????????????
            //
            textoption.setAlignment(Qt::AlignTop | Qt::AlignLeft);
            textoption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            font.setBold(false);
            painter.setFont(font);
            const qreal spacing = titleRect.height() / 2;
            const QRectF descriptionRect(titleRect.left(), titleRect.bottom() + spacing, titleRect.width(), cardRect.height() - titleRect.height() - spacing);
            QString descriptionText = item->description().isEmpty() ? item->fullText() : item->description();
            descriptionText.replace("\n", "\n\n");
            descriptionText = TextUtils::elidedText(descriptionText, painter.font(), descriptionRect.size(), textoption);
            painter.drawText(descriptionRect, descriptionText, textoption);
        }

        //
        // ?????????????????? ?? ?????????????????? ????????????????
        //
        ++currentCardIndex;
        if (currentCardIndex == cardsCount) {
            currentCardIndex = 0;
            lastY = 0;
        }
    }

    //
    // ???????????????? ????????????????
    //
    m_printDialog->hideProgress();
    m_printDialog->setEnabled(true);
}

void ScenarioCardsManager::initConnections()
{
    //
    // ???????? ???? ?????????????? ?????????????????? ?????????????????????? ??????????, ???????????????? ???? ????????????
    //
    connect(m_view, &ScenarioCardsView::schemeNotLoaded, [=] {
        m_view->load(m_model->simpleScheme());
    });

    connect(m_view, &ScenarioCardsView::undoRequest, this, &ScenarioCardsManager::undoRequest);
    connect(m_view, &ScenarioCardsView::redoRequest, this, &ScenarioCardsManager::redoRequest);

    connect(m_view, &ScenarioCardsView::goToCardRequest, this, &ScenarioCardsManager::goToCard);
    connect(m_view, &ScenarioCardsView::addCardClicked, this, &ScenarioCardsManager::addCard);
    connect(m_view, &ScenarioCardsView::editCardRequest, this, &ScenarioCardsManager::editCard);
    connect(m_view, &ScenarioCardsView::removeCardRequest, this, &ScenarioCardsManager::removeCard);
    connect(m_view, &ScenarioCardsView::cardMoved, this, &ScenarioCardsManager::moveCard);
    connect(m_view, &ScenarioCardsView::cardMovedToGroup, this, &ScenarioCardsManager::moveCardToGroup);
    connect(m_view, &ScenarioCardsView::cardColorsChanged, this, &ScenarioCardsManager::changeCardColors);
    connect(m_view, &ScenarioCardsView::cardStampChanged, this, &ScenarioCardsManager::changeCardStamp);
    connect(m_view, &ScenarioCardsView::cardTypeChanged, this, &ScenarioCardsManager::changeCardType);

    connect(m_view, &ScenarioCardsView::printRequest, m_printDialog, &PrintCardsDialog::exec);
    connect(m_printDialog, &PrintCardsDialog::printPreview, this, &ScenarioCardsManager::print);

    connect(m_view, &ScenarioCardsView::fullscreenRequest, this, &ScenarioCardsManager::fullscreenRequest);

    connect(m_view, &ScenarioCardsView::cardsChanged, this, &ScenarioCardsManager::cardsChanged);
}
