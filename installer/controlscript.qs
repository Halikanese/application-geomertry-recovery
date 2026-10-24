// Installer control script for the Qt Installer Framework
//
// The welcome page is shown before the component and its .qm files are
// loaded, so qsTranslate() cannot translate here yet. CMake therefore embeds
// the translations of the context "installscript" from i18n/*.ts below.

var translations = {};
//@IFW_TRANSLATIONS@

var welcomeText = null;

// Returns the translation for the installer language, or the English text
function translate(text)
{
    var language = String(installer.value("UILanguage")).substring(0, 2).toLowerCase();
    var table = translations[language];
    if (table && table[text])
        return table[text];
    return text;
}

function Controller()
{
    // There is only one component, which is always installed
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
}

// Shows the program description below the greeting on the welcome page
Controller.prototype.IntroductionPageCallback = function()
{
    var widget = gui.currentPageWidget();
    if (widget == null || widget.MessageLabel == null)
        return;

    // The page can be entered several times (Back button)
    if (welcomeText == null)
        welcomeText = widget.MessageLabel.text;

    widget.MessageLabel.setText(welcomeText + "\n\n"
        + translate(qsTranslate("installscript",
                                "Application Geometry Recovery saves the positions and sizes of all open "
                                + "windows in named profiles, for example one for the local monitors and "
                                + "one for a Remote Desktop connection. From the system tray, a profile "
                                + "restores the window layout with a single click.")));
}

// Offers to start the application right away, checked by default
Controller.prototype.FinishedPageCallback = function()
{
    var widget = gui.currentPageWidget();
    if (widget == null || widget.RunItCheckBox == null)
        return;

    widget.RunItCheckBox.setText(translate(qsTranslate("installscript",
                                                       "Run Application Geometry Recovery now")));
    widget.RunItCheckBox.setChecked(true);
}
