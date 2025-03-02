/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2022 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 Piotr Wójcik <chocimier@tlen.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "UserScriptsPage.h"
#include "../../../core/JsonSettings.h"
#include "../../../core/SessionsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/UserScript.h"

#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

namespace Otter
{

UserScriptsPage::UserScriptsPage(QWidget *parent) : AddonsPage(parent)
{
	connect(this, &AddonsPage::needsActionsUpdate, this, [&]()
	{
		const QModelIndexList indexes(getSelectedIndexes());

		if (indexes.isEmpty())
		{
			emit currentAddonChanged(nullptr);
		}
		else
		{
			emit currentAddonChanged(AddonsManager::getUserScript(indexes.first().data(IdentifierRole).toString()));
		}
	});
	connect(AddonsManager::getInstance(), &AddonsManager::userScriptModified, this, [&](const QString &name)
	{
		Addon *addon(AddonsManager::getUserScript(name));

		if (addon)
		{
			updateAddonEntry(addon);
		}
	});
}

void UserScriptsPage::delayedLoad()
{
	const QStringList userScripts(AddonsManager::getAddons(Addon::UserScriptType));

	for (int i = 0; i < userScripts.count(); ++i)
	{
		UserScript *script(AddonsManager::getUserScript(userScripts.at(i)));

		if (script)
		{
			addAddonEntry(script);
		}
	}
}

void UserScriptsPage::addAddon()
{
	const QStringList sourcePaths(QFileDialog::getOpenFileNames(this, tr("Select Files"), QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0), Utils::formatFileTypes({tr("User Script files (*.js)")})));

	if (sourcePaths.isEmpty())
	{
		return;
	}

	QStringList failedPaths;
	ReplaceMode replaceMode(UnknownMode);

	for (int i = 0; i < sourcePaths.count(); ++i)
	{
		if (sourcePaths.at(i).isEmpty())
		{
			continue;
		}

		const QString scriptName(QFileInfo(sourcePaths.at(i)).completeBaseName());
		const QString targetDirectory(QDir(SessionsManager::getWritableDataPath(QLatin1String("scripts"))).filePath(scriptName));
		const QString targetPath(QDir(targetDirectory).filePath(QFileInfo(sourcePaths.at(i)).fileName()));
		bool isReplacingScript(false);

		if (QFile::exists(targetPath))
		{
			if (replaceMode == IgnoreAllMode)
			{
				continue;
			}

			if (replaceMode == ReplaceAllMode)
			{
				isReplacingScript = true;
			}
			else
			{
				QMessageBox messageBox;
				messageBox.setWindowTitle(tr("Question"));
				messageBox.setText(tr("User Script with this name already exists:\n%1").arg(targetPath));
				messageBox.setInformativeText(tr("Do you want to replace it?"));
				messageBox.setIcon(QMessageBox::Question);
				messageBox.addButton(QMessageBox::Yes);
				messageBox.addButton(QMessageBox::No);

				if (i < (sourcePaths.count() - 1))
				{
					messageBox.setCheckBox(new QCheckBox(tr("Apply to all")));
				}

				messageBox.exec();

				isReplacingScript = (messageBox.standardButton(messageBox.clickedButton()) == QMessageBox::Yes);

				if (messageBox.checkBox() && messageBox.checkBox()->isChecked())
				{
					replaceMode = (isReplacingScript ? ReplaceAllMode : IgnoreAllMode);
				}
			}

			if (!isReplacingScript)
			{
				continue;
			}
		}

		if ((isReplacingScript && !QDir().remove(targetPath)) || (!isReplacingScript && !QDir().mkpath(targetDirectory)) || !QFile::copy(sourcePaths.at(i), targetPath))
		{
			failedPaths.append(sourcePaths.at(i));

			continue;
		}

		if (isReplacingScript)
		{
			UserScript *script(AddonsManager::getUserScript(scriptName));

			if (script)
			{
				script->reload();
			}
		}
		else
		{
			UserScript script(targetPath);

			addAddonEntry(&script, {{IdentifierRole, script.getName()}});
		}
	}

