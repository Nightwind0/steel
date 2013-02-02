/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "File.h"

namespace Steel { 

File::File() {

}

File::~File() {

}

bool File::open( const char* path )
{
	m_in.open(path);
	return m_in.good();
}

bool File::eof()const
{
	return m_in.eof();
}

int  File::read(char *buffer, int bytes)
{
	m_in.read(buffer,bytes);
	return m_in.gcount();
}

void File::close()
{
	m_in.close();
}

IFile* FileProvider::create()
{
	return new File();
}



}