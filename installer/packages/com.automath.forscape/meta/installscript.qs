function Component()
{
    // default constructor
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if ( installer.value("os") == "win" )
    {
        var target_dir = installer.value("TargetDir").split("/").join("\\");
        var forscape_path = target_dir + "\\Forscape.exe";

        component.addOperation(
            "RegisterFileType",
            "π",
            forscape_path + " \"%1\"",
            "Forscape script",
            "application/forscape",
            "\"" + forscape_path + "\", 1");
    }
}

