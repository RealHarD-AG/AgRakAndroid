/*
	Interactive SA:MP dialog UI (Windows HWND / Linux zenity or console).
	Dialog text from the server is CP1251; the UI layer converts for display.
*/

#ifndef DIALOG_UI_H
#define DIALOG_UI_H

void DialogUI_Show();
void DialogUI_Hide();

// Called from the console input thread. If a console dialog is waiting for a
// line, consumes it (CP1251) and returns true so RunCommand is skipped.
bool DialogUI_TryConsumeConsoleLine(char *line);

#endif // DIALOG_UI_H
