#include "ResearchItemDialog.h"
#include "ui_ResearchItemDialog.h"

#include <Domain/Research.h>

using UserInterface::ResearchItemDialog;


ResearchItemDialog::ResearchItemDialog(QWidget* _parent) :
	QLightBoxDialog(_parent),
	m_ui(new Ui::ResearchItemDialog)
{
	m_ui->setupUi(this);

	initView();
	initConnections();
}

ResearchItemDialog::~ResearchItemDialog()
{
	delete m_ui;
}

void ResearchItemDialog::clear()
{
	m_ui->name->clear();
}

void ResearchItemDialog::setInsertParent(const QString& _parentName)
{
	if (!_parentName.isEmpty()) {
		m_ui->isInsert->show();
		m_ui->isInsert->setText(tr("Insert into \"%1\"").arg(_parentName));
	} else {
		m_ui->isInsert->hide();
		m_ui->isInsert->setText(QString::null);
	}
}

int ResearchItemDialog::researchType() const
{
	Domain::Research::Type type;
	if (m_ui->folder->isChecked()) {
		type = Domain::Research::Folder;
	} else {
		type = Domain::Research::Text;
	}
	return type;
}

QString ResearchItemDialog::researchName() const
{
	return m_ui->name->text();
}

bool ResearchItemDialog::insertResearchInParent() const
{
	return !m_ui->isInsert->text().isEmpty() && m_ui->isInsert->isChecked();
}

QWidget* ResearchItemDialog::focusedOnExec() const
{
	return m_ui->name;
}

void ResearchItemDialog::initView()
{


	QLightBoxDialog::initView();
}

void ResearchItemDialog::initConnections()
{
	connect(m_ui->buttons, &QDialogButtonBox::accepted, [=](){
		if (!m_ui->name->text().isEmpty()) {
			accept();
		}
	});
	connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QLightBoxDialog::reject);

	QLightBoxDialog::initConnections();
}