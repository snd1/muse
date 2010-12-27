#include <stdio.h>
#include <qfiledialog.h>
#include <qdir.h>
#include "projectcreateimpl.h"
#include "gconfig.h"
#include "globals.h"
#include "app.h"

ProjectCreateImpl::ProjectCreateImpl(QWidget *parent) :
    QDialog(parent)
{
  setupUi(this);

  createFolderCheckbox->setChecked(config.projectStoreInFolder);
  connect(browseDirButton,SIGNAL(clicked()), this, SLOT(selectDirectory()));
  connect(projectNameEdit,SIGNAL(textChanged(QString)), this, SLOT(updateDirectoryPath()));
  connect(createFolderCheckbox,SIGNAL(clicked()), this, SLOT(updateDirectoryPath()));
  connect(buttonBox, SIGNAL(accepted()), this, SLOT(ok()));
#if QT_VERSION >= 0x040700
  projectNameEdit->setPlaceholderText("<Project Name>");
  commentEdit->setPlaceholderText("<Add information about project here>");
#endif
  directoryPath = config.projectBaseFolder;
  updateDirectoryPath();
  show();
}

void ProjectCreateImpl::selectDirectory()
{
  QFileDialog qfd;
  qfd.selectFile(directoryPath);
  qfd.setFileMode(QFileDialog::DirectoryOnly);
  if (qfd.exec() == QDialog::Rejected) {
    return;
  }
  directoryPath=qfd.selectedFiles().first();
  updateDirectoryPath();
}

void ProjectCreateImpl::updateDirectoryPath()
{
  if (createFolderCheckbox->isChecked()) {
    storageDirEdit->setText(directoryPath + projectNameEdit->text() + "/" + projectNameEdit->text() + ".med");
  }  else {
    storageDirEdit->setText(directoryPath + projectNameEdit->text() + ".med");
  }
}

QString ProjectCreateImpl::getProjectPath()
{
   return storageDirEdit->text();
}
QString ProjectCreateImpl::getSongInfo()
{
   return commentEdit->toPlainText();
}
void ProjectCreateImpl::ok()
{
  config.projectStoreInFolder = createFolderCheckbox->isChecked();
  config.projectBaseFolder = directoryPath;
  muse->changeConfig(true);
  emit accept();
}
