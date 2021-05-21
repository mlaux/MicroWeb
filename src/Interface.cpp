//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "Platform.h"
#include "Interface.h"
#include "App.h"
#include "KeyCodes.h"

AppInterface::AppInterface(App& inApp) : app(inApp)
{
	GenerateWidgets();

	hoverWidget = NULL;
	oldMouseX = -1;
	oldMouseY = -1;
	activeWidget = &addressBar;
}

void AppInterface::DrawInterfaceWidgets()
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		app.renderer.RenderWidget(appInterfaceWidgets[n]);
	}

	// Page top line divider
	Platform::video->HLine(0, Platform::video->windowY - 1, Platform::video->screenWidth);
}

void AppInterface::Update()
{
	int buttons, mouseX, mouseY;
	Platform::input->GetMouseStatus(buttons, mouseX, mouseY);

	Widget* oldHoverWidget = hoverWidget;

	if (hoverWidget && !IsOverWidget(hoverWidget, mouseX, mouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}
	else if (!hoverWidget && (mouseX != oldMouseX || mouseY != oldMouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}

	oldMouseX = mouseX;
	oldMouseY = mouseY;

	if (hoverWidget != oldHoverWidget)
	{
		if (hoverWidget && hoverWidget->GetLinkURL())
		{
			app.renderer.SetStatus(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
			Platform::input->SetMouseCursor(MouseCursor::Hand);
		}
		else if (hoverWidget && hoverWidget->type == Widget::TextField)
		{
			Platform::input->SetMouseCursor(MouseCursor::TextSelect);
		}
		else
		{
			app.renderer.SetStatus("");
			Platform::input->SetMouseCursor(MouseCursor::Pointer);
		}
	}
	

	InputButtonCode keyPress;
	int scrollDelta = 0;

	while (keyPress = Platform::input->GetKeyPress())
	{
		if (activeWidget)
		{
			if (HandleActiveWidget(keyPress))
			{
				continue;
			}
		}

		switch (keyPress)
		{
		case KEYCODE_MOUSE_LEFT:
			HandleClick();
			break;
		case KEYCODE_ESCAPE:
			app.Close();
			break;
		case KEYCODE_ARROW_UP:
			scrollDelta -= 8;
			break;
		case KEYCODE_ARROW_DOWN:
			scrollDelta += 8;
			break;
		case KEYCODE_PAGE_UP:
			scrollDelta -= (Platform::video->windowHeight - 24);
			break;
		case KEYCODE_PAGE_DOWN:
			scrollDelta += (Platform::video->windowHeight - 24);
			break;
		case KEYCODE_HOME:
			app.renderer.Scroll(-app.page.GetPageHeight());
			break;
		case KEYCODE_END:
			app.renderer.Scroll(app.page.GetPageHeight());
			break;
		case 'n':
			app.OpenURL("http://68k.news");
			break;
		//case 'b':
		case KEYCODE_BACKSPACE:
			app.PreviousPage();
			break;
		//case 'f':
		//	app.NextPage();
		//	break;
		case KEYCODE_CTRL_I:
		{
			Platform::input->HideMouse();
			Platform::video->InvertScreen();
			Platform::input->ShowMouse();
		}
			break;
		case KEYCODE_CTRL_L:
			activeWidget = &addressBar;
			break;
		default:
			//printf("%d\n", keyPress);
			break;
		}
	}

	if (scrollDelta)
	{
		app.renderer.Scroll(scrollDelta);
	}
}

void AppInterface::GenerateWidgets()
{
	appInterfaceWidgets[0] = &addressBar;
	appInterfaceWidgets[1] = &scrollBar;
	appInterfaceWidgets[2] = &backButton;
	appInterfaceWidgets[3] = &forwardButton;

	addressBar.type = Widget::TextField;
	addressBar.textField = &addressBarData;
	addressBarData.buffer = addressBarURL.url;
	addressBarData.bufferLength = MAX_URL_LENGTH - 1;
	addressBar.style = WidgetStyle(FontStyle::Regular);

	scrollBar.type = Widget::ScrollBar;

	backButtonData.text = (char*) "<";
	backButtonData.form = NULL;
	forwardButtonData.text = (char*) ">";
	forwardButtonData.form = NULL;

	backButton.type = Widget::Button;
	backButton.button = &backButtonData;
	backButton.style = WidgetStyle(FontStyle::Bold);
	forwardButton.type = Widget::Button;
	forwardButton.button = &forwardButtonData;
	forwardButton.style = WidgetStyle(FontStyle::Bold);

	Platform::video->ArrangeAppInterfaceWidgets(*this);
}

Widget* AppInterface::PickWidget(int x, int y)
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		Widget* widget = appInterfaceWidgets[n];
		if (x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height)
		{
			return widget;
		}
	}

	return app.renderer.PickPageWidget(x, y);
}

bool AppInterface::IsOverWidget(Widget* widget, int x, int y)
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		if (widget == appInterfaceWidgets[n])
		{
			// This is an interface widget
			return (x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height);
		}
	}

	return app.renderer.IsOverPageWidget(widget, x, y);
}

