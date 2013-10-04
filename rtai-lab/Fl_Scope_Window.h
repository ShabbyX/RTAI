/*
COPYRIGHT (C) 2003  Lorenzo Dozio <dozio@aero.polimi.it>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/

#ifndef _FL_SCOPE_WINDOW_H_
#define _FL_SCOPE_WINDOW_H_

#include <Fl_Scope.h>
#include <efltk/Fl_MDI_Window.h>

class Fl_Scope_Window
{
	public:
		Fl_Scope_Window(int x, int y, int w, int h, Fl_MDI_Viewport *s, const char *name, int n_traces, float dt);
		Fl_Scope *Plot;
		int x();
		int y();
		int w();
		int h();
		void resize(int, int, int, int);
		void show();
		void hide();
		int is_visible();
	private:
		Fl_MDI_Window *SWin;
};

#endif
