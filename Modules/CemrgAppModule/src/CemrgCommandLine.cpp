/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date$
Version:   $Revision$

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
/*=========================================================================
 *
 * Commandline Tools for MITK
 *
 * Cardiac Electromechanics Research Group
 * http://www.cemrg.co.uk/
 * orod.razeghi@kcl.ac.uk
 *
 * This software is distributed WITHOUT ANY WARRANTY or SUPPORT!
 *
=========================================================================*/

// Qmitk
#include <mitkIOUtil.h>
#include <mitkProgressBar.h>

// Qt
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>

#include <thread>
#include <chrono>
#include <sys/stat.h>
#include "CemrgCommandLine.h"


CemrgCommandLine::CemrgCommandLine() {
    isUI = true;
    //Setup panel
    panel = new QTextEdit(0,0);
    QPalette palette = panel->palette();
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::Text, Qt::red);
    panel->setPalette(palette);
    panel->setReadOnly(true);

    //Setup dialog
    layout = new QVBoxLayout();
    dial = new QDialog(0,0);
    dial->setFixedSize(640, 480);
    dial->setLayout(layout);
    dial->layout()->addWidget(panel);
    dial->show();

    //Setup the process
    process = std::unique_ptr<QProcess>(new QProcess(this));
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process.get(), SIGNAL(readyReadStandardOutput()), this, SLOT(UpdateStdText()));
    connect(process.get(), SIGNAL(readyReadStandardError()), this, SLOT(UpdateErrText()));
    connect(process.get(), SIGNAL(finished(int)), this, SLOT(FinishedAlert()));
}

CemrgCommandLine::CemrgCommandLine(bool cmd) {
  isUI = cmd;

  if(cmd){
    //Setup panel
    panel = new QTextEdit(0,0);
    QPalette palette = panel->palette();
    palette.setColor(QPalette::Base, Qt::black);
    palette.setColor(QPalette::Text, Qt::red);
    panel->setPalette(palette);
    panel->setReadOnly(true);

    //Setup dialog
    layout = new QVBoxLayout();
    dial = new QDialog(0,0);
    dial->setFixedSize(640, 480);
    dial->setLayout(layout);
    dial->layout()->addWidget(panel);
    dial->show();
  }
  //Setup the process
  process = std::unique_ptr<QProcess>(new QProcess(this));
  process->setProcessChannelMode(QProcess::MergedChannels);
  connect(process.get(), SIGNAL(readyReadStandardOutput()), this, SLOT(UpdateStdText()));
  connect(process.get(), SIGNAL(readyReadStandardError()), this, SLOT(UpdateErrText()));
  connect(process.get(), SIGNAL(finished(int)), this, SLOT(FinishedAlert()));
}

CemrgCommandLine::~CemrgCommandLine() {

    process->close();
    dial->deleteLater();
    panel->deleteLater();
    layout->deleteLater();
}

/***************************************************************************
 ************************* BUILDING MESH UTILITIES *************************
 ***************************************************************************/