void AppInterface::HandleClick()
{
	if (activeWidget)
	{
		activeWidget = NULL;
	}

	if (hoverWidget)
	{
		if (hoverWidget->GetLinkURL())
		{
			app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
		}
		else if (hoverWidget == &backButton)
		{
			app.PreviousPage();
		}
		else if (hoverWidget == &forwardButton)
		{
			app.NextPage();
		}
		else if (hoverWidget->type == Widget::TextField)
		{
			activeWidget = hoverWidget;
		}
		else if (hoverWidget->type == Widget::Button)
		{
			if (hoverWidget->button->form)
			{
				SubmitForm(hoverWidget->button->form);
			}
		}
	}
}

bool AppInterface::HandleActiveWidget(InputButtonCode keyPress)
{
	switch (activeWidget->type)
	{
	case Widget::TextField:
		{
			if(activeWidget->textField && activeWidget->textField->buffer)
			{
				int textLength = strlen(activeWidget->textField->buffer);

				if (keyPress >= 32 && keyPress < 128)
				{
					if (textLength < activeWidget->textField->bufferLength)
					{
						if (activeWidget == &addressBar)
							app.renderer.RenderWidget(activeWidget);
						else
							app.renderer.RenderPageWidget(activeWidget);
						activeWidget->textField->buffer[textLength++] = (char)(keyPress);
						activeWidget->textField->buffer[textLength++] = '\0';
						if (activeWidget == &addressBar)
							app.renderer.RenderWidget(activeWidget);
						else
							app.renderer.RenderPageWidget(activeWidget);
					}
					return true;
				}
				else if (keyPress == KEYCODE_BACKSPACE)
				{
					if (textLength > 0)
					{
						if (activeWidget == &addressBar)
							app.renderer.RenderWidget(activeWidget);
						else
							app.renderer.RenderPageWidget(activeWidget);
						activeWidget->textField->buffer[textLength - 1] = '\0';
						if (activeWidget == &addressBar)
							app.renderer.RenderWidget(activeWidget);
						else
							app.renderer.RenderPageWidget(activeWidget);
					}
					return true;
				}
				else if (keyPress == KEYCODE_ENTER)
				{
					if (activeWidget == &addressBar)
					{
						app.OpenURL(addressBarURL.url);
					}
					else if (activeWidget->textField->form)
					{
						SubmitForm(activeWidget->textField->form);
					}
				}
			}
		}
		break;
	}

	return false;
}

void AppInterface::SubmitForm(WidgetFormData* form)
{
	if (form->method == WidgetFormData::Get)
	{
		char* address = addressBarURL.url;
		strcpy(address, form->action);
		int numParams = 0;

		for (int n = 0; n < app.page.numFinishedWidgets; n++)
		{
			Widget& widget = app.page.widgets[n];

			switch (widget.type)
			{
			case Widget::TextField:
				if (widget.textField->form == form && widget.textField->name && widget.textField->buffer)
				{
					if (numParams == 0)
					{
						strcat(address, "?");
					}
					else
					{
						strcat(address, "&");
					}
					strcat(address, widget.textField->name);
					strcat(address, "=");
					strcat(address, widget.textField->buffer);
					numParams++;
				}
				break;
			}
		}

		// Replace any spaces with +
		for (char* p = address; *p; p++)
		{
			if (*p == ' ')
			{
				*p = '+';
			}
		}

		app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, address).url);
	}
}

void AppInterface::UpdateAddressBar(const URL& url)
{
	addressBarURL = url;
	Platform::video->ClearRect(addressBar.x + 1, addressBar.y + 1, addressBar.width - 2, addressBar.height - 2);
	app.renderer.RenderWidget(&addressBar);
}