	if (!failedPaths.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to import following User Script file(s):\n%1", "", failedPaths.count()).arg(failedPaths.join(QLatin1Char('\n'))), QMessageBox::Close);
	}

///TODO apply changes later, take removed addons into account?
	save();

	AddonsManager::loadUserScripts();
}

void UserScriptsPage::openAddons()
{
	const QVector<UserScript*> addons(getSelectedUserScripts());

	for (int i = 0; i < addons.count(); ++i)
	{
		Utils::runApplication({}, addons.at(i)->getPath());
	}
}

void UserScriptsPage::reloadAddons()
{
	const QVector<UserScript*> addons(getSelectedUserScripts());

	for (int i = 0; i < addons.count(); ++i)
	{
		addons.at(i)->reload();

		updateAddonEntry(addons.at(i));
	}
}

void UserScriptsPage::removeAddons()
{
	const QVector<UserScript*> addons(getSelectedUserScripts());

	if (addons.isEmpty())
	{
		return;
	}

	bool hasAddonsToRemove(false);
	QMessageBox messageBox;
	messageBox.setWindowTitle(tr("Question"));
	messageBox.setText(tr("You are about to irreversibly remove %n addon(s).", "", addons.count()));
	messageBox.setInformativeText(tr("Do you want to continue?"));
	messageBox.setIcon(QMessageBox::Question);
	messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	messageBox.setDefaultButton(QMessageBox::Yes);

	if (messageBox.exec() == QMessageBox::Yes)
	{
		for (int i = 0; i < addons.count(); ++i)
		{
			if (addons.at(i)->canRemove())
			{
				m_addonsToRemove.append(addons.at(i)->getName());

				hasAddonsToRemove = true;
			}
		}
	}

	if (hasAddonsToRemove)
	{
		emit settingsModified();
	}
}

void UserScriptsPage::save()
{
	for (int i = 0; i < m_addonsToRemove.count(); ++i)
	{
		Addon *addon(AddonsManager::getUserScript(m_addonsToRemove.at(i)));

		if (addon)
		{
			addon->remove();
		}
	}

	QStandardItemModel *model(getModel());
	QModelIndexList indexesToRemove;
	QJsonObject settingsObject;

	for (int i = 0; i < model->rowCount(); ++i)
	{
		const QModelIndex index(model->index(i, 0));

		if (index.isValid())
		{
			const QString name(index.data(IdentifierRole).toString());

			if (!name.isEmpty() && AddonsManager::getUserScript(name))
			{
				settingsObject.insert(name, QJsonObject({{QLatin1String("isEnabled"), QJsonValue(index.data(Qt::CheckStateRole).toInt() == Qt::Checked)}}));
			}
			else
			{
				indexesToRemove.append(index);
			}
		}
	}

	JsonSettings settings;
	settings.setObject(settingsObject);
	settings.save(SessionsManager::getWritableDataPath(QLatin1String("scripts/scripts.json")));

	for (int i = (indexesToRemove.count() - 1); i >= 0; --i)
	{
		model->removeRow(indexesToRemove.at(i).row(), indexesToRemove.at(i).parent());
	}

	m_addonsToAdd.clear();
	m_addonsToRemove.clear();
}

QString UserScriptsPage::getTitle() const
{
	return tr("User Scripts");
}

QIcon UserScriptsPage::getFallbackIcon() const
{
	return ThemesManager::createIcon(QLatin1String("addon-user-script"), false);
}

QVariant UserScriptsPage::getAddonIdentifier(Addon *addon) const
{
	return addon->getName();
}

QVector<UserScript*> UserScriptsPage::getSelectedUserScripts() const
{
	const QModelIndexList indexes(getSelectedIndexes());
	QVector<UserScript*> userScripts;
	userScripts.reserve(indexes.count());

	for (int i = 0; i < indexes.count(); ++i)
	{
		UserScript *script(AddonsManager::getUserScript(indexes.at(i).data(IdentifierRole).toString()));

		if (script)
		{
			userScripts.append(script);
		}
	}

	userScripts.squeeze();

	return userScripts;
}

bool UserScriptsPage::canOpenAddons() const
{
	return true;
}

bool UserScriptsPage::canReloadAddons() const
{
	return true;
}

}