QString CemrgCommandLine::ExecuteSurf(QString dir, QString segPath, int iter, float th, int blur, int smth) {

  QString retOutput;
  QString dockerOutput = dockerSurf(dir, segPath, iter, th, blur, smth);

  if(!isOutputSuccessful(dockerOutput)){
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
#if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
#endif
    process->setWorkingDirectory(aPath);

    QDir apathd(aPath);
    if (apathd.exists()){
      //Dilation
      QStringList arguments;
      QString input  = segPath;
      QString output = dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.d.nii";
      QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "dilate-image";
      arguments << input;
      arguments << output;
      arguments << "-iterations" << QString::number(iter);
      arguments << "-verbose" << "3";
      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      //Erosion
      arguments.clear();
      input  = output;
      output = dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.s.nii";
      mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "erode-image";
      arguments << input;
      arguments << output;
      arguments << "-iterations" << QString::number(iter);
      arguments << "-verbose" << "3";
      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      //Marching Cubes
      arguments.clear();
      input  = output;
      output = dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.vtk";
      mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "extract-surface";
      arguments << input;
      arguments << output;
      arguments << "-isovalue" << QString::number(th);
      arguments << "-blur" << QString::number(blur);
      arguments << "-ascii";
      arguments << "-verbose" << "3";
      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      //Smoothing
      arguments.clear();
      input  = output;
      output = output+"";
      mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "smooth-surface";
      arguments << input;
      arguments << output;
      arguments << "-iterations" << QString::number(smth);
      arguments << "-verbose" << "3";
      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      //Return path to output mesh
      remove((dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.d.nii").toStdString().c_str());
      remove((dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.s.nii").toStdString().c_str());

      retOutput = output;
    } else {
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+  mitk::IOUtil::GetProgramPath();

      retOutput = "";
    }
  } else {
      retOutput = dockerOutput;
  }
  return retOutput;
}

QString CemrgCommandLine::ExecuteCreateCGALMesh(QString dir, QString fileName, QString templatePath) {

  QString dockerOutput = dockerCreateCGALMesh(dir, fileName, templatePath);
  QString retOutput;

  if(!isOutputSuccessful(dockerOutput)){
    MITK_WARN << "Docker did not produce a good outcome. Trying with local Meshtools3D libraries.";
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "M3DLib";
    #if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("M3DLib");
    #endif

    QDir apathd(aPath);
    if(apathd.exists()){
      process->setWorkingDirectory(aPath);

      //Setup EnVariable - in windows TBB_NUM_THREADS should be set in the system environment variables
      #ifndef _WIN32
      //	QString setenv = "set TBB_NUM_THREADS=4";
      //	process->start(setenv);
      //#else
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
      env.insert("TBB_NUM_THREADS","12");
      process->setProcessEnvironment(env);
      #endif

      //Setup Mesh3DTool
      QStringList arguments;
      QString input  = templatePath;
      QString output = dir + mitk::IOUtil::GetDirectorySeparator() + "CGALMeshDir";
      QString mesh3D = aPath + mitk::IOUtil::GetDirectorySeparator() + "meshtools3d";

      arguments << "-f" << input;
      arguments << "-seg_dir" << dir;
      arguments << "-seg_name" << "converted.inr";
      arguments << "-out_dir" << output;
      arguments << "-out_name" << fileName;

      completion = false;
      process->start(mesh3D, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      //Return path to output CGAL mesh
      retOutput = output + mitk::IOUtil::GetDirectorySeparator() + fileName + ".vtk";
    }
    else{
      QMessageBox::warning(NULL, "Please check the LOG", "Meshtools3D libraries not found");
      MITK_WARN << "Meshtools3D libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath();
      retOutput = "";
    }

  }
  else
    retOutput = dockerOutput;

  return retOutput;
}

/***************************************************************************
 **************************** TRACKING UTILITIES ***************************
 ***************************************************************************/

void CemrgCommandLine::ExecuteTracking(QString dir, QString imgTimes, QString param) {

  bool successful = dockerTracking(dir, imgTimes, param);

  if(!successful){
    MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
    #if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
    #endif

    QDir apathd(aPath);
    if(apathd.exists()){
      process->setWorkingDirectory(aPath);

      //Setup
      QStringList arguments;
      QString output = dir + mitk::IOUtil::GetDirectorySeparator() + "tsffd.dof";
      QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "register";

      arguments << "-images" << imgTimes;
      if (!param.isEmpty()) arguments << "-parin" << param;
      arguments << "-dofout" << output;
      arguments << "-threads" << "12";
      arguments << "-verbose" << "3";

      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();
    }
   else{
    QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");
    MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
      mitk::IOUtil::GetProgramPath();
    }
  }

}

void CemrgCommandLine::ExecuteApplying(QString dir, QString inputMesh, double iniTime, QString dofin, int noFrames, int smooth) {
  bool successful = dockerApplying(dir, inputMesh, iniTime, dofin, noFrames, smooth);
  if(!successful){
    MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
#if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
#endif

    QDir apathd(aPath);
    if(apathd.exists()){
      process->setWorkingDirectory(aPath);
      //Setup
      QStringList arguments;
      QString input  = inputMesh;
      QString output = dir + mitk::IOUtil::GetDirectorySeparator() + "transformed-";
      QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "transform-points";
      int fctTime = 10;
      noFrames *= smooth;
      if (smooth == 2)
      fctTime = 5;
      else if (smooth == 5)
      fctTime = 2;

      for (int i=0; i<noFrames; i++) {

        arguments.clear();
        arguments << input;
        arguments << output + QString::number(i) + ".vtk";
        arguments << "-dofin" << dofin;
        arguments << "-ascii";
        arguments << "-St";
        arguments << QString::number(iniTime);
        arguments << "-verbose" << "3";

        completion = false;
        process->start(mirtk, arguments);
        while (!completion) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        iniTime += fctTime;
        mitk::ProgressBar::GetInstance()->Progress();
      }
    } else{
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");
      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath();
    }
  }
}

void CemrgCommandLine::ExecuteRegistration(QString dir, QString lge, QString mra) {

    //Setup registration
    QString input1 = dir + mitk::IOUtil::GetDirectorySeparator() + mra + ".nii";
    QString input2 = dir + mitk::IOUtil::GetDirectorySeparator() + lge + ".nii";
    QString output = dir + mitk::IOUtil::GetDirectorySeparator() + "rigid.dof";

    bool successful = dockerRegistration(dir, input2, input1, output, "Rigid");

    if(!successful){
      //Absolute path
      MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
      QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
#if defined(__APPLE__)
      aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
              mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
              mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
#endif
      QDir apathd(aPath);
      if (apathd.exists()){
        QStringList arguments;
        QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "register";
        process->setWorkingDirectory(aPath);

        arguments << input1;
        arguments << input2;
        arguments << "-dofout" << output;
        arguments << "-model" << "Rigid";
        arguments << "-verbose" << "3";

        completion = false;
        process->start(mirtk, arguments);
        while (!completion) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        mitk::ProgressBar::GetInstance()->Progress();
    } else{
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath();
    }
  }
}

void CemrgCommandLine::ExecuteRegistration(QString dir, QString fixedfullpath,
  QString movingfullpath, QString txname, QString modelname) {

    bool successful = dockerRegistration(dir, fixedfullpath, movingfullpath, txname, modelname);

    if(!successful){
      MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
      //Absolute path
      QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
#if defined(__APPLE__)
      aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
              mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
              mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
#endif
      QDir apathd(aPath);
      if (apathd.exists()){
        process->setWorkingDirectory(aPath);

        //Setup registration
        QStringList arguments;
        QString input1 = movingfullpath;
        QString input2 = fixedfullpath;
        QString output = txname;
        QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "register";

        arguments << input1;
        arguments << input2;
        arguments << "-dofout" << output;
        arguments << "-model" << modelname;
        arguments << "-verbose" << "3";

        completion = false;

        process->start(mirtk, arguments);
        MITK_INFO << "Executing a " + modelname.toStdString() + " registration.";
          while (!completion) {
              std::this_thread::sleep_for(std::chrono::seconds(1));
              QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
          }
          mitk::ProgressBar::GetInstance()->Progress();
    } else{
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath();
    }
    }
}

void CemrgCommandLine::ExecuteTransformation(QString dir, QString imgName, QString regImgName) {

  QString input  = imgName;
  QString output = regImgName;
  QString dofpath = dir + mitk::IOUtil::GetDirectorySeparator() + "rigid.dof";

  bool successful = dockerTranformation(dir, input, output, dofpath);

  if(!successful){
    MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
    #if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
    #endif
    QDir apathd(aPath);
    if (apathd.exists()){
      process->setWorkingDirectory(aPath);

      //Setup transformation
      QStringList arguments;
      QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "transform-image";

      arguments << input;
      arguments << output;
      arguments << "-dof" << dofpath;
      arguments << "-verbose" << "3";

      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();
    } else{
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
      mitk::IOUtil::GetProgramPath() + "\nLooked for folder in: \n\t" + aPath.toStdString();
    }
  }

}

void CemrgCommandLine::ExecuteTransformation(QString dir, QString imgNamefullpath,
  QString regImgNamefullpath, QString txfullpath) {

    bool successful = dockerTranformation(dir, imgNamefullpath, regImgNamefullpath, txfullpath);

    if(!successful){
      MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
      //Absolute path
      QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
      #if defined(__APPLE__)
      aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
      mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
      mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
      #endif
      QDir apathd(aPath);
      if (apathd.exists()){
        process->setWorkingDirectory(aPath);

        //Setup transformation
        QStringList arguments;
        QString input  = imgNamefullpath;
        QString output = regImgNamefullpath;
        QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "transform-image";

        arguments << input;
        arguments << output;
        arguments << "-dof" << txfullpath;
        arguments << "-verbose" << "3";

        completion = false;
        process->start(mirtk, arguments);
        while (!completion) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        mitk::ProgressBar::GetInstance()->Progress();
      } else{
        QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

        MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath() + "\nLooked for folder in: \n\t" + aPath.toStdString();
      }
    }
}

void CemrgCommandLine::ExecuteResamplingOmNifti(QString niifullpath,
  QString outputtniifullpath, int isovalue) {
// /resample-image niifullpath outputtniifullpath -isotropic 0.5 -interp CSpline -verbose 3
  bool successful = dockerResamplingOmNifti(niifullpath, outputtniifullpath, isovalue);

  if(!successful){
    MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
    //Absolute path
    QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
    #if defined(__APPLE__)
    aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
    mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
    mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
    #endif
    QDir apathd(aPath);
    if (apathd.exists()){
      process->setWorkingDirectory(aPath);

      //Setup transformation
      QStringList arguments;
      QString input  = niifullpath;
      QString output = outputtniifullpath;
      QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "resample-image";

      arguments << input;
      arguments << output;
      arguments << "-isotropic" << QString::number(isovalue);
      arguments << "-interp" << "CSpline";
      arguments << "-verbose" << "3";

      completion = false;
      process->start(mirtk, arguments);
      while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();
    } else{
      QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");

      MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
      mitk::IOUtil::GetProgramPath();
    }
  }
}

void CemrgCommandLine::ExecuteTransformationOnPoints(QString dir, QString meshfullpath,
  QString outputtmeshfullpath, QString txfullpath) {

    bool successful = dockerTransformationOnPoints(dir, meshfullpath,  outputtmeshfullpath, txfullpath);

    if(!successful){
      MITK_WARN << "Docker did not produce a good outcome. Trying with local MIRTK libraries.";
      //Absolute path
      QString aPath = QString::fromStdString(mitk::IOUtil::GetProgramPath()) + mitk::IOUtil::GetDirectorySeparator() + "MLib";
      #if defined(__APPLE__)
      aPath = mitk::IOUtil::GetDirectorySeparator() + QString("Applications") +
      mitk::IOUtil::GetDirectorySeparator() + QString("CemrgApp") +
      mitk::IOUtil::GetDirectorySeparator() + QString("MLib");
      #endif
      QDir apathd(aPath);
      if (apathd.exists()){
        process->setWorkingDirectory(aPath);

        //Setup transformation
        QStringList arguments;
        QString input  = meshfullpath;
        QString output = outputtmeshfullpath;
        QString mirtk  = aPath + mitk::IOUtil::GetDirectorySeparator() + "transform-points";

        arguments << input;
        arguments << output;
        arguments << "-dofin" << txfullpath;
        arguments << "-ascii";
        arguments << "-verbose" << "3";

        completion = false;
        process->start(mirtk, arguments);
        while (!completion) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        mitk::ProgressBar::GetInstance()->Progress();
      } else{
        QMessageBox::warning(NULL, "Please check the LOG", "MIRTK libraries not found");
        MITK_WARN << "MIRTK libraries not found. Please make sure the MLib folder is inside the directory;\n\t"+
        mitk::IOUtil::GetProgramPath();
      }
    }
}

/***************************************************************************
 ************************** SERVER CONC UTILITIES **************************
 ***************************************************************************/

bool CemrgCommandLine::ConnectToServer(QString userID, QString server) {

    //Setup ssh pass prompt
#if defined(__APPLE__)
    ofstream sshAskPass;
    sshAskPass.open(QDir::homePath().toStdString() + "/.ssh/ssh-askpass");
    sshAskPass << "#!/bin/bash" << endl;
    sshAskPass << "TITLE=\"${SSH_ASKPASS_TITLE:-SSH}\";" << endl;
    sshAskPass << "TEXT=\"$(whoami)'s password:\";" << endl;
    sshAskPass << "IFS=$(printf \"\\n\");" << endl;
    sshAskPass << "CODE=(\"on GetCurrentApp()\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"tell application \\\"System Events\\\" to get short name of first process whose frontmost is true\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"end GetCurrentApp\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"tell application GetCurrentApp()\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"activate\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"display dialog \\\"${@:-$TEXT}\\\" default answer \\\"\\\" with title \\\"${TITLE}\\\" with icon caution with hidden answer\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"text returned of result\");" << endl;
    sshAskPass << "CODE=(${CODE[*]} \"end tell\");" << endl;
    sshAskPass << "SCRIPT=\"/usr/bin/osascript\"" << endl;
    sshAskPass << "for LINE in ${CODE[*]}; do" << endl;
    sshAskPass << "\tSCRIPT=\"${SCRIPT} -e $(printf \"%q\" \"${LINE}\")\";" << endl;
    sshAskPass << "done;" << endl;
    sshAskPass << "eval \"${SCRIPT}\";" << endl;
    sshAskPass.close();
    chmod((QDir::homePath().toStdString() + "/.ssh/ssh-askpass").c_str(), S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IXUSR|S_IXGRP|S_IXOTH);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SSH_ASKPASS", (QDir::homePath().toStdString() + "/.ssh/ssh-askpass").c_str());
    process->setProcessEnvironment(env);
#endif

    //Setup ssh config file
    ofstream sshConfigFile;
    sshConfigFile.open(QDir::homePath().toStdString() + "/.ssh/config");
    sshConfigFile << "Host " + server.toStdString() << endl;
    sshConfigFile << "ControlMaster auto" << endl;
    sshConfigFile << "ControlPath ~/.ssh/%r@%h:%p";
    sshConfigFile.close();

    //Setup connection
    QStringList arguments;
    QString connection = "ssh";
    QString usernameID = userID;
    QString serverName = server;
    arguments << usernameID + "@" + serverName;

    completion = false;
    process->start(connection, arguments);
    if (!process->waitForStarted(20000)) {
        process->close();
        return false;
    }
    if (!process->waitForReadyRead(20000)) {
        process->close();
        return false;
    }//_if_connection

    //User logged in
    process->write("mkdir ~/CEMRG-GPUReconstruction\n");
    process->write("echo\n"); process->write("echo\n");
    process->write("echo 'Festive Connection Established!'\n");
    process->write("echo\n"); process->write("echo\n");
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if (panel->toPlainText().contains("Festive Connection Established!")) return true;
        if (panel->toPlainText().contains("ssh Completed!")) return false;
    }//_while

    return false;
}

bool CemrgCommandLine::TransferTFServer(QString directory, QString fname, QString userID, QString server, bool download) {

    //Setup transfer command
    QStringList arguments;
    QString transfer = "scp";
    QString usernameID = userID;
    QString serverName = server;

    //Setup download/upload
    if (download == false) {

        //Clear remote host first
        arguments << usernameID + "@" + serverName;
        arguments << "rm -rf" << "~/CEMRG-GPUReconstruction/" + fname;
        process->start("ssh", arguments);
        if (!process->waitForFinished(60000)) {
            process->close();
            return false;
        }//_if_logged
        arguments.clear();
        arguments << "-r" << directory + mitk::IOUtil::GetDirectorySeparator() + fname;
        arguments << usernameID + "@" + serverName + ":~/CEMRG-GPUReconstruction";
        completion = false;
        process->start(transfer, arguments);
        if (!process->waitForStarted(20000)) {
            process->close();
            return false;
        }//_if_logged
        while (!completion) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (panel->toPlainText().contains("lost connection")) return false;
            if (panel->toPlainText().contains("scp Completed!")) return true;
        }//_while

    } else {

        arguments.clear();
        arguments << usernameID + "@" + serverName + ":~/CEMRG-GPUReconstruction/" + fname;
        arguments << directory;
        completion = false;
        process->start(transfer, arguments);
        if (!process->waitForStarted(20000)) {
            process->close();
            return false;
        }//_if_logged
        while (!completion) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (panel->toPlainText().contains("No such file or directory")) return false;
            if (panel->toPlainText().contains("scp Completed!")) return true;
        }//_while

    }//_if_upload
    return false;
}

void CemrgCommandLine::GPUReconstruction(QString userID, QString server, QStringList imgsList, QString targetImg, double resolution, double delta, int package, QString out) {

    //Setup remote commands
    QStringList arguments;
    QString usernameID = userID;
    QString serverName = server;
    QString cwdCommand = "cd ~/CEMRG-GPUReconstruction/Transfer;";

    //Set order of images
    QString packageList = "";
    imgsList.removeAt(imgsList.indexOf("Mask.nii.gz"));
    imgsList.removeAt(imgsList.indexOf(targetImg));
    imgsList.insert(0, targetImg);
    for (int i=0; i<imgsList.size(); i++)
        packageList = packageList + QString::number(package) + " ";

    //Setup reconstruction command
    arguments << usernameID + "@" + serverName;
    arguments << cwdCommand;
    arguments << "reconstruction_GPU2";
    arguments << "-o" << "../" + out;
    arguments << "-i" << imgsList;
    arguments << "-m" << "Mask.nii.gz";
    arguments << "-d" << QString::number(0);
    arguments << "--resolution" << QString::number(resolution);
    arguments << "--delta" << QString::number(delta);
    arguments << "--packages" << packageList.trimmed();

    completion = false;
    process->start("ssh", arguments);
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }//_while
}

// Docker
bool CemrgCommandLine::dockerRegistration(QString directory, QString fixed, QString moving, QString txname, QString modelname){
  MITK_INFO << "[ATTENTION] Attempting MIRTK REGISTRATION using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif

  QDir mirtkhome(directory);

  QString fixedRelativePath = mirtkhome.relativeFilePath(fixed);
  QString movingRelativePath = mirtkhome.relativeFilePath(moving);
  QString dofRelativePath = mirtkhome.relativeFilePath(txname);

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QStringList arguments;
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";

  //Setup registration
  QString input1 = movingRelativePath;
  QString input2 = fixedRelativePath;
  QString output = dofRelativePath;
  QString dockerexe  = "register";

  arguments << dockerimage;
  arguments << dockerexe;
  arguments << input1;
  arguments << input2;
  arguments << "-dofout" << output;
  arguments << "-model" << modelname;
  arguments << "-verbose" << "3";
  arguments << "-color";

  MITK_INFO << "Executing a " + modelname.toStdString() + " registration.";

  MITK_INFO << ("Performing a " + modelname + " registration").toStdString();
  MITK_WARN << printFullCommand(docker, arguments);

  MITK_INFO << "\n";
  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  QString  outAbsolutepath = mirtkhome.absolutePath() +
      mitk::IOUtil::GetDirectorySeparator() + dofRelativePath;
  bool successful = isOutputSuccessful(outAbsolutepath);
  if (!successful)
    MITK_WARN << "Docker unsuccessful. Check your configuration.";

  return successful;
}

bool CemrgCommandLine::dockerTranformation(QString directory, QString imgNamefullpath, QString regImgNamefullpath, QString txfullpath){
  MITK_INFO << "[ATTENTION] Attempting MIRTK IMAGE TRANSFORMATION using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif

  QDir mirtkhome(directory);

  QString inputRelativePath = mirtkhome.relativeFilePath(imgNamefullpath);
  QString outputRelativePath = mirtkhome.relativeFilePath(regImgNamefullpath);
  QString dofRelativePath = mirtkhome.relativeFilePath(txfullpath);

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QStringList arguments;
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";

  //Setup transformation
  QString dockerexe  = "transform-image";

  arguments << dockerimage;
  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-dofin" << dofRelativePath;
  arguments << "-verbose" << "3";

  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  QString  outAbsolutepath = mirtkhome.absolutePath() +
      mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;

  bool successful = isOutputSuccessful(outAbsolutepath);
  if (!successful)
    MITK_WARN << "Docker unsuccessful. Check your configuration.";

  return successful;

}

bool CemrgCommandLine::dockerTransformationOnPoints(QString directory, QString meshfullpath, QString outputtmeshfullpath, QString txfullpath){
  MITK_INFO << "[ATTENTION] Attempting MIRTK MESH TRANSFORMATION using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif

  QDir mirtkhome(directory);

  QString inputRelativePath = mirtkhome.relativeFilePath(meshfullpath);
  QString outputRelativePath = mirtkhome.relativeFilePath(outputtmeshfullpath);
  QString dofRelativePath = mirtkhome.relativeFilePath(txfullpath);

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QStringList arguments;
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";

  //Setup transformation
  QString dockerexe  = "transform-points";

  arguments << dockerimage;
  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-dofin" << dofRelativePath;
  arguments << "-nocompress";
  arguments << "-ascii";
  arguments << "-verbose" << "3";

  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  QString  outAbsolutepath = mirtkhome.absolutePath() +
      mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;

  bool successful = isOutputSuccessful(outAbsolutepath);
  if (!successful)
    MITK_WARN << "Docker unsuccessful. Check your configuration.";

  return successful;
}

QString CemrgCommandLine::dockerExpandSurf(QString dir, QString segPath, int iter, float th, int blur, int smth){
  MITK_INFO << "[ATTENTION] Attempting SURFACE CREATION using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif

  QDir mirtkhome(dir);

  QString inputRelativePath = mirtkhome.relativeFilePath(segPath);
  QString outputRelativePath = mirtkhome.relativeFilePath("segmentation.s.nii");

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";
  QStringList arguments;

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";

  QString dockerexe  = "dilate-image"; // Dilation followed by Erosion

  arguments << dockerimage;
  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-iterations" << QString::number(iter);

  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  // Reset arguments
  arguments.clear();
  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";
  arguments << dockerimage;

  //Marching Cubes
  inputRelativePath  = outputRelativePath;
  outputRelativePath = mirtkhome.relativeFilePath("segmentation.vtk");
  dockerexe = "extract-surface";

  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-isovalue" << QString::number(th);
  arguments << "-blur" << QString::number(blur);
  arguments << "-ascii";
  arguments << "-verbose" << "3";
  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  // Reset arguments
  arguments.clear();
  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";
  arguments << dockerimage;

  //Smoothing
  inputRelativePath  = outputRelativePath;
  outputRelativePath = outputRelativePath+"";
  dockerexe = "smooth-surface";

  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-iterations" << QString::number(smth);
  arguments << "-verbose" << "3";
  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  //Return path to output mesh
  remove((dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.s.nii").toStdString().c_str());

  QString  outAbsolutepath = mirtkhome.absolutePath() +
      mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;
  return outAbsolutepath;
}
QString CemrgCommandLine::dockerSurf(QString dir, QString segPath, int iter, float th, int blur, int smth){
  MITK_INFO << "[ATTENTION] Attempting SURFACE CREATION using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif
  QDir mirtkhome(dir);

  QString inputRelativePath = mirtkhome.relativeFilePath(segPath);
  QString outputRelativePath = mirtkhome.relativeFilePath("segmentation.s.nii");

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";
  QStringList arguments;

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";

  QString dockerexe  = "close-image"; // Dilation followed by Erosion

  arguments << dockerimage;
  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-iterations" << QString::number(iter);
  arguments << "-verbose" << "3";

  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  // Reset arguments
  arguments.clear();
  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";
  arguments << dockerimage;

  //Marching Cubes
  inputRelativePath  = outputRelativePath;
  outputRelativePath = mirtkhome.relativeFilePath("segmentation.vtk");
  dockerexe = "extract-surface";

  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-isovalue" << QString::number(th);
  arguments << "-blur" << QString::number(blur);
  arguments << "-ascii";
  arguments << "-verbose" << "3";
  completion = false;
  process->start(docker, arguments);
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  // Reset arguments
  arguments.clear();
  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";
  arguments << dockerimage;

  //Smoothing
  inputRelativePath  = outputRelativePath;
  outputRelativePath = outputRelativePath+"";
  dockerexe = "smooth-surface";

  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-iterations" << QString::number(smth);
  arguments << "-verbose" << "3";
  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  //Return path to output mesh
  remove((dir + mitk::IOUtil::GetDirectorySeparator() + "segmentation.s.nii").toStdString().c_str());

  QString  outAbsolutepath = mirtkhome.absolutePath() +
      mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;
  return outAbsolutepath;
}

bool CemrgCommandLine::dockerResamplingOmNifti(QString niifullpath, QString outputtniifullpath, int isovalue) {
  // /resample-image niifullpath outputtniifullpath -isotropic 0.5 -interp CSpline -verbose 3
  MITK_INFO << "[ATTENTION] Attempting RESAMPLING IMAGE using Docker.";
  QString aPath = "";
  #if defined(__APPLE__)
      aPath = "/usr/local/bin/";
  #endif
  QFileInfo inputnii(niifullpath);
  QDir mirtkhome(inputnii.absolutePath());

  QString inputRelativePath = mirtkhome.relativeFilePath(niifullpath);
  QString outputRelativePath = mirtkhome.relativeFilePath(outputtniifullpath);

  process->setWorkingDirectory(mirtkhome.absolutePath());

  // Setup docker
  QString docker = aPath+"docker";
  QString dockerimage = "biomedia/mirtk:v1.1.0";
  QStringList arguments;

  arguments << "run";
  arguments << "--volume="+mirtkhome.absolutePath()+":/data";
  arguments << dockerimage;

  QString dockerexe  = "resample-image"; // Dilation followed by Erosion

  arguments << dockerexe;
  arguments << inputRelativePath;
  arguments << outputRelativePath;
  arguments << "-isotropic" << QString::number(isovalue);
  arguments << "-interp" << "CSpline";
  arguments << "-verbose" << "3";

  completion = false;
  process->start(docker, arguments);
  checkForStartedProcess();
  while (!completion) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
  mitk::ProgressBar::GetInstance()->Progress();

  bool successful = isOutputSuccessful(outputtniifullpath);
  if (!successful)
    MITK_WARN << "Docker unsuccessful. Check your configuration.";

  return successful;

  }

  // Tracking Utilities - Docker
  bool CemrgCommandLine::dockerTracking(QString dir, QString imgTimes, QString param) {
    MITK_INFO << "[ATTENTION] Attempting TRACKING (registration) using Docker";
    QString aPath = "";
    #if defined(__APPLE__)
        aPath = "/usr/local/bin/";
    #endif
    QDir mirtkhome(dir);

    QString inputRelativePath = mirtkhome.relativeFilePath(imgTimes);
    QString outputRelativePath = mirtkhome.relativeFilePath("tsffd.dof");

    process->setWorkingDirectory(mirtkhome.absolutePath());

    // Setup docker
    QString docker = aPath+"docker";
    QString dockerimage = "biomedia/mirtk:v1.1.0";
    QString dockerexe  = "register";
    QStringList arguments;

    arguments << "run";
    arguments << "--volume="+mirtkhome.absolutePath()+":/data";
    arguments << dockerimage;

    arguments << dockerexe;

    arguments << "-images" << inputRelativePath;
    if (!param.isEmpty()){
      QString paramRelativePath = mirtkhome.relativeFilePath(param);
      arguments << "-parin" << paramRelativePath;
    }
    arguments << "-dofout" << outputRelativePath;
    arguments << "-verbose" << "3";

    completion = false;

    MITK_INFO << printFullCommand(docker, arguments);

    process->start(docker, arguments);
    checkForStartedProcess();
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mitk::ProgressBar::GetInstance()->Progress();

    QString  outAbsolutepath = mirtkhome.absolutePath() +
        mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;

    bool successful = isOutputSuccessful(outAbsolutepath);
    MITK_WARN(!successful) << "Docker unsuccessful. Check your configuration.";

    return successful;
}

  bool CemrgCommandLine::dockerApplying(QString dir, QString inputMesh, double iniTime, QString dofin, int noFrames, int smooth) {
    MITK_INFO << "[ATTENTION] Attempting APPLYING (registration) using Docker";
    QString aPath = "";
    #if defined(__APPLE__)
        aPath = "/usr/local/bin/";
    #endif
    QDir mirtkhome(dir);

    QString inputRelativePath = mirtkhome.relativeFilePath(inputMesh);
    QString outputRelativePath_ = mirtkhome.relativeFilePath("transformed-");
    QString dofRelativePath = mirtkhome.relativeFilePath(dofin);

    process->setWorkingDirectory(mirtkhome.absolutePath());

    // Setup docker
    QString docker = aPath+"docker";
    QString dockerimage = "biomedia/mirtk:v1.1.0";
    QString dockerexe  = "transform-points";
    QStringList arguments;

    arguments << "run";
    arguments << "--volume="+mirtkhome.absolutePath()+":/data";
    arguments << dockerimage;
    arguments << dockerexe;

      int fctTime = 10;
      noFrames *= smooth;
      int suxs = 0;
      if (smooth == 2)
          fctTime = 5;
      else if (smooth == 5)
          fctTime = 2;

      QString  outAbsolutepath = mirtkhome.absolutePath() +
          mitk::IOUtil::GetDirectorySeparator() + outputRelativePath_;
      for (int i=0; i<noFrames; i++) {

          arguments.clear();
          arguments << "run";
          arguments << "--volume="+mirtkhome.absolutePath()+":/data";
          arguments << dockerimage;
          arguments << dockerexe;

          arguments << inputRelativePath;
          arguments << outputRelativePath_ + QString::number(i) + ".vtk";
          arguments << "-dofin" << dofRelativePath;
          arguments << "-ascii";
          arguments << "-St";
          arguments << QString::number(iniTime);
          arguments << "-verbose" << "3";

          completion = false;
          process->start(docker, arguments);
          checkForStartedProcess();
          while (!completion) {
              std::this_thread::sleep_for(std::chrono::seconds(1));
              QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
          }

          suxs += (isOutputSuccessful(outAbsolutepath + QString::number(i) + ".vtk")) ? 1 : 0;
          iniTime += fctTime;
          mitk::ProgressBar::GetInstance()->Progress();
    }

    bool successful = (suxs == noFrames);
    return successful;
  }

  bool CemrgCommandLine::dockerSimpleTranslation(QString dir, QString sourceMeshP, QString targetMeshP, QString outputPath){
    MITK_INFO << "[ATTENTION] Attempting INIT-DOF + TRANSFORM-POINTS using Docker.";
    QString aPath = "";
    #if defined(__APPLE__)
        aPath = "/usr/local/bin/";
    #endif
    QDir mirtkhome(dir);

    QString sourceRelativePath = mirtkhome.relativeFilePath(sourceMeshP);
    QString targetRelativePath = mirtkhome.relativeFilePath(targetMeshP);
    QString txRelativePath = mirtkhome.relativeFilePath(".init-tx.dof");
    QString outputRelativePath = mirtkhome.relativeFilePath(outputPath);

    process->setWorkingDirectory(mirtkhome.absolutePath());

    // Setup docker
    QString docker = aPath+"docker";
    QString dockerimage = "biomedia/mirtk:v1.1.0";
    QStringList arguments;

    arguments << "run";
    arguments << "--volume="+mirtkhome.absolutePath()+":/data";

    QString dockerexe  = "init-dof"; // simple translation

    arguments << dockerimage;
    arguments << dockerexe;
    arguments << txRelativePath;
    arguments << "-translations" << "-norotations" << "-noscaling" << "-noshearing";
    arguments << "-displacements";
    arguments << sourceRelativePath;
    arguments << targetRelativePath;
    arguments << "-verbose" << "3";

    MITK_INFO << printFullCommand(docker, arguments);

    completion = false;
    process->start(docker, arguments);
    checkForStartedProcess();
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mitk::ProgressBar::GetInstance()->Progress();

    // Reset arguments
    arguments.clear();
    arguments << "run";
    arguments << "--volume="+mirtkhome.absolutePath()+":/data";
    arguments << dockerimage;

    // Transformation
    dockerexe = "transform-points";

    arguments << dockerexe;
    arguments << sourceRelativePath;
    arguments << outputRelativePath;
    arguments << "-dofin" << txRelativePath;
    arguments << "-ascii";
    arguments << "-verbose" << "3";

    MITK_INFO << printFullCommand(docker, arguments);

    completion = false;
    process->start(docker, arguments);
    checkForStartedProcess();
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mitk::ProgressBar::GetInstance()->Progress();

    QString  outAbsolutepath = mirtkhome.absolutePath() +
        mitk::IOUtil::GetDirectorySeparator() + outputRelativePath;

    bool successful = isOutputSuccessful(outAbsolutepath);
    if (!successful)
      MITK_WARN << "Docker unsuccessful. Check your configuration.";

    return successful;

  }
  // Docker - meshtools3d
  QString CemrgCommandLine::dockerCreateCGALMesh(QString dir, QString fileName, QString templatePath){
    MITK_INFO << "[ATTENTION] Attempting CreateCGALMesh (Meshtools3D) using Docker.";
    QString aPath = "";
    #if defined(__APPLE__)
        aPath = "/usr/local/bin/";
    #endif
    QDir meshtools3dhome(dir);

    QString templateRelativePath = meshtools3dhome.relativeFilePath(templatePath);
    QString segRelativePath = meshtools3dhome.relativeFilePath(dir);
    // QString outnameRelativePath = fileName;
    QString outdirRelativePath = meshtools3dhome.relativeFilePath("CGALMeshDir");

    process->setWorkingDirectory(meshtools3dhome.absolutePath());

    // Setup docker
    QString docker = aPath+"docker";
    QString dockerimage = "alonsojasl/meshtools3d:v1.0";
    QStringList arguments;

    arguments << "run";
    arguments << "--volume="+meshtools3dhome.absolutePath()+":/data";
    arguments << dockerimage;

    arguments << "-f" << templateRelativePath;
    arguments << "-seg_dir" << segRelativePath;
    arguments << "-seg_name" << "converted.inr";
    arguments << "-out_dir" << outdirRelativePath;
    arguments << "-out_name" << fileName;

    bool debugvar = true;

    MITK_INFO(debugvar) << printFullCommand(docker, arguments);

    completion = false;
    process->start(docker, arguments);
    checkForStartedProcess();
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mitk::ProgressBar::GetInstance()->Progress();

    QString  outAbsolutepath = meshtools3dhome.absolutePath() +
        mitk::IOUtil::GetDirectorySeparator() + "CGALMeshDir" +
         mitk::IOUtil::GetDirectorySeparator() + fileName + ".vtk";

    bool successful = isOutputSuccessful(outAbsolutepath);
    MITK_WARN((!successful)) << "Docker unsuccessful. Check your configuration.";

    return outAbsolutepath;
  }

  //Docker - ML
  QString CemrgCommandLine::dockerCemrgNetPrediction(QString mra){
    MITK_INFO << "[CEMRGNET] Attempting prediction using Docker";
    QString aPath = "";
    #if defined(__APPLE__)
        aPath = "/usr/local/bin/";
    #endif
    QFileInfo finfo(mra);
    QDir cemrgnethome(finfo.absolutePath());

    QString inputfilepath = cemrgnethome.absolutePath() + mitk::IOUtil::GetDirectorySeparator() + "test.nii";
    QString tempfilepath = cemrgnethome.absolutePath() + mitk::IOUtil::GetDirectorySeparator() + "output.nii";
    QString outputfilepath = cemrgnethome.absolutePath() + mitk::IOUtil::GetDirectorySeparator() + "LA-cemrgnet.nii";

    bool test;
    if(QFile::exists(inputfilepath)){
      MITK_INFO << "[CEMRGNET] File test.nii exists.";
      test = true;
    }
    else{
      MITK_INFO << "[CEMRGNET] Copying file to test.nii";
      test = QFile::copy(finfo.absoluteFilePath(), inputfilepath);
    }

    QString res;
    if(test){
      QString inputRelativePath = cemrgnethome.relativeFilePath(inputfilepath);
      process->setWorkingDirectory(cemrgnethome.absolutePath());

      // Setup docker
      QString docker = aPath+"docker";
      QString dockerimage = "orodrazeghi/cemrgnet";
      // QString dockerexe  = "cemrgnet";
      QStringList arguments;

      arguments << "run";
      arguments << "--rm";
      arguments << "--volume="+cemrgnethome.absolutePath()+":/data";
      arguments << dockerimage;
      // arguments << dockerexe;

      bool debugvar=true;
      if(debugvar){
        MITK_INFO << "[DEBUG] Input path:";
        MITK_INFO << inputfilepath.toStdString();

        MITK_INFO << "[DEBUG] Docker command to run:";
        MITK_INFO << printFullCommand(docker, arguments);
      }

      completion = false;
      process->start(docker, arguments);
      checkForStartedProcess();
      while (!completion) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
      mitk::ProgressBar::GetInstance()->Progress();

      bool test2 = QFile::rename(tempfilepath, outputfilepath);
      if(test2){
        MITK_INFO << "[CEMRGNET] Prediction and output creation - successful.";
        res = outputfilepath;
      } else if(isOutputSuccessful(tempfilepath)){
        MITK_INFO << "[CEMRGNET] Prediction - successful.";
        res = tempfilepath;
      } else {
        MITK_WARN << "[CEMRGNET] Problem with prediction.";
        res = "";
      }
    } else {
      MITK_WARN << "Copying input file to 'test.nii' was unsuccessful.";
      res = "";
    }

    return res;
  }

  // Helper functions
  bool CemrgCommandLine::isOutputSuccessful(QString outputfullpath){
    MITK_INFO << "[ATTENTION] Checking for successful output on path:";
    MITK_INFO << outputfullpath.toStdString();

    QFileInfo finfo(outputfullpath);
    bool res = finfo.exists();

    MITK_INFO << (res ? "Successful output" : "Output file not found.");

    return res;
  }

  void CemrgCommandLine::ExecuteTouch(QString filepath){
    QStringList arguments;
    arguments << filepath;

    completion = false;
    process->start("touch", arguments);
    while (!completion) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mitk::ProgressBar::GetInstance()->Progress();
  }

  std::string CemrgCommandLine::printFullCommand(QString command, QStringList arguments){
    QString teststr = "";
    for (int ix=0; ix < arguments.size(); ix++){
      teststr += arguments.at(ix) + " ";
    }
    bool debugging = true;
    if(debugging){
      QString prodPath = QString::fromStdString(mitk::IOUtil::GetProgramPath());
      MITK_INFO << mitk::IOUtil::GetProgramPath();
      ofstream prodFile1;
      prodFile1.open((prodPath + "dockerDebug.txt").toStdString(), ofstream::out | ofstream::app);
      prodFile1 << (command + " " + teststr).toStdString() << "\n";
      prodFile1.close();
    }

    return (command + " " + teststr).toStdString();
  }

  void CemrgCommandLine::checkForStartedProcess(){
    bool debugvar = false;
    if(debugvar){
      QStringList errinfo = QProcess::systemEnvironment();
      QString teststr = "";
      for (int ix=0; ix < errinfo.size(); ix++){
        teststr += errinfo.at(ix) + " ";
      }
      MITK_INFO << "SYSTEM ENVIRONMENT:";
      MITK_INFO << teststr.toStdString();
    }

    if(process->waitForStarted()){
      MITK_INFO << "Starting process";
    }
    else{
      completion=true;
      MITK_WARN << "[ATTENTION] Process error!";
      MITK_INFO << "STATE:";
      MITK_INFO << process->state();
      MITK_INFO << "ERROR:";
      MITK_INFO << process->error();
    }

  }
/***************************************************************************
 ************************** COMMANDLINE UTILITIES **************************
 ***************************************************************************/

void CemrgCommandLine::UpdateStdText() {

    QByteArray data = process->readAllStandardOutput();
    panel->append(QString(data));
}

void CemrgCommandLine::UpdateErrText() {

    QByteArray data = process->readAllStandardError();
    panel->append(QString(data));
}

void CemrgCommandLine::FinishedAlert() {

    completion = true;
    QString data = process->program() + " Completed!";
    panel->append(data);
}

QDialog* CemrgCommandLine::GetDialog() {

    return dial;
}
