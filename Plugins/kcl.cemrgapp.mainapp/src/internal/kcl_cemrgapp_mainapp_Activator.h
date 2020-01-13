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


#ifndef kcl_cemrgapp_mainapp_Activator_h
#define kcl_cemrgapp_mainapp_Activator_h

#include <berryAbstractUICTKPlugin.h>
#include <QString>

namespace mitk {

class kcl_cemrgapp_mainapp_Activator : public berry::AbstractUICTKPlugin {

    Q_OBJECT
    Q_PLUGIN_METADATA(IID "kcl_cemrgapp_mainapp")
    Q_INTERFACES(ctkPluginActivator)

public:

    kcl_cemrgapp_mainapp_Activator();
    ~kcl_cemrgapp_mainapp_Activator() override;

    static kcl_cemrgapp_mainapp_Activator* GetDefault();
    ctkPluginContext* GetPluginContext() const;
    void start(ctkPluginContext*) override;
    QString GetQtHelpCollectionFile() const;

private:

    static kcl_cemrgapp_mainapp_Activator* inst;
    ctkPluginContext* context;
    mutable QString helpCollectionFile;
};

}

#endif /* MITK_CEMRGAPP_PLUGIN_ACTIVATOR_H_ */
// #include <ctkPluginActivator.h>
//
// namespace mitk
// {
//   class kcl_cemrgapp_mainapp_Activator : public QObject, public ctkPluginActivator
//   {
//     Q_OBJECT
//     Q_PLUGIN_METADATA(IID "kcl_cemrgapp_mainapp")
//     Q_INTERFACES(ctkPluginActivator)
//
//   public:
//     void start(ctkPluginContext *context);
//     void stop(ctkPluginContext *context);
//
//   }; // kcl_cemrgapp_mainapp_Activator
// }
//
// #endif // kcl_cemrgapp_mainapp_Activator_h
