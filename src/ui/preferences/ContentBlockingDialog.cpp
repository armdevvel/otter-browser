/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 - 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ContentBlockingDialog.h"
#include "ContentBlockingIntervalDelegate.h"
#include "../OptionDelegate.h"
#include "../../core/Console.h"
#include "../../core/ContentBlockingManager.h"
#include "../../core/ContentBlockingProfile.h"
#include "../../core/SessionsManager.h"
#include "../../core/SettingsManager.h"
#include "../../core/Utils.h"

#include "ui_ContentBlockingDialog.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QMessageBox>

namespace Otter
{

ContentBlockingDialog::ContentBlockingDialog(QWidget *parent) : Dialog(parent),
	m_ui(new Ui::ContentBlockingDialog)
{
	m_ui->setupUi(this);

	const QSettings profilesSettings(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking.ini")), QSettings::IniFormat);
	const QStringList globalProfiles(SettingsManager::getValue(QLatin1String("Content/BlockingProfiles")).toStringList());
	const QVector<ContentBlockingProfile*> profiles(ContentBlockingManager::getProfiles());
	QStandardItemModel *model(new QStandardItemModel(this));

	model->setHorizontalHeaderLabels(QStringList({tr("Title"), tr("Update Interval"), tr("Last Update")}));

	for (int i = 0; i < profiles.count(); ++i)
	{
		const QString name(profiles.at(i)->getName());

		if (name == QLatin1String("custom"))
		{
			continue;
		}

		QList<QStandardItem*> items({new QStandardItem(profiles.at(i)->getTitle()), new QStandardItem(profilesSettings.value(name + QLatin1String("/updateInterval")).toString()), new QStandardItem(Utils::formatDateTime(profilesSettings.value(name + QLatin1String("/lastUpdate")).toDateTime()))});
		items[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		items[0]->setData(name, Qt::UserRole);
		items[0]->setData(profiles.at(i)->getUpdateUrl(), (Qt::UserRole + 1));
		items[0]->setCheckable(true);
		items[0]->setCheckState(globalProfiles.contains(name) ? Qt::Checked : Qt::Unchecked);
		items[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		items[2]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		model->appendRow(items);
	}

	m_ui->enableCustomRulesCheckBox->setChecked(globalProfiles.contains("custom"));
	m_ui->profilesViewWidget->setModel(model);
	m_ui->profilesViewWidget->setItemDelegate(new OptionDelegate(true, this));
	m_ui->profilesViewWidget->setItemDelegateForColumn(1, new ContentBlockingIntervalDelegate(this));
	m_ui->profilesViewWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);

	QStandardItemModel *customRulesModel(new QStandardItemModel(this));
	QFile file(SessionsManager::getWritableDataPath("blocking/custom.txt"));

	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QTextStream stream(&file);

		if (stream.readLine().trimmed().startsWith(QLatin1String("[Adblock Plus"), Qt::CaseInsensitive))
		{
			while (!stream.atEnd())
			{
				customRulesModel->appendRow(new QStandardItem(stream.readLine().trimmed()));
			}
		}
		else
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to load custom rules: invalid adblock header"), Console::OtherCategory, Console::ErrorLevel, file.fileName());
		}

		file.close();
	}

	m_ui->customRulesViewWidget->setModel(customRulesModel);
	m_ui->customRulesViewWidget->setItemDelegate(new OptionDelegate(true, this));
	m_ui->enableWildcardsCheckBox->setChecked(SettingsManager::getValue(QLatin1String("ContentBlocking/EnableWildcards")).toBool());

	connect(ContentBlockingManager::getInstance(), SIGNAL(profileModified(QString)), this, SLOT(updateProfile(QString)));
	connect(m_ui->profilesViewWidget->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(updateProfilesActions()));
	connect(m_ui->updateProfileButton, SIGNAL(clicked(bool)), this, SLOT(updateProfile()));
	connect(m_ui->confirmButtonBox, SIGNAL(accepted()), this, SLOT(save()));
	connect(m_ui->confirmButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui->enableCustomRulesCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateRulesActions()));
	connect(m_ui->customRulesViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateRulesActions()));
	connect(m_ui->addRuleButton, SIGNAL(clicked(bool)), this, SLOT(addRule()));
	connect(m_ui->editRuleButton, SIGNAL(clicked(bool)), this, SLOT(editRule()));
	connect(m_ui->removeRuleButton, SIGNAL(clicked(bool)), this, SLOT(removeRule()));

	updateRulesActions();
}

ContentBlockingDialog::~ContentBlockingDialog()
{
	delete m_ui;
}

void ContentBlockingDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void ContentBlockingDialog::updateProfile()
{
	const QModelIndex index(m_ui->profilesViewWidget->currentIndex());

	if (index.isValid())
	{
		ContentBlockingManager::updateProfile(index.data(Qt::UserRole).toString());
	}
}

void ContentBlockingDialog::updateProfilesActions()
{
	const QModelIndex index(m_ui->profilesViewWidget->currentIndex());

	m_ui->updateProfileButton->setEnabled(index.isValid() && index.data(Qt::UserRole + 1).toUrl().isValid());
}

void ContentBlockingDialog::addRule()
{
	m_ui->customRulesViewWidget->insertRow();

	editRule();
}

void ContentBlockingDialog::editRule()
{
	m_ui->customRulesViewWidget->edit(m_ui->customRulesViewWidget->getIndex(m_ui->customRulesViewWidget->getCurrentRow()));
}

void ContentBlockingDialog::removeRule()
{
	m_ui->customRulesViewWidget->removeRow();
	m_ui->customRulesViewWidget->setFocus();

	updateRulesActions();
}

void ContentBlockingDialog::updateRulesActions()
{
	m_ui->tabWidget->setTabEnabled(1, m_ui->enableCustomRulesCheckBox->isChecked());

	const bool isEditable(m_ui->customRulesViewWidget->getCurrentRow() >= 0);

	m_ui->editRuleButton->setEnabled(isEditable);
	m_ui->removeRuleButton->setEnabled(isEditable);
}

void ContentBlockingDialog::updateProfile(const QString &name)
{
	ContentBlockingProfile *profile(ContentBlockingManager::getProfile(name));

	if (!profile)
	{
		return;
	}

	for (int i = 0; i < m_ui->profilesViewWidget->getRowCount(); ++i)
	{
		const QModelIndex index(m_ui->profilesViewWidget->getIndex(i, 0));

		if (index.data(Qt::UserRole).toString() == name)
		{
			const QSettings profilesSettings(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking.ini")), QSettings::IniFormat);

			m_ui->profilesViewWidget->setData(index, profile->getTitle(), Qt::DisplayRole);
			m_ui->profilesViewWidget->setData(index.sibling(i, 2), Utils::formatDateTime(profilesSettings.value(name + QLatin1String("/lastUpdate")).toDateTime()), Qt::DisplayRole);

			break;
		}
	}
}

void ContentBlockingDialog::save()
{
	QSettings profilesSettings(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking.ini")), QSettings::IniFormat);
	QStringList profiles;

	for (int i = 0; i < m_ui->profilesViewWidget->getRowCount(); ++i)
	{
		profilesSettings.beginGroup(m_ui->profilesViewWidget->getIndex(i, 0).data(Qt::UserRole).toString());
		profilesSettings.setValue(QLatin1String("updateInterval"), m_ui->profilesViewWidget->getIndex(i, 1).data(Qt::DisplayRole));
		profilesSettings.endGroup();

		if (m_ui->profilesViewWidget->getIndex(i, 0).data(Qt::CheckStateRole).toBool())
		{
			profiles.append(m_ui->profilesViewWidget->getIndex(i, 0).data(Qt::UserRole).toString());
		}
	}

	if (m_ui->enableCustomRulesCheckBox->isChecked())
	{
		QFile file(SessionsManager::getWritableDataPath("blocking/custom.txt"));

		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to create a file with custom rules: %1").arg(file.errorString()), Console::OtherCategory, Console::ErrorLevel, file.fileName());

			QMessageBox::critical(this, tr("Error"), tr("Failed to create a file with custom rules!"), QMessageBox::Close);
		}
		else
		{
			file.write(QStringLiteral("[AdBlock Plus 2.0]\n").toUtf8());

			for (int i = 0; i < m_ui->customRulesViewWidget->getRowCount(); ++i)
			{
				file.write(m_ui->customRulesViewWidget->getIndex(i, 0).data().toString().toLatin1() + QStringLiteral("\n").toUtf8());
			}

			file.close();

			profiles.append(QLatin1String("custom"));

			ContentBlockingProfile *profile(ContentBlockingManager::getProfile(QLatin1String("custom")));

			if (profile)
			{
				profile->clear();
			}
		}
	}

	SettingsManager::setValue(QLatin1String("Content/BlockingProfiles"), profiles);
	SettingsManager::setValue(QLatin1String("ContentBlocking/EnableWildcards"), m_ui->enableWildcardsCheckBox->isChecked());

	close();
}

}
