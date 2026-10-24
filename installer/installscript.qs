// Component install script for the Qt Installer Framework

function Component()
{
    // Page with the shortcut options, shown before the Start menu page
    installer.addWizardPage(component, "ShortcutsPage", QInstaller.StartMenuSelection);

    // Translations are only available once the component is loaded
    component.loaded.connect(this, Component.prototype.loaded);
}

Component.prototype.loaded = function()
{
    component.setValue("Description",
        qsTranslate("installscript", "Saves and restores window geometries in profiles."));

    var page = component.userInterface("ShortcutsPage");
    if (page != null)
        page.StartMenuCheckBox.toggled.connect(this, Component.prototype.startMenuToggled);
}

// The Start menu folder is only asked for when a Start menu shortcut is wanted
Component.prototype.startMenuToggled = function(checked)
{
    installer.setDefaultPageVisible(QInstaller.StartMenuSelection, checked);
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType !== "windows")
        return;

    var executable = "@TargetDir@/bin/ApplicationGeometryRecovery.exe";
    var page = component.userInterface("ShortcutsPage");

    if (page == null || page.StartMenuCheckBox.checked) {
        component.addOperation("CreateShortcut", executable,
                               "@StartMenuDir@/Application Geometry Recovery.lnk",
                               "workingDirectory=@TargetDir@/bin");
    }

    // Autostart for the user who installs, removed again on uninstall. A Run
    // entry pointing to the installed program is used instead of a shortcut in
    // the Startup folder, which virus scanners report as a dropper
    // (Drop.Win64.Startup).
    if (page != null && page.AutostartCheckBox.checked) {
        var command = "\"" + installer.toNativeSeparators(
            installer.value("TargetDir") + "/bin/ApplicationGeometryRecovery.exe") + "\"";
        component.addOperation("GlobalConfig",
                               "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                               "ApplicationGeometryRecovery", command);
    }
}
