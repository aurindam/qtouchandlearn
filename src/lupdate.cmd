for %%i in (data\*.ts) do lupdate.exe -no-obsolete -locations none qml\qmlparalax\database.js -ts %%i